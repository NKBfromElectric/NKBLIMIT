// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"

namespace {
const juce::Colour background {0xff11191e};
const juce::Colour panel {0xff19242b};
const juce::Colour border {0xff30414b};
const juce::Colour text {0xffedf5f8};
const juce::Colour muted {0xff9eb1bc};
const juce::Colour orange {0xffffaf6a};
const juce::Colour blue {0xff56cbe8};
const juce::Colour ceilingAccent {0xffb8d5df};

juce::String decibels(float value) {
    if (!std::isfinite(value) || value < -120.0f) return "-inf";
    // Avoid alternating between a signed zero and zero around silence/release.
    return juce::String(std::abs(value) < 0.05f ? 0.0f : value, 1);
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(panel);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 8.0f, 1.0f);
}
}

NkblimitLookAndFeel::NkblimitLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, background);
    setColour(juce::Slider::textBoxOutlineColourId, border.brighter(0.2f));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff253640));
    setColour(juce::TextButton::textColourOffId, muted);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff22333d));
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

juce::Slider::SliderLayout NkblimitLookAndFeel::getSliderLayout(juce::Slider& slider) {
    if (slider.getSliderStyle() != juce::Slider::LinearVertical)
        return LookAndFeel_V4::getSliderLayout(slider);
    juce::Slider::SliderLayout layout;
    layout.textBoxBounds = {(slider.getWidth()-142)/2, 0, 142, 34};
    layout.sliderBounds = {slider.getWidth()/2-25, 64, 50, std::max(1,slider.getHeight()-76)};
    return layout;
}

juce::Label* NkblimitLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::FontOptions {23.0f, juce::Font::bold});
    label->setJustificationType(juce::Justification::centred);
    return label;
}

void NkblimitLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float position, float minimumPosition, float maximumPosition,
                                          juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style != juce::Slider::LinearVertical) {
        LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimumPosition,maximumPosition,style,slider);
        return;
    }
    const bool isThreshold = slider.getName() == "Threshold";
    const auto accent = slider.findColour(juce::Slider::trackColourId);
    const float centre = float(x)+float(width)*0.5f;
    const float top = float(y), bottom = float(y+height);
    const float thumbY = juce::jlimit(top,bottom,position);
    g.setColour(background.darker(0.15f));
    g.fillRoundedRectangle(centre-6.0f,top,12.0f,float(height),5.0f);
    g.setColour(accent.withAlpha(0.7f));
    const auto activeTop = isThreshold ? top : thumbY;
    const auto activeBottom = isThreshold ? thumbY : bottom;
    g.fillRoundedRectangle(centre-3.0f,activeTop,6.0f,std::max(0.0f,activeBottom-activeTop),3.0f);

    const auto drawTick = [&](double value) {
        if (value < slider.getMinimum() || value > slider.getMaximum()) return;
        const float tickY = bottom-float(slider.valueToProportionOfLength(value))*float(height);
        g.setColour(border.brighter(0.2f));
        g.drawHorizontalLine(juce::roundToInt(tickY),centre+14.0f,centre+23.0f);
        g.setColour(muted);
        g.setFont(11.0f);
        g.drawText(juce::String(value,isThreshold || std::abs(value-std::round(value))<0.01 ? 0 : 1),
                   juce::roundToInt(centre+28.0f),juce::roundToInt(tickY)-7,53,14,juce::Justification::centredLeft);
    };
    if (isThreshold) for (double value : {-24.,-18.,-12.,-6.,0.,6.,12.}) drawTick(value);
    else for (double value : {-3.,-2.5,-2.,-1.5,-1.,-.5,0.}) drawTick(value);

    const juce::Rectangle<float> thumb {centre-22.0f,thumbY-8.0f,44.0f,16.0f};
    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.fillRoundedRectangle(thumb.translated(0.0f,2.0f),3.0f);
    g.setGradientFill(juce::ColourGradient(accent.brighter(0.2f),thumb.getTopLeft(),
                                         accent.darker(0.35f),thumb.getBottomLeft(),false));
    g.fillRoundedRectangle(thumb,3.0f);
    g.setColour(accent.brighter(0.4f));
    g.drawRoundedRectangle(thumb.reduced(0.5f),3.0f,1.0f);
    g.setColour(background.withAlpha(0.8f));
    g.fillRect(centre-14.0f,thumbY-1.0f,28.0f,2.0f);
}

