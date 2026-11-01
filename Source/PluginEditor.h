#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PlateLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PlateLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class PlateReverbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PlateReverbEditor (PlateReverbProcessor&);
    ~PlateReverbEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void timerCallback() override;
    float param (const char* id) const;
    void paintVisualiser (juce::Graphics&, juce::Rectangle<float>) const;
    void layoutGroup (juce::Rectangle<int> panel, int firstKnob, int count);

    PlateReverbProcessor& proc;
    PlateLookAndFeel lnf;
    std::array<Knob, 10> knobs;
    juce::Rectangle<int> headerArea, vizArea, spaceArea, characterArea, toneArea;
    float lastSig = -1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateReverbEditor)
};
