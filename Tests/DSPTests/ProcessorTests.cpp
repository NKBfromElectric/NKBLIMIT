// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include <iostream>
#include <cmath>
#include <complex>
void check(bool x,const char* description) { if (!x) throw std::runtime_error(description); }
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
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
