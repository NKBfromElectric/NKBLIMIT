// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"

namespace {
const juce::Colour background {0xff101923};
const juce::Colour panel {0xff172430};
const juce::Colour border {0xff2d3e4c};
const juce::Colour text {0xffedf4f7};
const juce::Colour muted {0xff9fb3c2};
const juce::Colour orange {0xffffa354};
const juce::Colour blue {0xff62c9ec};

juce::String decibels(float value) {
    if (!std::isfinite(value) || value < -120.0f) return "-inf";
    // Avoid alternating between a signed zero and zero around silence/release.
    return juce::String(std::abs(value) < 0.05f ? 0.0f : value, 1);
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(panel);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 10.0f, 1.0f);
}
}

NkblimitLookAndFeel::NkblimitLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, border.brighter(0.2f));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff253744));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff223440));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border.brighter(0.2f));
    setColour(juce::ComboBox::arrowColourId, muted);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff345365));
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, border);
}

void NkblimitLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float position, float startAngle, float endAngle, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                               static_cast<float>(width), static_cast<float>(height)).reduced(9.0f);
    const auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    juce::Path track, active;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour(border.brighter(0.1f));
    g.strokePath(track, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    g.setColour(accent);
    g.strokePath(active, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(background);
    g.fillEllipse(centre.x - radius + 10.0f, centre.y - radius + 10.0f, (radius - 10.0f) * 2.0f, (radius - 10.0f) * 2.0f);
    const juce::Point<float> outer {centre.x + std::sin(angle) * (radius - 15.0f),
                                    centre.y - std::cos(angle) * (radius - 15.0f)};
    const juce::Point<float> inner {centre.x + std::sin(angle) * (radius * 0.35f),
                                    centre.y - std::cos(angle) * (radius * 0.35f)};
    g.setColour(accent);
    g.drawLine({inner, outer}, 3.0f);
    g.fillEllipse(centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
}

NkblimitEditor::NkblimitEditor(NkblimitProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&lookAndFeel);
    for (auto* slider : {&input, &ceiling}) {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 126, 32);
        slider->setTextValueSuffix(slider == &input ? " dB" : " dBTP");
        slider->setColour(juce::Slider::rotarySliderFillColourId, slider == &input ? orange : blue);
        slider->setMouseDragSensitivity(230);
        slider->setVelocityModeParameters(0.2, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider->setDoubleClickReturnValue(true, slider == &input ? 0.0 : -0.5);
        addAndMakeVisible(*slider);
    }
    input.setName("Input gain");
    ceiling.setName("Output ceiling");
    input.setTooltip("Level before limiting. Drag to adjust; Shift-drag for fine adjustment. Double-click resets to 0 dB.");
    ceiling.setTooltip("Maximum output level. Drag to adjust; Shift-drag for fine adjustment. Double-click resets to -0.5 dBTP.");
    oversampling.addItem("4x", 1);
    oversampling.addItem("8x", 2);
    oversampling.setName("Oversampling");
    oversampling.setTooltip("Oversampling used by the limiter.");
    addAndMakeVisible(oversampling);
    addAndMakeVisible(resetInput);
    addAndMakeVisible(resetCeiling);
    resetInput.onClick = [this] { resetParameter("inputGain", 0.0f); };
    resetCeiling.onClick = [this] { resetParameter("ceiling", -0.5f); };
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, "inputGain", input);
    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, "ceiling", ceiling);
    osAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "oversampling", oversampling);
    input.setNumDecimalPlacesToDisplay(2);
    ceiling.setNumDecimalPlacesToDisplay(2);
    setSize(820, 500);
    meterEpoch = processor.meters.epoch.load(std::memory_order_acquire);
    consumeMeasurements();
    updateNumbers();
    lastTimerTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz(30);
}

NkblimitEditor::~NkblimitEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void NkblimitEditor::resetParameter(const char* id, float defaultValue) {
    if (auto* parameter = processor.state.getParameter(id)) {
        // A button is a complete automation gesture, rather than a slider drag.
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(defaultValue));
        parameter->endChangeGesture();
    }
}

