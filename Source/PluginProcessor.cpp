#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace liminal {

LiminalProcessor::LiminalProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "LIMINAL", params::makeLayout()) {
    power = parameters.getRawParameterValue(params::power);
    master = parameters.getRawParameterValue(params::master);
    instrumentLevel = parameters.getRawParameterValue(params::instrument);
    ambienceLevel = parameters.getRawParameterValue(params::ambience);
    space = parameters.getRawParameterValue(params::space);
}

bool LiminalProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LiminalProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    piano.prepare(sampleRate);
    roomTone.prepare(sampleRate);
    room.prepare(sampleRate);
    juce::ignoreUnused(samplesPerBlock);
    // 20 ms smoothing for levels; Power fades over about 60 ms.
    const float levelCoef = float(1.0 - std::exp(-1.0 / (0.02 * sampleRate)));
    for (auto* s : { &masterGain, &instrumentGain, &ambienceGain, &spaceMix }) s->coef = levelCoef;
    powerGain.coef = float(1.0 - std::exp(-1.0 / (0.012 * sampleRate)));
    powerGain.reset(power->load() > 0.5f ? 1.0f : 0.0f);
    masterGain.reset(params::levelGain(master->load()));
    instrumentGain.reset(params::levelGain(instrumentLevel->load()));
    ambienceGain.reset(params::levelGain(ambienceLevel->load()));
    spaceMix.reset(space->load());
}

void LiminalProcessor::handleMidi(const juce::MidiMessage& m) {
    if (m.isNoteOn()) {
        piano.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    } else if (m.isNoteOff()) {
        piano.noteOff(m.getNoteNumber());
    } else if (m.isSustainPedalOn()) {
        piano.sustain(true);
    } else if (m.isSustainPedalOff()) {
        piano.sustain(false);
    } else if (m.isPitchWheel()) {
        piano.pitchBend(m.getPitchWheelValue());
    } else if (m.isAllSoundOff()) {
        piano.allSoundOff();
    } else if (m.isAllNotesOff() || m.isResetAllControllers()) {
        piano.allNotesOff();
    }
}

void LiminalProcessor::renderSegment(float* left, float* right, int n) {
    float* il = scratchL;
    float* ir = scratchR;
    std::fill(il, il + n, 0.0f);
    std::fill(ir, ir + n, 0.0f);
    piano.render(il, ir, n);
    for (int i = 0; i < n; ++i) {
        const float p = powerGain.next(), m = masterGain.next() * p;
        const float ig = instrumentGain.next(), ag = ambienceGain.next(), sp = spaceMix.next();
        const dsp::Stereo bed = roomTone.tick();
        const float instL = il[i] * ig, instR = ir[i] * ig;
        const float ambL = bed.l * ag, ambR = bed.r * ag;
        // The room hears the instrument fully and the ambience a little (it already is a room).
        const dsp::Stereo wet = room.tick({ instL + 0.4f * ambL, instR + 0.4f * ambR });
        const float dry = 1.0f - 0.45f * sp;
        float l = (dry * (instL + ambL) + sp * wet.l) * m;
        float r = (dry * (instR + ambR) + sp * wet.r) * m;
        // Soft safety limit above -3 dBFS; never above 0 dBFS.
        const auto soft = [](float x) {
            const float a = std::abs(x);
            if (a <= 0.7f) return x;
            return std::copysign(0.7f + 0.3f * std::tanh((a - 0.7f) / 0.3f), x);
        };
        l = soft(l);
        r = soft(r);
        if (p < 1.0e-6f) l = r = 0.0f;   // Power off is exact silence once faded
        left[i] = l;
        right[i] = r;
        instrumentSum += double(instL * instL + instR * instR) * 0.5;
        ambienceSum += double(ambL * ambL + ambR * ambR) * 0.5;
        outputSum += double(l * l + r * r) * 0.5;
    }
}

void LiminalProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    powerGain.target = power->load() > 0.5f ? 1.0f : 0.0f;
    masterGain.target = params::levelGain(master->load());
    instrumentGain.target = params::levelGain(instrumentLevel->load());
    ambienceGain.target = params::levelGain(ambienceLevel->load());
    spaceMix.target = space->load();

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    instrumentSum = ambienceSum = outputSum = 0;
    // Rendered in chunks of at most `chunk` samples, split at each MIDI event's position.
    auto renderRange = [&](int from, int to) {
        float monoRight[chunk];
        while (from < to) {
            const int n = std::min(chunk, to - from);
            if (right != nullptr) {
                renderSegment(left + from, right + from, n);
            } else {
                renderSegment(left + from, monoRight, n);
                for (int i = 0; i < n; ++i) left[from + i] = 0.5f * (left[from + i] + monoRight[i]);
            }
            from += n;
        }
    };
    int position = 0;
    for (const auto metadata : midi) {
        const int at = std::clamp(metadata.samplePosition, 0, total);
        renderRange(position, at);
        position = at;
        handleMidi(metadata.getMessage());
    }
    renderRange(position, total);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, total);

    if (total > 0) {
        meters.instrument.store(float(std::sqrt(instrumentSum / total)), std::memory_order_relaxed);
        meters.ambience.store(float(std::sqrt(ambienceSum / total)), std::memory_order_relaxed);
        meters.output.store(float(std::sqrt(outputSum / total)), std::memory_order_relaxed);
    }
    meters.voices.store(piano.activeVoices(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* LiminalProcessor::createEditor() { return new LiminalEditor(*this); }

void LiminalProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("stateVersion", stateVersion);
    copyXmlToBinary(*xml, destData);
}

void LiminalProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;   // not ours: ignore
    parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

} // namespace liminal

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new liminal::LiminalProcessor(); }
