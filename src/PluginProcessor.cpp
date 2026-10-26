#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>
#include <cerrno>
using namespace liminal;
namespace
{
bool parseValue(const juce::var &input, const Spec &spec, float &result)
{
    auto text = input.toString().trim();
    if (text.isEmpty())
        return false;
    auto *begin = text.toRawUTF8();
    char *end = nullptr;
    errno = 0;
    double v = std::strtod(begin, &end);
    if (end == begin || *end != 0 || errno == ERANGE || !std::isfinite(v) || v < spec.min || v > spec.max ||
        (spec.discrete && std::round(v) != v))
        return false;
    result = float(v);
    return true;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout LiminalProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto &s : specs)
    {
        if (s.discrete && s.max == 1)
            l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{s.id, 1}, s.name,
                                                             s.initial > .5f));
        else if (s.discrete)
            l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{s.id, 1}, s.name, int(s.min),
                                                            int(s.max), int(s.initial)));
        else
        {
            juce::NormalisableRange<float> range(s.min, s.max);
            if (s.max > 100 && s.min > 0)
                range.setSkewForCentre(std::sqrt(s.min * s.max));
            l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{s.id, 1}, s.name, range,
                                                              s.initial));
        }
    }
    return l;
}
LiminalProcessor::LiminalProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "LIMINAL", layout())
{
    auto d = defaults();
    for (std::size_t i = 0; i < count; ++i)
    {
        raw[i] = state.getRawParameterValue(specs[i].id);
        sceneA[i].store(d[i]);
        sceneB[i].store(d[i]);
    }
    loadEnvironment(0);
}
Settings LiminalProcessor::settings() const
{
    Settings p{};
    for (std::size_t i = 0; i < count; ++i)
        p[i] = raw[i]->load();
    return p;
}
void LiminalProcessor::prepareToPlay(double sr, int)
{
    engine.prepare(sr, settings());
    instruments.prepare(sr, settings());
}
bool LiminalProcessor::isBusesLayoutSupported(const BusesLayout &b) const
{
    return b.getMainInputChannelSet().isDisabled() &&
           (b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() ||
            b.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}
void LiminalProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    juce::ScopedNoDenormals noDenormals;
    auto p = settings();
    if (scenesEnabled.load(std::memory_order_acquire))
    {
        // Reduced morph: continuous environmental parameters; global/performance,
        // discrete choices, seed, output safety and stochastic state remain live.
        for (std::size_t i = electricalLevel; i <= eventDensity; ++i)
            if (!specs[i].discrete)
                p[i] = sceneA[i].load() + (sceneB[i].load() - sceneA[i].load()) * p[morph];
    }
    engine.setSettings(p);
    instruments.setSettings(p);
    if (resetRequested.exchange(false))
        engine.reset(std::uint32_t(p[seed]));
    bool play = true;
    double tempo = 120;
    if (auto *ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            play = pos->getIsPlaying();
            if (auto b = pos->getBpm())
                tempo = *b;
        }
    engine.setTransport(play, tempo);
    auto it = midi.cbegin();
    float peakL = 0, peakR = 0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        while (it != midi.cend() && (*it).samplePosition <= i)
        {
            if ((*it).numBytes > 3)
            {
                ++it;
                continue;
            }
            auto msg = (*it).getMessage();
            if (msg.isNoteOn())
                instruments.noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                instruments.noteOff(msg.getChannel(), msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                instruments.allNotesOff(msg.getChannel(), msg.isAllSoundOff());
            else if (msg.isController() && msg.getControllerNumber() == 64)
                instruments.sustain(msg.getChannel(), msg.getControllerValue() >= 64);
            else if (msg.isPitchWheel())
                instruments.bend(msg.getChannel(), msg.getPitchWheelValue());
            ++it;
        }
        auto out = engine.tick(instruments.tick());
        if (buffer.getNumChannels() == 1)
            buffer.setSample(0, i, (out.l + out.r) * .5f);
        else
        {
            buffer.setSample(0, i, out.l);
            buffer.setSample(1, i, out.r);
        }
        peakL = std::max(peakL, std::abs(out.l));
        peakR = std::max(peakR, std::abs(out.r));
    }
    midi.clear();
    meterL.store(peakL);
    meterR.store(peakR);
}
void LiminalProcessor::loadSettings(const Settings &input)
{
    auto p = sanitise(input);
    scenesEnabled.store(false);
    for (std::size_t i = 0; i < count; ++i)
        if (auto *parameter = state.getParameter(specs[i].id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(p[i]));
            parameter->endChangeGesture();
        }
}
void LiminalProcessor::loadEnvironment(int index)
{
    index = std::clamp(index, 0, 9);
    loadSettings(environmentSettings(index, settings()));
    {
        const juce::ScopedLock lock(metadataLock);
        currentPreset = environments[index].name;
    }
}
void LiminalProcessor::loadRecommendedEnvironment(int index)
{
    index = std::clamp(index, 0, 9);
    loadSettings(recommendedEnvironmentSettings(index, settings()));
    const juce::ScopedLock lock(metadataLock);
    currentPreset = juce::String(environments[index].name) + " / instruments";
}
void LiminalProcessor::loadFactory(int i)
{
    auto &f = factoryPresets()[std::size_t(std::clamp(i, 0, 14))];
    loadSettings(f.values);
    {
        const juce::ScopedLock lock(metadataLock);
        currentPreset = f.name;
    }
    resetEvolution();
}
void LiminalProcessor::captureScene(bool b)
{
    auto p = settings();
    if (!haveScene.exchange(true))
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            sceneA[i].store(p[i]);
            sceneB[i].store(p[i]);
        }
    }
    auto &slot = b ? sceneB : sceneA;
    for (std::size_t i = 0; i < count; ++i)
        slot[i].store(p[i]);
    scenesEnabled.store(true, std::memory_order_release);
}
juce::String LiminalProcessor::presetName() const
{
    const juce::ScopedLock lock(metadataLock);
    return currentPreset;
}
void LiminalProcessor::getStateInformation(juce::MemoryBlock &dest)
{
    auto tree = state.copyState();
    tree.setProperty("version", 4, nullptr);
    tree.setProperty("presetName", presetName(), nullptr);
    tree.setProperty("scenesEnabled", scenesEnabled.load(), nullptr);
    for (int slot = 0; slot < 2; ++slot)
    {
        juce::ValueTree child(slot ? "SceneB" : "SceneA");
        auto &data = slot ? sceneB : sceneA;
        for (std::size_t i = 0; i < count; ++i)
            child.setProperty(specs[i].id, data[i].load(), nullptr);
        tree.appendChild(child, nullptr);
    }
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, dest);
}
void LiminalProcessor::setStateInformation(const void *data, int size)
{
    restoreState(data, size);
}
bool LiminalProcessor::restoreState(const void *data, int size)
{
    if (size < 8 || size > 1024 * 1024)
        return false;
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("LIMINAL"))
        return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    int version = int(tree.getProperty("version", 0));
    if (version != 1 && version != 2 && version != 3 && version != 4)
        return false;
    // Validate the complete parameter payload before altering live state.
    auto validated = defaults();
    std::array<bool, count> seen{};
    for (auto child : tree)
        if (child.hasType("PARAM"))
        {
            if (!child.hasProperty("id") || !child.hasProperty("value"))
                return false;
            auto id = child.getProperty("id").toString();
            bool known = false;
            for (std::size_t i = 0; i < count; ++i)
                if (id == specs[i].id)
                {
                    if (seen[i] || !parseValue(child.getProperty("value"), specs[i], validated[i]))
                        return false;
                    seen[i] = true;
                    known = true;
                    break;
                }
            if (!known)
                return false;
        }
    if (std::find(seen.begin(),
                  version == 1
                      ? seen.begin() + pianoOn
                      : (version == 2 ? seen.begin() + instrumentBusLevel
                                      : (version == 3 ? seen.begin() + environmentModel : seen.end())),
                  false) !=
        (version == 1 ? seen.begin() + pianoOn
                      : (version == 2 ? seen.begin() + instrumentBusLevel
                                      : (version == 3 ? seen.begin() + environmentModel : seen.end()))))
        return false;
    auto a = tree.getChildWithName("SceneA"), b = tree.getChildWithName("SceneB");
    Settings av{}, bv{};
    bool enabled = bool(tree.getProperty("scenesEnabled", false));
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!parseValue(a.getProperty(specs[i].id, validated[i]), specs[i], av[i]) ||
            !parseValue(b.getProperty(specs[i].id, validated[i]), specs[i], bv[i]))
            return false;
    }
    loadSettings(validated);
    for (std::size_t i = 0; i < count; ++i)
    {
        sceneA[i].store(av[i]);
        sceneB[i].store(bv[i]);
    }
    scenesEnabled.store(enabled, std::memory_order_release);
    {
        const juce::ScopedLock lock(metadataLock);
        currentPreset = tree.getProperty("presetName", "User").toString().substring(0, 128);
    }
    haveScene.store(true);
    resetEvolution();
    return true;
}
juce::Result LiminalProcessor::savePreset(const juce::File &file)
{
    juce::MemoryBlock data;
    getStateInformation(data);
    juce::TemporaryFile temp(file);
    if (!temp.getFile().replaceWithData(data.getData(), data.getSize()) ||
        !temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not write preset");
    return juce::Result::ok();
}
juce::Result LiminalProcessor::loadPreset(const juce::File &file)
{
    if (file.getSize() > 1024 * 1024 || file.getSize() < 8)
        return juce::Result::fail("Invalid preset size");
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data))
        return juce::Result::fail("Cannot read preset");
    auto xml = getXmlFromBinary(data.getData(), int(data.getSize()));
    if (!xml || !xml->hasTagName("LIMINAL") ||
        (xml->getIntAttribute("version") != 1 && xml->getIntAttribute("version") != 2 &&
         xml->getIntAttribute("version") != 3 && xml->getIntAttribute("version") != 4))
        return juce::Result::fail("Unsupported preset");
    // State restoration performs full range and schema validation.
    if (!restoreState(data.getData(), int(data.getSize())))
        return juce::Result::fail("Invalid preset parameter or scene data");
    return juce::Result::ok();
}
juce::AudioProcessorEditor *LiminalProcessor::createEditor()
{
    return new LiminalEditor(*this);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new LiminalProcessor();
}
