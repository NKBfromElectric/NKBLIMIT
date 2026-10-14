// SPDX-License-Identifier: AGPL-3.0-only
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <iostream>
#include <cmath>
#include "../Benchmarks/LoudnessComparison.h"
#include "../Benchmarks/RealAudioComparison.h"
void check(bool passed,const char* message) { if(!passed) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        if(argc==4 && juce::String(argv[1])=="--compare") return compareLoudnessBundles(argv[2],argv[3]);
        if(argc==4 && juce::String(argv[1])=="--music") return compareRealAudio(argv[2],argv[3]);
        check(argc==2,"expected VST3 bundle path");
        juce::AudioPluginFormatManager manager;
        manager.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());
        juce::OwnedArray<juce::PluginDescription> descriptions;
        const auto bundle=juce::File(juce::String(argv[1])).getFullPathName();
        manager.getFormat(0)->findAllTypesForFile(descriptions,bundle);
        check(descriptions.size()==1,"VST3 scan failed");
        check(descriptions[0]->name=="nkblimit","wrong product name");
        juce::String error;
        auto instance=manager.createPluginInstance(*descriptions[0],48000,256,error);
        if(!instance) throw std::runtime_error(error.toStdString());
        check(instance->getTotalNumInputChannels()==2 && instance->getTotalNumOutputChannels()==2,"wrong buses");
        check(instance->supportsDoublePrecisionProcessing(),"VST3 does not advertise double precision");
        // Includes the wrapper's host bypass parameter; original DSP has 3 controls.
        check(instance->getParameters().size()>=3,"parameters missing");
        for(auto* parameter:instance->getParameters())
            if(parameter->getName(128)=="Input Gain") { parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0.5f); parameter->endChangeGesture(); }
        juce::MemoryBlock state; instance->getStateInformation(state);
        check(state.getSize()>0,"empty VST3 state"); instance->setStateInformation(state.getData(),int(state.getSize()));
        for(double rate:{44100.,48000.,88200.,96000.,192000.}) {
            for(bool doublePrecision:{false,true}) {
                instance->setProcessingPrecision(doublePrecision ? juce::AudioProcessor::doublePrecision : juce::AudioProcessor::singlePrecision);
                instance->setRateAndBufferSizeDetails(rate,256); instance->prepareToPlay(rate,256);
                check(instance->getLatencySamples()>0,"VST3 latency missing");
                juce::AudioBuffer<float> f(2,256); juce::AudioBuffer<double> d(2,256); juce::MidiBuffer midi;
                double outputPeak=0;
                for(int block=0;block<80;++block) {
                    for(int c=0;c<2;++c) for(int i=0;i<256;++i) {
                        const double x=1.2*std::sin((block*256+i)*2*3.141592653589793*997/rate);
                        f.setSample(c,i,float(x)); d.setSample(c,i,x);
                    }
                    if(doublePrecision) instance->processBlock(d,midi); else instance->processBlock(f,midi);
                    for(int c=0;c<2;++c) for(int i=0;i<256;++i) {
                        const double x=doublePrecision ? d.getSample(c,i) : f.getSample(c,i);
                        check(std::isfinite(x),"nonfinite VST3 output"); outputPeak=std::max(outputPeak,std::abs(x));
                    }
                }
                check(outputPeak>0.05 && outputPeak<1.001,"VST3 did not limit real audio");
                instance->releaseResources();
                std::cout << "VST3 rate=" << rate << " double=" << doublePrecision << " latency="
                    << instance->getLatencySamples() << " PASS\n";
            }
        }
        instance.reset();
        std::cout << "PASS actual VST3 scan/load/process, host gestures, state, float/double, unload\n";
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
