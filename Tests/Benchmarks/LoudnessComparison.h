// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include "Metering/LoudnessMeter.h"
#include "../DSPTests/ReferenceTruePeak.h"
#include <iostream>
#include <random>
#include <vector>
#include <numbers>
#include <iomanip>
// Loads the actual old and new bundles sequentially. Synthetic comparison only:
// not an Ozone comparison or a substitute for music listening tests.
inline int compareLoudnessBundles(const juce::String& oldBundle,const juce::String& newBundle) {
    juce::AudioPluginFormatManager manager;
    manager.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());
    struct Result { double lufs=0,peak=0; };
    auto measure = [&](const juce::String& bundle,int signal,double inputGain) -> Result {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        manager.getFormat(0)->findAllTypesForFile(descriptions,juce::File(bundle).getFullPathName());
        if(descriptions.size()!=1) throw std::runtime_error("comparison VST3 scan failed");
        juce::String error;
        auto instance=manager.createPluginInstance(*descriptions[0],48000,256,error);
        if(!instance) throw std::runtime_error(error.toStdString());
        for(auto* parameter:instance->getParameters()) {
            if(parameter->getName(128)=="Input Gain") parameter->setValueNotifyingHost(float((inputGain+12)/36));
            if(parameter->getName(128)=="Output Ceiling") parameter->setValueNotifyingHost(float(2.5/3));
            if(parameter->getName(128)=="Oversampling") parameter->setValueNotifyingHost(1.0f);
        }
        instance->setProcessingPrecision(juce::AudioProcessor::doublePrecision);
        instance->setRateAndBufferSizeDetails(48000,256); instance->prepareToPlay(48000,256);
        nkb::LoudnessMeter loudness; loudness.prepare(48000);
        juce::AudioBuffer<double> buffer(2,256); juce::MidiBuffer midi;
        std::vector<double> left,right; left.reserve(240000); right.reserve(240000);
        std::mt19937 random(42);
        for(int block=0;block<938;++block) {
            for(int i=0;i<256;++i) {
                const int sample=block*256+i; const double t=sample/48000.0;
                double l=0,r=0;
                if(signal==0) l=r=0.15*std::sin(2*std::numbers::pi*997*t);
                else {
                    const double noise=double(int(random()%20001)-10000)/10000;
                    const double kickPhase=std::fmod(t,0.5);
                    const double snarePhase=std::fmod(t+0.25,0.5);
                    const double kick=0.12*std::exp(-kickPhase*24)*std::sin(2*std::numbers::pi*62*t);
                    const double snare=0.065*std::exp(-snarePhase*40)*noise;
                    const double guitar=0.042*std::tanh(2.2*(std::sin(2*std::numbers::pi*220*t)+0.4*std::sin(2*std::numbers::pi*330*t)));
                    const double cymbal=0.006*std::exp(-std::fmod(t,0.125)*30)*noise;
                    l=kick+snare+guitar+cymbal;
                    r=kick+0.8*snare+guitar+1.1*cymbal;
                }
                buffer.setSample(0,i,l); buffer.setSample(1,i,r);
            }
            instance->processBlock(buffer,midi);
            for(int i=0;i<256;++i) {
                const auto l=buffer.getSample(0,i),r=buffer.getSample(1,i);
                loudness.push(l,r); left.push_back(l); right.push_back(r);
            }
        }
        const double measuredLufs=loudness.shortTermLufs();
        instance->releaseResources(); instance.reset();
        const double peak=std::max(referenceTruePeak(left),referenceTruePeak(right));
        return {measuredLufs,20*std::log10(peak)};
    };
    bool success=true;
    std::cout << std::fixed << std::setprecision(3);
    for(int signal:{0,1}) for(double drive:{16.0,24.0}) {
        const auto before=measure(oldBundle,signal,drive);
        const auto after=measure(newBundle,signal,drive);
        std::cout << "comparison " << (signal==0 ? "997Hz sine" : "synthetic drums/guitar")
            << " gain=" << drive << " old_LUFS-S=" << before.lufs << " new_LUFS-S=" << after.lufs
            << " difference_LU=" << after.lufs-before.lufs << " old_dBTP=" << before.peak
            << " new_dBTP=" << after.peak << '\n';
        if(after.peak > -0.4) success=false;
    }
    return success ? 0 : 1;
}
