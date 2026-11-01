#include "PluginEditor.h"

namespace
{
    const juce::Colour kBgTop     { 0xff1a1c21 };
    const juce::Colour kBgBottom  { 0xff0c0d10 };
    const juce::Colour kPanel     { 0xff202328 };
    const juce::Colour kPanelEdge { 0xff30343b };
    const juce::Colour kAccent    { 0xffe0a458 };   // warm copper
    const juce::Colour kText      { 0xffeceae4 };
    const juce::Colour kMuted     { 0xff8d8a82 };

    struct Def { const char* id; const char* name; };
    // Order = layout order: SPACE (0-2), CHARACTER (3-5), TONE (6-9)
    const Def defs[10] = {
        { "predelay", "PRE-DELAY" }, { "time", "REVERB TIME" }, { "mix", "MIX" },
        { "diffusion", "DIFFUSION" }, { "mod", "MOTION" },      { "width", "WIDTH" },
        { "damping", "DAMPING" }, { "bandwidth", "BANDWIDTH" }, { "lowcut", "LOW CUT" }, { "highcut", "HIGH CUT" } };
}

//==============================================================================
PlateLookAndFeel::PlateLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.35f));
    setColour (juce::Label::textColourId, kMuted);
}

void PlateLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float d = std::min (b.getWidth(), b.getHeight());
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const float trackW = std::max (3.0f, r * 0.11f);
    const float arcR = r - trackW * 0.5f;
    const float ang = startAngle + pos * (endAngle - startAngle);
    const bool hot = slider.isMouseOverOrDragging();

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2b2f36));
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
    g.setColour (kAccent.withAlpha (hot ? 0.32f : 0.20f));
    g.strokePath (val, juce::PathStrokeType (trackW + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kAccent);
    g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float br = r - trackW * 2.4f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - br, c.y - br + 3.0f, br * 2.0f, br * 2.0f);               // drop shadow
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5f68), c.x - br * 0.5f, c.y - br * 0.8f,
                                             juce::Colour (0xff1b1d21), c.x + br * 0.6f, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colour (0x33ffffff));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);
    g.setColour (juce::Colour (0xff15171a));
    g.fillEllipse (c.x - br * 0.62f, c.y - br * 0.62f, br * 1.24f, br * 1.24f);    // inset cap
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343840), c.x, c.y - br * 0.6f,
                                             juce::Colour (0xff1e2024), c.x, c.y + br * 0.6f, false));
    g.fillEllipse (c.x - br * 0.58f, c.y - br * 0.58f, br * 1.16f, br * 1.16f);

    // pointer
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.6f, -br * 0.92f, 3.2f, br * 0.5f, 1.6f);
    g.setColour (kAccent);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
PlateReverbEditor::PlateReverbEditor (PlateReverbProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        auto* par = proc.apvts.getParameter (defs[i].id);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        k.slider.setPopupMenuEnabled (true);
        k.label.setText (defs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, defs[i].id, k.slider);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
    }

    setSize (720, 520);
    startTimerHz (30);
}

float PlateReverbEditor::param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void PlateReverbEditor::timerCallback()
{
    const float sig = param ("predelay") * 1.0f + param ("time") * 97.0f + param ("mix") * 1013.0f;
    if (sig != lastSig) { lastSig = sig; repaint (vizArea); }
}

void PlateReverbEditor::layoutGroup (juce::Rectangle<int> panel, int first, int count)
{
    auto a = panel.reduced (10, 8);
    a.removeFromTop (20);                           // panel title
    const int w = a.getWidth() / count;
    for (int i = 0; i < count; ++i)
    {
        auto cell = juce::Rectangle<int> (a.getX() + i * w, a.getY(), w, a.getHeight()).reduced (2, 0);
        knobs[(size_t) (first + i)].label.setBounds (cell.removeFromTop (16));
        knobs[(size_t) (first + i)].slider.setBounds (cell);
    }
}

void PlateReverbEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    headerArea = r.removeFromTop (44);   r.removeFromTop (6);
    vizArea    = r.removeFromTop (120);  r.removeFromTop (10);
    spaceArea  = r.removeFromTop (170);  r.removeFromTop (10);
    characterArea = r.removeFromLeft (r.getWidth() * 3 / 7 - 5);  r.removeFromLeft (10);
    toneArea = r;

    layoutGroup (spaceArea, 0, 3);
    layoutGroup (characterArea, 3, 3);
    layoutGroup (toneArea, 6, 4);
}

