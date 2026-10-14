// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include <iostream>
#include <cmath>
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
        const char invalid[]="invalid"; restored.setStateInformation(invalid,7);
        std::cout << "PASS processor float/double, parameter restore, constant OS latency, editor lifecycle, invalid state\n";
    } catch(const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
