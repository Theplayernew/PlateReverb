#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PlateReverb.h"

class PlateReverbProcessor : public juce::AudioProcessor
{
public:
    PlateReverbProcessor();
    ~PlateReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override  { processT (b); }
    void processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) override { processT (b); }
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Plate Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    template <typename T> void processT (juce::AudioBuffer<T>&);
    plate::PlateReverb reverb;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateReverbProcessor)
};
