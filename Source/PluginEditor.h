#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PlateReverbEditor : public juce::AudioProcessorEditor
{
public:
    explicit PlateReverbEditor (PlateReverbProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    PlateReverbProcessor& proc;
    std::array<Knob, 10> knobs;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateReverbEditor)
};