void NkblimitEditor::drawReadout(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title,
                                const juce::String& value, const juce::String& unit,
                                const juce::String& caption, juce::Colour accent) {
    drawPanel(g, bounds);
    const auto left = bounds.getX() + 16;
    const auto width = bounds.getWidth() - 32;
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText(title, left, bounds.getY() + 12, width, 18, juce::Justification::centredLeft);
    g.setColour(accent);
    g.setFont(juce::FontOptions {36.0f, juce::Font::bold});
    g.drawText(value, left, bounds.getY() + 33, width, 42, juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(13.0f);
    g.drawText(unit, left, bounds.getY() + 77, width, 18, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(11.5f);
    g.drawText(caption, left, bounds.getY() + 100, width, 16, juce::Justification::centredLeft);
}

void NkblimitEditor::drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float level, float held, juce::Colour accent) {
    const auto yAt = [&bounds](float db) {
        return bounds.getBottom() - juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f) * bounds.getHeight();
    };
    g.setColour(background);
    g.fillRoundedRectangle(bounds, 4.0f);
    const auto filled = bounds.withTop(yAt(level)).reduced(3.0f, 0.0f);
    if (filled.getHeight() > 0.0f) {
        g.setGradientFill(juce::ColourGradient(accent.darker(0.35f), bounds.getBottomLeft(),
                                              accent, bounds.getTopLeft(), false));
        g.fillRoundedRectangle(filled, 2.0f);
    }
    const auto zeroY = yAt(0.0f);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawHorizontalLine(juce::roundToInt(zeroY), bounds.getX(), bounds.getRight());
    if (held >= -60.0f) {
        g.setColour(held > 0.0f ? juce::Colour(0xffff7373) : text);
        g.fillRect(bounds.getX() + 1.0f, yAt(held), bounds.getWidth() - 2.0f, 2.0f);
    }
}

void NkblimitEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(text);
    g.setFont(juce::FontOptions {29.0f, juce::Font::bold});
    g.drawText("nkblimit", 24, 15, 180, 40, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("MASTERING LIMITER", 195, 23, 200, 25, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff223a43));
    g.fillRoundedRectangle(680.0f, 22.0f, 116.0f, 28.0f, 14.0f);
    g.setColour(blue);
    g.setFont(13.0f);
    g.drawText("Transparent", 680, 22, 116, 28, juce::Justification::centred);
    g.setColour(border);
    g.drawHorizontalLine(68, 24.0f, 796.0f);

    drawPanel(g, {24, 90, 178, 352});
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("PEAK / dBFS", 40, 104, 146, 20, juce::Justification::centredLeft);
    g.setColour(orange);
    g.drawText("IN", 31, 134, 68, 18, juce::Justification::centred);
    g.setColour(blue);
    g.drawText("OUT", 127, 134, 68, 18, juce::Justification::centred);
    g.setColour(text);
    g.setFont(juce::FontOptions {18.0f, juce::Font::bold});
    g.drawText(inputText, 28, 156, 74, 25, juce::Justification::centred);
    g.drawText(outputText, 124, 156, 74, 25, juce::Justification::centred);
    const juce::Rectangle<float> inBar {53.0f, 196.0f, 24.0f, 200.0f};
    const juce::Rectangle<float> outBar {149.0f, 196.0f, 24.0f, 200.0f};
    drawLevelMeter(g, inBar, inputBar.get(), inputHold.get(), orange);
    drawLevelMeter(g, outBar, outputBar.get(), outputHold.get(), blue);
    g.setFont(11.0f);
    g.setColour(muted);
    for (auto db : {6, 0, -6, -12, -24, -36, -48, -60}) {
        const auto y = juce::roundToInt(396.0f - (static_cast<float>(db) + 60.0f) / 66.0f * 200.0f);
        g.drawText(db > 0 ? "+" + juce::String(db) : juce::String(db), 83, y - 7, 58, 14, juce::Justification::centred);
    }
    g.drawText("1 s peak hold", 40, 412, 146, 18, juce::Justification::centred);

    drawReadout(g, {222, 90, 181, 128}, "OUTPUT LOUDNESS", loudnessText, "LUFS-S",
                loudnessReady ? "3 s short-term" : "Measuring first 3 s...", text);
    drawReadout(g, {415, 90, 181, 128}, "TRUE PEAK", truePeakText, "dBTP (estimate)", "1 s peak hold", blue);
    drawReadout(g, {608, 90, 188, 128}, "GAIN REDUCTION", reductionText, "dB", "1 s hold / control estimate", orange);

    drawPanel(g, {222, 234, 282, 208});
    drawPanel(g, {516, 234, 280, 208});
    g.setColour(text);
    g.setFont(juce::FontOptions {15.0f, juce::Font::bold});
    g.drawText("INPUT GAIN", 242, 248, 242, 25, juce::Justification::centredLeft);
    g.drawText("OUTPUT CEILING", 536, 248, 240, 25, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(13.0f);
    g.drawFittedText("Adjust\nloudness", 393, 292, 94, 48, juce::Justification::centredLeft, 2);
    g.drawFittedText("Set output\nlimit", 685, 292, 94, 48, juce::Justification::centredLeft, 2);
    g.setColour(border);
    g.drawHorizontalLine(455, 24.0f, 796.0f);
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("OVERSAMPLING", 24, 466, 132, 22, juce::Justification::centredLeft);
    g.drawText("True Peak safety on  /  8x TP estimate", 400, 466, 396, 22, juce::Justification::centredRight);
}

