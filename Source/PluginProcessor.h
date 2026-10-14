// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/OversamplingEngine.h"
#include <atomic>
class NkblimitProcessor final : public juce::AudioProcessor {
public:
    NkblimitProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&,juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "nkblimit"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return preparedRate > 0 ? double(getLatencySamples())/preparedRate : 0.01; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState state;
    struct MeterValues {
        std::atomic<float> input {-240}, output {-240}, truePeak {-240}, reduction {0};
    } meters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    nkb::Parameters parameters() const noexcept;
    template<typename T> void process(juce::AudioBuffer<T>&,nkb::OversamplingEngine<T>&);
    nkb::OversamplingEngine<float> floatEngine;
    nkb::OversamplingEngine<double> doubleEngine;
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    double preparedRate = 0;
    int preparedBlock = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NkblimitProcessor)
};