NkblimitEditor::NkblimitEditor(NkblimitProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&lookAndFeel);
    for (auto* slider : {&threshold,&ceiling}) {
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::TextBoxAbove,false,142,34);
        slider->setTextValueSuffix(slider == &threshold ? " dB" : " dBTP");
        slider->setColour(juce::Slider::trackColourId,slider == &threshold ? blue : ceilingAccent);
        slider->setMouseDragSensitivity(230);
        slider->setVelocityModeParameters(0.2,1,0.0,true,juce::ModifierKeys::shiftModifier);
        slider->setNumDecimalPlacesToDisplay(2);
        addAndMakeVisible(*slider);
    }
    threshold.setName("Threshold");
    ceiling.setName("Ceiling");
    threshold.setRange(-24.5,11.5,0.01);
    threshold.setTooltip("Lower the threshold to increase loudness. Shift-drag for fine adjustment; double-click to reset.");
    ceiling.setTooltip("Maximum output level. Shift-drag for fine adjustment; double-click resets to -0.5 dBTP.");
    threshold.onDragStart = [this] {
        if (thresholdAttachment && !thresholdGestureActive) {
            thresholdGestureActive = true;
            thresholdAttachment->beginGesture();
        }
    };
    threshold.onDragEnd = [this] {
        if (thresholdAttachment && thresholdGestureActive) {
            thresholdAttachment->endGesture();
            thresholdGestureActive = false;
        }
    };
    threshold.onValueChange = [this] { thresholdChanged(); };
    ceiling.onValueChange = [this] { syncThreshold(); };
    oversampling.addItem("4x",1);
    oversampling.addItem("8x",2);
    oversampling.setName("Oversampling");
    oversampling.setTooltip("Oversampling used by the limiter.");
    addAndMakeVisible(oversampling);
    resetThreshold.setName("Reset Threshold");
    resetCeiling.setName("Reset Ceiling");
    resetThreshold.setTooltip("Reset threshold to the ceiling, with no loudness boost.");
    resetCeiling.setTooltip("Reset ceiling to -0.5 dBTP.");
    addAndMakeVisible(resetThreshold);
    addAndMakeVisible(resetCeiling);
    resetThreshold.onClick = [this] { resetParameter("inputGain",0.0f); };
    resetCeiling.onClick = [this] { resetParameter("ceiling",-0.5f); };
    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"ceiling",ceiling);
    osAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"oversampling",oversampling);
    if (auto* gain = p.state.getParameter("inputGain")) {
        thresholdAttachment = std::make_unique<juce::ParameterAttachment>(*gain,[this](float) { syncThreshold(); });
        thresholdAttachment->sendInitialUpdate();
    }
    syncThreshold();
    setSize(860,560);
    meterEpoch = processor.meters.epoch.load(std::memory_order_acquire);
    consumeMeasurements();
    updateNumbers();
    lastTimerTime = juce::Time::getMillisecondCounterHiRes()*0.001;
    startTimerHz(30);
}

NkblimitEditor::~NkblimitEditor() {
    stopTimer();
    if (thresholdAttachment && thresholdGestureActive) thresholdAttachment->endGesture();
    threshold.onDragStart = {};
    threshold.onDragEnd = {};
    threshold.onValueChange = {};
    ceiling.onValueChange = {};
    setLookAndFeel(nullptr);
}

void NkblimitEditor::syncThreshold() {
    const auto* gainParameter = processor.state.getParameter("inputGain");
    const auto* ceilingParameter = processor.state.getParameter("ceiling");
    if (gainParameter == nullptr || ceilingParameter == nullptr) return;
    const double gainDb = gainParameter->convertFrom0to1(gainParameter->getValue());
    const double ceilingDb = ceilingParameter->convertFrom0to1(ceilingParameter->getValue());
    const juce::ScopedValueSetter<bool> guard(syncingThreshold,true);
    // This is a view of the existing gain parameter. Ceiling changes only
    // update that view; they must never change gain or saved host automation.
    if (std::abs(threshold.getMinimum()-(ceilingDb-24.0))>1.0e-6
        || std::abs(threshold.getMaximum()-(ceilingDb+12.0))>1.0e-6)
        threshold.setRange(ceilingDb-24.0,ceilingDb+12.0,0.01);
    threshold.setValue(ceilingDb-gainDb,juce::dontSendNotification);
    threshold.setDoubleClickReturnValue(true,ceilingDb);
}

