#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
thread_local bool guardAllocations = false;
thread_local std::size_t allocations = 0;
void *operator new(std::size_t size)
{
    if (guardAllocations)
        ++allocations;
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}
void check(bool x, const char *msg)
{
    if (!x)
        throw std::runtime_error(msg);
}
int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    try
    {
        LiminalProcessor p;
        p.prepareToPlay(48000, 512);
        p.loadFactory(9);
        p.captureScene(false);
        auto values = p.settings();
        values[liminal::unreal] = .9f;
        p.loadSettings(values);
        p.captureScene(true);
        juce::MemoryBlock state;
        p.getStateInformation(state);
        LiminalProcessor q;
        q.prepareToPlay(48000, 512);
        q.setStateInformation(state.getData(), int(state.getSize()));
        for (std::size_t i = 0; i < liminal::count; ++i)
            check(std::abs(p.settings()[i] - q.settings()[i]) < .001f, "parameter roundtrip");
        check(p.presetName() == q.presetName(), "metadata roundtrip");
        juce::MemoryBlock restored;
        q.getStateInformation(restored);
        auto originalXml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        auto restoredXml =
            juce::AudioProcessor::getXmlFromBinary(restored.getData(), int(restored.getSize()));
        auto originalTree = juce::ValueTree::fromXml(*originalXml),
             restoredTree = juce::ValueTree::fromXml(*restoredXml);
        for (const char *slot : {"SceneA", "SceneB"})
            for (const auto &spec : liminal::specs)
                check(std::abs(double(originalTree.getChildWithName(slot).getProperty(spec.id)) -
                               double(restoredTree.getChildWithName(slot).getProperty(spec.id))) < 1e-5,
                      "scene slots roundtrip");
        check(bool(originalTree.getProperty("scenesEnabled")) ==
                  bool(restoredTree.getProperty("scenesEnabled")),
              "scene enable roundtrip");
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        auto tree = juce::ValueTree::fromXml(*xml);
        check(std::abs(float(tree.getChildWithName("SceneA").getProperty("unreal")) - .18f) < .001f,
              "A survives editing B");
        check(std::abs(float(tree.getChildWithName("SceneB").getProperty("unreal")) - .9f) < .001f,
              "B capture");
        tree.getChild(0).setProperty("value", 999999, nullptr);
        auto malformed = tree.createXml();
        juce::MemoryBlock invalid;
        juce::AudioProcessor::copyXmlToBinary(*malformed, invalid);
        auto original = q.settings();
        q.setStateInformation(invalid.getData(), int(invalid.getSize()));
        check(q.settings() == original, "invalid range rejected atomically");
        auto legacy = originalTree.createCopy();
        legacy.setProperty("version", 1, nullptr);
        for (int index = legacy.getNumChildren() - 1; index >= 0; --index)
        {
            auto child = legacy.getChild(index);
            if (child.hasType("PARAM"))
            {
                for (std::size_t i = liminal::pianoOn; i < liminal::count; ++i)
                    if (child.getProperty("id").toString() == liminal::specs[i].id)
                    {
                        legacy.removeChild(index, nullptr);
                        break;
                    }
            }
            else if (child.hasType("SceneA") || child.hasType("SceneB"))
            {
                for (std::size_t i = liminal::pianoOn; i < liminal::count; ++i)
                    child.removeProperty(liminal::specs[i].id, nullptr);
            }
        }
        auto legacyXml = legacy.createXml();
        juce::MemoryBlock legacyData;
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyData);
        q.setStateInformation(legacyData.getData(), int(legacyData.getSize()));
        for (std::size_t i = liminal::pianoOn; i < liminal::count; ++i)
            check(std::abs(q.settings()[i] - liminal::specs[i].initial) < 1e-5,
                  "legacy instrument defaults migration");
        auto version2 = originalTree.createCopy();
        version2.setProperty("version", 2, nullptr);
        for (int index = version2.getNumChildren() - 1; index >= 0; --index)
        {
            auto child = version2.getChild(index);
            if (child.hasType("PARAM"))
            {
                auto id = child.getProperty("id").toString();
                if (id == "instrumentBusLevel" || id == "ambienceBusLevel")
                    version2.removeChild(index, nullptr);
            }
            else
            {
                child.removeProperty("instrumentBusLevel", nullptr);
                child.removeProperty("ambienceBusLevel", nullptr);
            }
        }
        auto v2xml = version2.createXml();
        juce::MemoryBlock v2data;
        juce::AudioProcessor::copyXmlToBinary(*v2xml, v2data);
        q.setStateInformation(v2data.getData(), int(v2data.getSize()));
        check(std::abs(q.settings()[liminal::instrumentBusLevel]) < 1e-5 &&
                  std::abs(q.settings()[liminal::ambienceBusLevel] + 12) < 1e-5,
              "version 2 bus gain defaults migration");
        auto v3 = originalTree.createCopy();
        v3.setProperty("version", 3, nullptr);
        for (int index = v3.getNumChildren() - 1; index >= 0; --index)
        {
            auto child = v3.getChild(index);
            if (child.hasType("PARAM"))
            {
                auto id = child.getProperty("id").toString();
                if (id == "environmentModel" || id == "environmentInfluence")
                    v3.removeChild(index, nullptr);
            }
            else
            {
                child.removeProperty("environmentModel", nullptr);
                child.removeProperty("environmentInfluence", nullptr);
            }
        }
        auto v3xml = v3.createXml();
        juce::MemoryBlock v3data;
        juce::AudioProcessor::copyXmlToBinary(*v3xml, v3data);
        q.setStateInformation(v3data.getData(), int(v3data.getSize()));
        check(q.settings()[liminal::environmentModel] == -1 &&
                  q.settings()[liminal::environmentInfluence] == 1,
              "version 3 legacy location migration");
        const char bad[]{'x', 'y', 'z'};
        q.setStateInformation(bad, 3);
        check(q.presetName() == p.presetName(), "invalid state rejected");
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("liminal-test", ".liminal");
        check(p.savePreset(file).wasOk(), "preset save");
        check(q.loadPreset(file).wasOk(), "preset load");
        file.deleteFile();
        for (int n = 0; n < 15; ++n)
        {
            p.loadFactory(n);
            juce::AudioBuffer<float> b(2, 512);
            juce::MidiBuffer midi;
            p.processBlock(b, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    check(std::isfinite(b.getSample(c, i)), "preset finite");
        }
        for (int channels : {1, 2})
        {
            auto layout = p.getBusesLayout();
            layout.outputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono()
                                                    : juce::AudioChannelSet::stereo());
            check(p.setBusesLayout(layout), "bus layout");
            p.prepareToPlay(44100, 128);
            auto v = liminal::defaults();
            v[liminal::mode] = 1;
            p.loadSettings(v);
            p.prepareToPlay(44100, 128);
            p.resetEvolution();
            juce::AudioBuffer<float> b(channels, 128);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, .8f), 64);
            p.processBlock(b, midi);
            check(b.getMagnitude(0, 64) == 0, "sample accurate MIDI silence before note");
            for (int j = 0; j < 20; ++j)
                p.processBlock(b, midi);
            check(b.getMagnitude(0, 128) > 0, "MIDI through processor");
        }
        p.loadFactory(0);
        p.loadEnvironment(0);
        p.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer events;
        events.ensureSize(2048);
        p.processBlock(audio, events);
        for (int i = 0; i < 100; ++i)
        {
            events.addEvent(juce::MidiMessage::noteOn(1, 36 + i % 48, .7f), 0);
            events.addEvent(juce::MidiMessage::noteOff(1, 36 + i % 48), 256);
            const unsigned char sysex[]{0xf0, 0x01, 0x02, 0x03, 0xf7};
            events.addEvent(sysex, 5, 128);
            guardAllocations = true;
            p.processBlock(audio, events);
            guardAllocations = false;
        }
        check(allocations == 0, "C++ allocation in audio callback");
        std::cout << "100 MIDI/SysEx audio callbacks: zero guarded C++ allocations\n";
        std::cout
            << "State, scenes, 15 presets, invalid input, mono/stereo, and sample-accurate MIDI passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