void NkblimitEditor::resized() {
    input.setBounds(236, 277, 145, 151);
    ceiling.setBounds(530, 277, 145, 151);
    resetInput.setBounds(388, 370, 100, 28);
    resetCeiling.setBounds(682, 370, 100, 28);
    oversampling.setBounds(162, 465, 84, 26);
}

void NkblimitEditor::consumeMeasurements() {
    // Audio publishes maxima since the last exchange; consume each exactly once.
    const auto in = processor.meters.input.exchange(-240.0f, std::memory_order_relaxed);
    const auto out = processor.meters.output.exchange(-240.0f, std::memory_order_relaxed);
    const auto peak = processor.meters.truePeak.exchange(-240.0f, std::memory_order_relaxed);
    const auto reduction = processor.meters.reduction.exchange(0.0f, std::memory_order_relaxed);
    inputBar.push(in);
    outputBar.push(out);
    inputHold.push(in);
    outputHold.push(out);
    truePeakHold.push(peak);
    reductionHold.push(reduction);
    loudnessReady = processor.meters.loudnessReady.load(std::memory_order_acquire);
    latestLoudness = processor.meters.shortTermLufs.load(std::memory_order_relaxed);
}

void NkblimitEditor::resetDisplays() {
    for (auto* meter : {&inputBar, &outputBar, &inputHold, &outputHold, &truePeakHold, &reductionHold}) meter->reset();
    latestLoudness = -240.0f;
    loudnessReady = false;
    consumeElapsed = 0.0;
    numberElapsed = 0.0;
}

void NkblimitEditor::updateNumbers() {
    inputText = decibels(inputHold.get());
    outputText = decibels(outputHold.get());
    truePeakText = decibels(truePeakHold.get());
    reductionText = decibels(reductionHold.get());
    loudnessText = loudnessReady ? decibels(latestLoudness) : "--";
}

void NkblimitEditor::timerCallback() {
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsed = std::max(0.0, now - lastTimerTime);
    lastTimerTime = now;
    const auto epoch = processor.meters.epoch.load(std::memory_order_acquire);
    if (epoch != meterEpoch) {
        meterEpoch = epoch;
        resetDisplays();
        consumeMeasurements();
        updateNumbers();
    } else {
        for (auto* meter : {&inputBar, &outputBar, &inputHold, &outputHold, &truePeakHold, &reductionHold}) meter->advance(elapsed);
        consumeElapsed += elapsed;
        numberElapsed += elapsed;
        if (consumeElapsed >= 0.1) {
            consumeElapsed = std::fmod(consumeElapsed, 0.1);
            consumeMeasurements();
        }
        if (numberElapsed >= 0.2) {
            numberElapsed = std::fmod(numberElapsed, 0.2);
            updateNumbers();
        }
    }
    repaint();
}
