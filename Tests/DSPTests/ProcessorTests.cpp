// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include <iostream>
#include <cmath>
#include <complex>
void check(bool x,const char* description) { if (!x) throw std::runtime_error(description); }
namespace {
struct ParameterEvents final : juce::AudioProcessorParameter::Listener {
    void parameterValueChanged(int, float) override { ++changes; }
    void parameterGestureChanged(int, bool starting) override { starting ? ++begins : ++ends; }
    int changes = 0, begins = 0, ends = 0;
};
void checkThresholdControls() {
    // Exercise the UI transform against the actual legacy parameter state.
    // Existing hosts still store/automate inputGain; the UI must neither
    // invert old sessions nor alter it while synchronising Ceiling.
    NkblimitProcessor processor;
    auto* gain = processor.state.getParameter("inputGain");
    auto* ceiling = processor.state.getParameter("ceiling");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    juce::Slider* thresholdSlider = nullptr;
    juce::Slider* ceilingSlider = nullptr;
    for (int i=0; i<editor->getNumChildComponents(); ++i) {
        if (auto* slider = dynamic_cast<juce::Slider*>(editor->getChildComponent(i))) {
            if (slider->getName().equalsIgnoreCase("Threshold")) thresholdSlider = slider;
            if (slider->getName().equalsIgnoreCase("Ceiling")) ceilingSlider = slider;
        }
    }
    check(thresholdSlider && ceilingSlider, "Threshold/Ceiling faders missing");
    check(std::abs(thresholdSlider->getValueFromText("-7.50 dB")+7.5) < 0.001
          && std::abs(ceilingSlider->getValueFromText("-0.50 dBTP")+0.5) < 0.001,
          "Threshold/Ceiling numeric text parsing failed");
    for (float outputCeiling : {-3.0f, -0.5f, 0.0f}) {
        ceiling->setValueNotifyingHost(ceiling->convertTo0to1(outputCeiling));
        for (float inputGain : {-12.0f, 0.0f, 7.0f, 24.0f}) {
            gain->setValueNotifyingHost(gain->convertTo0to1(inputGain));
            check(std::abs(thresholdSlider->getValue()-(outputCeiling-inputGain)) < 0.011,
                  "host parameters did not update derived Threshold");
            check(std::abs(thresholdSlider->getMinimum()-(outputCeiling-24.0)) < 0.011
               && std::abs(thresholdSlider->getMaximum()-(outputCeiling+12.0)) < 0.011,
                  "Threshold range no longer maps to legacy input gain bounds");
        }
    }
    gain->setValueNotifyingHost(gain->convertTo0to1(7.0f));
    ParameterEvents gainEvents;
    gain->addListener(&gainEvents);
    thresholdSlider->setValue(-13.5,juce::sendNotificationSync);
    check(std::abs(processor.state.getRawParameterValue("inputGain")->load()-13.5f) < 0.011f,
          "lowering Threshold did not boost input by the inverse mapping");
    check(gainEvents.changes>0 && gainEvents.begins==1 && gainEvents.ends==1,
          "typed Threshold change did not emit one complete host gesture");
    gainEvents.changes = gainEvents.begins = gainEvents.ends = 0;
    ceilingSlider->setValue(-1.25,juce::sendNotificationSync);
    check(std::abs(processor.state.getRawParameterValue("ceiling")->load()+1.25f) < 0.011f,
          "Ceiling fader did not update host state");
    check(std::abs(processor.state.getRawParameterValue("inputGain")->load()-13.5f) < 0.011f
          && gainEvents.changes==0 && gainEvents.begins==0 && gainEvents.ends==0,
          "Ceiling display synchronisation modified or automated input gain");
    check(std::abs(thresholdSlider->getValue()+14.75) < 0.011,
          "Ceiling fader left derived Threshold stale");
    thresholdSlider->onDragStart();
    thresholdSlider->setValue(-18.25,juce::sendNotificationSync);
    thresholdSlider->setValue(-19.25,juce::sendNotificationSync);
    check(gainEvents.begins==1 && gainEvents.ends==0 && gainEvents.changes==2,
          "Threshold drag did not group changes into one host gesture");
    // Closing an editor during a drag must release its host gesture.
    editor.reset();
    check(gainEvents.ends==1, "editor destruction left a Threshold host gesture open");
    gain->removeListener(&gainEvents);
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        checkThresholdControls();
        NkblimitProcessor p;
        check(p.getParameters().size()==3,"unexpected public parameters");
        auto* gain=p.state.getParameter("inputGain");
        auto* ceiling=p.state.getParameter("ceiling");
        auto* os=p.state.getParameter("oversampling");
        gain->setValueNotifyingHost(gain->convertTo0to1(7.25f));
        ceiling->setValueNotifyingHost(ceiling->convertTo0to1(-1.25f));
        os->setValueNotifyingHost(1);
        juce::MemoryBlock state; p.getStateInformation(state);
        NkblimitProcessor restored; restored.setStateInformation(state.getData(),int(state.getSize()));
        check(std::abs(restored.state.getRawParameterValue("inputGain")->load()-7.25f)<0.01f,"gain restore failed");
        check(std::abs(restored.state.getRawParameterValue("ceiling")->load()+1.25f)<0.01f,"ceiling restore failed");
        check(restored.state.getRawParameterValue("oversampling")->load()==1,"OS restore failed");
        restored.prepareToPlay(48000,64);
        const int latency=restored.getLatencySamples(); check(latency>120,"missing filter/guard latency");
        juce::AudioBuffer<float> f(2,513); f.clear(); f.setSample(0,0,1); f.setSample(1,0,1);
        juce::MidiBuffer midi; restored.processBlock(f,midi);
        os=restored.state.getParameter("oversampling"); os->setValueNotifyingHost(0);
        restored.processBlock(f,midi); check(restored.getLatencySamples()==latency,"OS switch changed latency");
        restored.setProcessingPrecision(juce::AudioProcessor::doublePrecision); restored.prepareToPlay(96000,128);
        juce::AudioBuffer<double> d(2,1024); d.clear(); d.setSample(0,0,2); d.setSample(1,0,2);
        restored.processBlock(d,midi);
        for (int c=0;c<2;++c) for (int i=0;i<d.getNumSamples();++i) check(std::isfinite(d.getSample(c,i)),"bad double output");
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor()); check(editor!=nullptr,"editor creation failed");
        if (auto stream=juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("docs/ui-phase1.png").createOutputStream()) {
            stream->setPosition(0);
            juce::PNGImageFormat png;
            check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f),*stream),"UI snapshot failed");
            check(stream->truncate().wasOk(),"UI snapshot truncation failed");
        }
        editor.reset(); restored.reset();
        check(!restored.meters.loudnessReady.load(),"reset did not clear LUFS readiness");
        // A transient must survive quieter later blocks until UI consumption.
        NkblimitProcessor metering; metering.prepareToPlay(48000,64);
        juce::AudioBuffer<float> signal(2,256); signal.clear(); signal.setSample(0,0,1.0f);
        metering.processBlock(signal,midi); signal.clear();
        for(int block=0;block<10;++block) { signal.clear(); metering.processBlock(signal,midi); }
        check(std::abs(metering.meters.input.exchange(-240))<0.001f,"interval input peak was overwritten");
        signal.clear(); metering.processBlock(signal,midi);
        check(metering.meters.input.load()<-120,"consumed peak was stale");
        metering.reset();
        // Compare final-output LUFS with an independently evaluated normative
        // 48 kHz filter response at 997 Hz; both channels carry the same tone.
        for(int block=0;block<600;++block) {
            for(int c=0;c<2;++c) for(int i=0;i<256;++i)
                signal.setSample(c,i,float(0.05*std::sin((block*256+i)*2*3.141592653589793*997/48000)));
            metering.processBlock(signal,midi);
        }
        check(metering.meters.loudnessReady.load(),"output LUFS never became ready");
        const auto z=std::exp(std::complex<double>(0,-2*3.141592653589793*997/48000));
        const auto shelf=(1.53512485958697-2.69169618940638*z+1.19839281085285*z*z)
                         /(1.0-1.69065929318241*z+0.73248077421585*z*z);
        const auto highpass=(1.0-2.0*z+z*z)/(1.0-1.99004745483398*z+0.99007225036621*z*z);
        const double expected=-0.691+10*std::log10(0.05*0.05*std::norm(shelf*highpass));
        check(std::abs(metering.meters.shortTermLufs.load()-expected)<0.05,"final-output LUFS mismatch");
        // Ready-state screenshot contains actual processed-tone readings.
        std::unique_ptr<juce::AudioProcessorEditor> readyEditor(metering.createEditor());
        if(auto stream=juce::File::getCurrentWorkingDirectory().getParentDirectory().getChildFile("docs/ui-metering.png").createOutputStream()) {
            stream->setPosition(0); juce::PNGImageFormat png;
            check(png.writeImageToStream(readyEditor->createComponentSnapshot(readyEditor->getLocalBounds(),true,2.0f),*stream),"ready UI snapshot failed");
            check(stream->truncate().wasOk(),"ready UI snapshot truncation failed");
        }
        readyEditor.reset();
        const auto previousEpoch=metering.meters.epoch.load(); metering.reset();
        check(metering.meters.epoch.load()!=previousEpoch && metering.meters.shortTermLufs.load()==-240,
              "meter reset did not publish epoch/silence");
        const char invalid[]="invalid"; restored.setStateInformation(invalid,7);
        std::cout << "PASS processor float/double, restore, OS latency, editor, interval peaks, final-output LUFS, meter reset\n";
    } catch(const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