void NkblimitEditor::thresholdChanged() {
    if (syncingThreshold || thresholdAttachment == nullptr) return;
    if (const auto* parameter = processor.state.getParameter("ceiling")) {
        const double ceilingDb = parameter->convertFrom0to1(parameter->getValue());
        const float gainDb = float(juce::jlimit(-12.0,24.0,ceilingDb-threshold.getValue()));
        if (thresholdGestureActive) thresholdAttachment->setValueAsPartOfGesture(gainDb);
        else thresholdAttachment->setValueAsCompleteGesture(gainDb);
    }
}

void NkblimitEditor::resetParameter(const char* id, float defaultValue) {
    if (auto* parameter = processor.state.getParameter(id)) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(defaultValue));
        parameter->endChangeGesture();
    }
}

void NkblimitEditor::drawReadout(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title,
                                const juce::String& value, const juce::String& unit,
                                const juce::String& caption, juce::Colour accent) {
    drawPanel(g,bounds);
    const auto left = bounds.getX()+18;
    const auto width = bounds.getWidth()-36;
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText(title,left,bounds.getY()+11,width,18,juce::Justification::centredLeft);
    g.setColour(accent);
    g.setFont(juce::FontOptions {40.0f,juce::Font::bold});
    g.drawText(value,left,bounds.getY()+31,width,45,juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(13.0f);
    g.drawText(unit,left,bounds.getY()+77,width,17,juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(11.5f);
    g.drawText(caption,left,bounds.getY()+99,width,16,juce::Justification::centredLeft);
}

void NkblimitEditor::drawLevelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float level, float held, juce::Colour accent) {
    const auto yAt = [&bounds](float db) {
        return bounds.getBottom()-juce::jlimit(0.0f,1.0f,(db+60.0f)/66.0f)*bounds.getHeight();
    };
    g.setColour(background);
    g.fillRoundedRectangle(bounds,4.0f);
    const auto filled = bounds.withTop(yAt(level)).reduced(3.0f,0.0f);
    if (filled.getHeight()>0.0f) {
        g.setGradientFill(juce::ColourGradient(accent.darker(0.35f),bounds.getBottomLeft(),
                                              accent,bounds.getTopLeft(),false));
        g.fillRoundedRectangle(filled,2.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawHorizontalLine(juce::roundToInt(yAt(0.0f)),bounds.getX(),bounds.getRight());
    if (held>=-60.0f) {
        g.setColour(held>0.0f ? juce::Colour(0xffff7373) : text);
        g.fillRect(bounds.getX()+1.0f,yAt(held),bounds.getWidth()-2.0f,2.0f);
    }
}

void NkblimitEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(text);
    g.setFont(juce::FontOptions {30.0f,juce::Font::bold});
    g.drawText("NKB LIMIT",24,15,236,40,juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("MASTERING LIMITER",250,23,220,25,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff20363e));
    g.fillRoundedRectangle(718.0f,22.0f,118.0f,28.0f,14.0f);
    g.setColour(blue);
    g.setFont(13.0f);
    g.drawText("Transparent",718,22,118,28,juce::Justification::centred);
    g.setColour(border);
    g.drawHorizontalLine(68,24.0f,836.0f);

    drawReadout(g,{24,85,260,125},"OUTPUT LOUDNESS",loudnessText,"LUFS-S",
                loudnessReady ? "3 s short-term" : "Measuring first 3 s...",text);
    drawReadout(g,{300,85,260,125},"TRUE PEAK",truePeakText,"dBTP","1 s peak hold / estimate",blue);
    drawReadout(g,{576,85,260,125},"GAIN REDUCTION",reductionText,"dB","1 s hold",orange);

    drawPanel(g,{24,226,148,286});
    drawPanel(g,{188,226,484,286});
    drawPanel(g,{688,226,148,286});
    g.setColour(border);
    g.drawVerticalLine(430,246.0f,492.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions {15.0f,juce::Font::bold});
    g.drawText("THRESHOLD",212,236,200,24,juce::Justification::centred);
    g.drawText("CEILING",448,236,200,24,juce::Justification::centred);
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("Lower = louder",212,306,200,18,juce::Justification::centred);
    g.drawText("Maximum output level",448,306,200,18,juce::Justification::centred);

    g.setFont(juce::FontOptions {12.0f,juce::Font::bold});
    g.setColour(orange);
    g.drawText("INPUT",40,240,116,20,juce::Justification::centred);
    g.setColour(blue);
    g.drawText("OUTPUT",704,240,116,20,juce::Justification::centred);
    g.setColour(text);
    g.setFont(juce::FontOptions {28.0f,juce::Font::bold});
    g.drawText(inputText,36,265,124,34,juce::Justification::centred);
    g.drawText(outputText,700,265,124,34,juce::Justification::centred);
    g.setColour(muted);
    g.setFont(11.0f);
    g.drawText("dBFS",40,302,116,16,juce::Justification::centred);
    g.drawText("dBFS",704,302,116,16,juce::Justification::centred);
    const juce::Rectangle<float> inBar {62.0f,329.0f,28.0f,148.0f};
    const juce::Rectangle<float> outBar {770.0f,329.0f,28.0f,148.0f};
    drawLevelMeter(g,inBar,inputBar.get(),inputHold.get(),orange);
    drawLevelMeter(g,outBar,outputBar.get(),outputHold.get(),blue);
    g.setFont(10.0f);
    g.setColour(muted);
    for (auto db : {6,0,-6,-12,-24,-36,-48,-60}) {
        const auto tickY = juce::roundToInt(477.0f-(float(db)+60.0f)/66.0f*148.0f);
        const auto label = db>0 ? "+"+juce::String(db) : juce::String(db);
        g.drawText(label,100,tickY-6,46,12,juce::Justification::centredLeft);
        g.drawText(label,714,tickY-6,46,12,juce::Justification::centredRight);
    }
    g.setFont(11.0f);
    g.drawText("1 s peak hold",36,490,124,16,juce::Justification::centred);
    g.drawText("1 s peak hold",700,490,124,16,juce::Justification::centred);
    g.setColour(border);
    g.drawHorizontalLine(525,24.0f,836.0f);
    g.setColour(muted);
    g.setFont(12.0f);
    g.drawText("OVERSAMPLING",24,533,132,22,juce::Justification::centredLeft);
    g.setColour(blue);
    g.fillEllipse(661.0f,541.0f,5.0f,5.0f);
    g.setColour(muted);
    g.drawText("True Peak protection",674,533,162,22,juce::Justification::centredRight);
}

void NkblimitEditor::resized() {
    threshold.setBounds(216,263,192,214);
    ceiling.setBounds(452,263,192,214);
    resetThreshold.setBounds(265,486,94,24);
    resetCeiling.setBounds(501,486,94,24);
    oversampling.setBounds(162,532,84,26);
}

void NkblimitEditor::consumeMeasurements() {
    // Audio publishes maxima since the last exchange; consume each exactly once.
    const auto in = processor.meters.input.exchange(-240.0f,std::memory_order_relaxed);
    const auto out = processor.meters.output.exchange(-240.0f,std::memory_order_relaxed);
    const auto peak = processor.meters.truePeak.exchange(-240.0f,std::memory_order_relaxed);
    const auto reduction = processor.meters.reduction.exchange(0.0f,std::memory_order_relaxed);
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
    for (auto* meter : {&inputBar,&outputBar,&inputHold,&outputHold,&truePeakHold,&reductionHold}) meter->reset();
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
    // ParameterAttachment handles host changes; this also keeps a derived range
    // current when a host restores several parameters in one state operation.
    syncThreshold();
    const auto now = juce::Time::getMillisecondCounterHiRes()*0.001;
    const auto elapsed = std::max(0.0,now-lastTimerTime);
    lastTimerTime = now;
    const auto epoch = processor.meters.epoch.load(std::memory_order_acquire);
    if (epoch != meterEpoch) {
        meterEpoch = epoch;
        resetDisplays();
        consumeMeasurements();
        updateNumbers();
    } else {
        for (auto* meter : {&inputBar,&outputBar,&inputHold,&outputHold,&truePeakHold,&reductionHold}) meter->advance(elapsed);
        consumeElapsed += elapsed;
        numberElapsed += elapsed;
        if (consumeElapsed >= 0.1) {
            consumeElapsed = std::fmod(consumeElapsed,0.1);
            consumeMeasurements();
        }
        if (numberElapsed >= 0.2) {
            numberElapsed = std::fmod(numberElapsed,0.2);
            updateNumbers();
        }
    }
    repaint();
}
