// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
class NkblimitEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit NkblimitEditor(NkblimitProcessor&);
    ~NkblimitEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    NkblimitProcessor& processor;
    juce::Slider input,ceiling;
    juce::Label inputLabel,ceilingLabel,meterLabel;
    juce::ComboBox oversampling;
    juce::TextButton resetInput {"Reset gain"},resetCeiling {"Reset ceiling"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment,ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAttachment;
};