//==============================================================================
void PlateReverbEditor::paintVisualiser (juce::Graphics& g, juce::Rectangle<float> r) const
{
    g.setColour (juce::Colour (0xff111316));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kPanelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto plot = r.reduced (16.0f, 14.0f);
    plot.removeFromBottom (14.0f);

    const float pd  = param ("predelay") * 0.001f;
    const float rt  = param ("time");
    const float mix = param ("mix");
    const float wetG = std::sin (mix * juce::MathConstants<float>::halfPi);
    const float dryG = std::cos (mix * juce::MathConstants<float>::halfPi);
    const float tMax = pd + rt * 1.1f + 0.05f;

    auto X = [&] (float t) { return plot.getX() + t / tMax * plot.getWidth(); };
    auto Y = [&] (float a) { return plot.getBottom() - a * plot.getHeight(); };

    // grid
    g.setColour (juce::Colour (0xff1e2127));
    for (int i = 0; i <= 4; ++i)
        g.drawHorizontalLine ((int) Y ((float) i / 4.0f), plot.getX(), plot.getRight());

    // wet decay envelope (amplitude)
    juce::Path env;
    env.startNewSubPath (X (pd), Y (0.0f));
    const int steps = 200;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = pd + (tMax - pd) * (float) i / (float) steps;
        env.lineTo (X (t), Y (wetG * std::pow (10.0f, -3.0f * (t - pd) / rt)));
    }
    env.lineTo (X (tMax), Y (0.0f));
    env.closeSubPath();
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.40f), 0.0f, plot.getY(),
                                             kAccent.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (env);
    g.setColour (kAccent);
    g.strokePath (env, juce::PathStrokeType (1.6f));

    // dry spike
    g.setColour (kText.withAlpha (0.85f));
    g.drawLine (X (0.0f), Y (0.0f), X (0.0f), Y (dryG), 2.0f);

    // markers
    const float dashes[] = { 3.0f, 3.0f };
    g.setColour (kMuted.withAlpha (0.6f));
    g.drawDashedLine ({ X (pd), plot.getY(), X (pd), plot.getBottom() }, dashes, 2, 1.0f);
    g.drawDashedLine ({ X (pd + rt), plot.getY(), X (pd + rt), plot.getBottom() }, dashes, 2, 1.0f);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kMuted);
    auto labelRow = r.reduced (16.0f, 0.0f).removeFromBottom (16.0f);
    g.drawText ("DRY", (int) X (0.0f) - 4, (int) labelRow.getY(), 40, 14, juce::Justification::left);
    const juce::String pdText = "PRE " + juce::String (pd * 1000.0f, 0) + " ms";
    const juce::String rtText = "RT60 " + juce::String (rt, rt < 10.0f ? 2 : 1) + " s";
    g.drawText (pdText, (int) X (pd) + 4, (int) plot.getY(), 90, 14, juce::Justification::left);
    g.setColour (kAccent);
    g.drawText (rtText, (int) X (pd + rt) - 94, (int) plot.getY(), 90, 14, juce::Justification::right);
}

void PlateReverbEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0.0f, 0.0f, kBgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kText);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("PLATE", headerArea.withWidth (90), juce::Justification::centredLeft);
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (24.0f));
    g.drawText ("REVERB", headerArea.withTrimmedLeft (92).withWidth (130), juce::Justification::centredLeft);
    g.setColour (kMuted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("STEREO PLATE  ·  64-BIT PROCESSING", headerArea, juce::Justification::centredRight);

    paintVisualiser (g, vizArea.toFloat());

    auto panel = [&] (juce::Rectangle<int> a, const char* title)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (kPanelEdge);
        g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (kAccent.withAlpha (0.9f));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (title, a.getX() + 14, a.getY() + 8, 200, 14, juce::Justification::left);
        g.setColour (kPanelEdge);
        g.drawHorizontalLine (a.getY() + 26, (float) a.getX() + 14, (float) a.getRight() - 14);
    };
    panel (spaceArea, "SPACE");
    panel (characterArea, "CHARACTER");
    panel (toneArea, "TONE");
}
