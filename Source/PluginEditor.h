// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/MeterBallistics.h"

class NkblimitLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    NkblimitLookAndFeel();
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

class NkblimitEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NkblimitEditor(NkblimitProcessor&);
    ~NkblimitEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void consumeMeasurements();
    void resetDisplays();
    void updateNumbers();
    void syncThreshold();
    void thresholdChanged();
    void resetParameter(const char*, float);
    void drawLevelMeter(juce::Graphics&, juce::Rectangle<float>, float, float, juce::Colour);
    void drawReadout(juce::Graphics&, juce::Rectangle<int>, const juce::String&, const juce::String&,
                     const juce::String&, const juce::String&, juce::Colour);
    NkblimitProcessor& processor;
    NkblimitLookAndFeel lookAndFeel;
    juce::Slider threshold, ceiling;
    juce::ComboBox oversampling;
    juce::TextButton resetThreshold {"Reset"}, resetCeiling {"Reset"};
    juce::TooltipWindow tooltips {this, 700};
    std::unique_ptr<juce::ParameterAttachment> thresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAttachment;
    bool syncingThreshold = false, thresholdGestureActive = false;

    nkb::MeterBallistics inputBar {-240.0f, 0.0, 24.0f};
    nkb::MeterBallistics outputBar {-240.0f, 0.0, 24.0f};
    nkb::MeterBallistics inputHold {-240.0f, 1.0, 12.0f};
    nkb::MeterBallistics outputHold {-240.0f, 1.0, 12.0f};
    nkb::MeterBallistics truePeakHold {-240.0f, 1.0, 12.0f};
    nkb::MeterBallistics reductionHold {0.0f, 1.0, 6.0f};
    juce::String inputText {"-inf"}, outputText {"-inf"}, truePeakText {"-inf"};
    juce::String reductionText {"0.0"}, loudnessText {"--"};
    bool loudnessReady = false;
    float latestLoudness = -240.0f;
    std::uint32_t meterEpoch = 0;
    double lastTimerTime = 0.0, consumeElapsed = 0.0, numberElapsed = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NkblimitEditor)
};
