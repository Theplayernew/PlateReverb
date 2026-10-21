#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout PlateReverbProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;

    auto pct = [] (float x, int) { return juce::String (juce::roundToInt (x * 100.0f)) + " %"; };
    auto hz  = [] (float x, int) { return x >= 1000.0f ? juce::String (x / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (x)) + " Hz"; };
    auto ms  = [] (float x, int) { return juce::String (x, 1) + " ms"; };

    v.push_back (std::make_unique<P> (juce::ParameterID { "predelay", 1 }, "Pre-Delay", NR (0.0f, 250.0f, 0.01f, 0.5f), 10.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "decay", 1 },    "Decay",     NR (0.0f, 1.0f, 0.001f), 0.6f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "damping", 1 },  "Damping",   NR (500.0f, 20000.0f, 1.0f, 0.3f), 6000.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "bandwidth", 1 },"Bandwidth", NR (500.0f, 20000.0f, 1.0f, 0.3f), 14000.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "diffusion", 1 },"Diffusion", NR (0.0f, 1.0f, 0.001f), 1.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "mod", 1 },      "Modulation",NR (0.0f, 1.0f, 0.001f), 0.3f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "lowcut", 1 },   "Low Cut",   NR (20.0f, 1000.0f, 1.0f, 0.4f), 80.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "highcut", 1 },  "High Cut",  NR (1000.0f, 20000.0f, 1.0f, 0.4f), 12000.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "width", 1 },    "Width",     NR (0.0f, 1.0f, 0.001f), 1.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    v.push_back (std::make_unique<P> (juce::ParameterID { "mix", 1 },      "Mix",       NR (0.0f, 1.0f, 0.001f), 0.3f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    return { v.begin(), v.end() };
}

PlateReverbProcessor::PlateReverbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool PlateReverbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void PlateReverbProcessor::prepareToPlay (double sr, int)
{
    reverb.prepare (sr);
}

void PlateReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        if (ch >= 1 && numIn == 1) buffer.copyFrom (ch, 0, buffer, 0, 0, n);   // mono in -> duplicate to R
        else buffer.clear (ch, 0, n);

    plate::Params p;
    p.predelayMs  = *apvts.getRawParameterValue ("predelay");
    p.decay       = *apvts.getRawParameterValue ("decay");
    p.dampHz      = *apvts.getRawParameterValue ("damping");
    p.bandwidthHz = *apvts.getRawParameterValue ("bandwidth");
    p.diffusion   = *apvts.getRawParameterValue ("diffusion");
    p.modDepth    = *apvts.getRawParameterValue ("mod");
    p.lowCutHz    = *apvts.getRawParameterValue ("lowcut");
    p.highCutHz   = *apvts.getRawParameterValue ("highcut");
    p.width       = *apvts.getRawParameterValue ("width");
    p.mix         = *apvts.getRawParameterValue ("mix");
    reverb.setParams (p);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    for (int i = 0; i < n; ++i)
    {
        float oL, oR;
        reverb.process (L[i], R[i], oL, oR);
        L[i] = oL; R[i] = oR;
    }
}

void PlateReverbProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void PlateReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PlateReverbProcessor::createEditor() { return new PlateReverbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlateReverbProcessor(); }
