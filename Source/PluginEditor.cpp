#include "PluginEditor.h"

namespace
{
    struct Def { const char* id; const char* name; };
    const Def defs[10] = {
        { "predelay", "PRE-DELAY" }, { "decay", "DECAY" },     { "damping", "DAMPING" },  { "bandwidth", "BANDWIDTH" }, { "diffusion", "DIFFUSION" },
        { "mod", "MOD" },            { "lowcut", "LOW CUT" },  { "highcut", "HIGH CUT" }, { "width", "WIDTH" },         { "mix", "MIX" } };
    const juce::Colour bg { 0xff1b1d21 }, panel { 0xff24272c }, accent { 0xffd9a441 }, text { 0xffe8e6e1 };
}

PlateReverbEditor::PlateReverbEditor (PlateReverbProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3e45));
        k.slider.setColour (juce::Slider::thumbColourId, text);
        k.slider.setColour (juce::Slider::textBoxTextColourId, text);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (defs[i].id)->convertFrom0to1 (proc.apvts.getParameter (defs[i].id)->getDefaultValue()));
        k.label.setText (defs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colour (0xff9a9890));
        k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[i].id, k.slider);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
    }
    setSize (620, 400);
}

void PlateReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().reduced (12).withTrimmedTop (44).toFloat(), 8.0f);
    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("PLATE REVERB", 24, 10, 300, 30, juce::Justification::centredLeft);
    g.setColour (accent);
    g.fillRect (24, 40, 40, 2);
}

void PlateReverbEditor::resized()
{
    auto area = getLocalBounds().reduced (12).withTrimmedTop (44).reduced (8);
    const int cols = 5, rows = 2;
    const int w = area.getWidth() / cols, h = area.getHeight() / rows;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + (int) (i % cols) * w, area.getY() + (int) (i / cols) * h, w, h).reduced (6);
        knobs[i].label.setBounds (cell.removeFromTop (16));
        knobs[i].slider.setBounds (cell);
    }
}
