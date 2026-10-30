#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2)
    {
        std::cerr << "Usage: host_smoke /path/LIMINAL.vst3\n";
        return 1;
    }
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, argv[1]);
    if (descriptions.isEmpty())
    {
        std::cerr << "VST3 scan failed\n";
        return 1;
    }
    juce::String error;
    auto instance = format.createInstanceFromDescription(*descriptions[0], 48000, 512, error);
    if (!instance)
    {
        std::cerr << error << "\n";
        return 1;
    }
    instance->prepareToPlay(48000, 512);
    double energy = 0;
    for (int block = 0; block < 200; ++block)
    {
        juce::AudioBuffer<float> b(2, 512);
        b.clear();
        juce::MidiBuffer midi;
        instance->processBlock(b, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                auto s = b.getSample(c, i);
                if (!std::isfinite(s) || std::abs(s) > 1.01f)
                    return 1;
                energy += s * s;
            }
    }
    juce::MemoryBlock state;
    instance->getStateInformation(state);
    instance->setStateInformation(state.getData(), int(state.getSize()));
    if (energy < 1e-8)
    {
        std::cerr << "Silent plugin\n";
        return 1;
    }
    if (juce::String(juce::SystemStats::getEnvironmentVariable("DISPLAY", {})).isNotEmpty())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditor());
        if (!editor)
            return 1;
        editor->setSize(1160, 820);
        editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar);
        editor->setTopLeftPosition(10, 10);
        editor->setVisible(true);
        juce::Timer::callAfterDelay(std::getenv("LIMINAL_GUI_HOLD") ? 10000 : 250,
                                    [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
        juce::MessageManager::getInstance()->runDispatchLoop();
        editor.reset();
        std::unique_ptr<juce::AudioProcessorEditor> reopened(instance->createEditor());
        if (!reopened)
            return 1;
    }
    instance->releaseResources();
    std::cout << "VST3 scanned, loaded, rendered non-silent bounded stereo, restored state"
              << (std::getenv("DISPLAY") ? ", opened and reopened editor" : " (editor untested: no display)")
              << "\n";
    return 0;
}
