// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"
NkblimitEditor::NkblimitEditor(NkblimitProcessor& p) : AudioProcessorEditor(p),processor(p) {
    for (auto* slider : {&input,&ceiling}) {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,120,28);
        slider->setTextValueSuffix(slider == &input ? " dB" : " dBTP");
        slider->setColour(juce::Slider::rotarySliderFillColourId,
            slider == &input ? juce::Colour(0xffed944b) : juce::Colour(0xff8faabb));
        slider->setVelocityModeParameters(0.2,1,0.0,true,juce::ModifierKeys::shiftModifier);
        addAndMakeVisible(*slider);
    }
    inputLabel.setText("INPUT GAIN",juce::dontSendNotification);
    ceilingLabel.setText("OUTPUT CEILING",juce::dontSendNotification);
    for (auto* label : {&inputLabel,&ceilingLabel,&meterLabel}) {
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId,juce::Colour(0xffbbc8d4)); addAndMakeVisible(*label);
    }
    oversampling.addItem("4x",1); oversampling.addItem("8x",2); addAndMakeVisible(oversampling);
    addAndMakeVisible(resetInput); addAndMakeVisible(resetCeiling);
    resetInput.onClick = [this] { input.setValue(0.0,juce::sendNotificationSync); };
    resetCeiling.onClick = [this] { ceiling.setValue(-0.5,juce::sendNotificationSync); };
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"inputGain",input);
    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"ceiling",ceiling);
    osAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"oversampling",oversampling);
    setSize(720,390); timerCallback(); startTimerHz(30);
}
NkblimitEditor::~NkblimitEditor() { stopTimer(); }
void NkblimitEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101823));
    g.setColour(juce::Colour(0xffedf1f4)); g.setFont(28.0f); g.drawText("nkblimit",28,18,200,44,juce::Justification::centredLeft);
    g.setFont(15.0f); g.setColour(juce::Colour(0xff8faabb));
    g.drawText("Transparent  /  Phase 1",280,25,400,30,juce::Justification::centredRight);
    g.setColour(juce::Colour(0xff263445)); g.drawLine(28,78,692,78);
    g.setFont(13.0f); g.setColour(juce::Colour(0xff8faabb));
    g.drawText("OVERSAMPLING",520,199,170,25,juce::Justification::centred);
    g.drawText("True Peak safety always on  |  TP estimate: 8x FIR",28,345,664,28,juce::Justification::centred);
}
void NkblimitEditor::resized() {
    meterLabel.setBounds(28,91,getWidth()-56,88);
    inputLabel.setBounds(45,181,180,25); input.setBounds(45,206,180,115); resetInput.setBounds(73,322,124,24);
    ceilingLabel.setBounds(285,181,180,25); ceiling.setBounds(285,206,180,115); resetCeiling.setBounds(313,322,124,24);
    oversampling.setBounds(540,235,130,34);
}
void NkblimitEditor::timerCallback() {
    auto display = [](float v) { return v < -120 ? juce::String("-inf") : juce::String(v,1); };
    meterLabel.setText("IN " + display(processor.meters.input.load()) + " dBFS     OUT "
        + display(processor.meters.output.load()) + " dBFS\nTP estimate "
        + display(processor.meters.truePeak.load()) + " dBTP     GR control "
        + juce::String(processor.meters.reduction.load(),1) + " dB",juce::dontSendNotification);
}
