#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "VegaUI.h"

class VegaChordsEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit VegaChordsEditor (VegaChordsProcessor&);
    ~VegaChordsEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    static constexpr int W = 1300, H = 800;
    class Main;
private:
    void timerCallback() override;
    VegaChordsProcessor& proc;
    VegaLookAndFeel lnf;
    std::unique_ptr<Main> main;
    juce::ComponentBoundsConstrainer constrainer;
};
