// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "LoudnessComparison.h"
#include <chrono>
#include <fstream>
// User-provided audio is read from an environment variable and never placed in
// the repository. This host-side test is not part of the audio callback.
inline int compareRealAudio(const juce::String& oldBundle,const juce::String& newBundle) {
    const auto audioPath=juce::SystemStats::getEnvironmentVariable("NKBLIMIT_TEST_AUDIO",{});
    const auto renderDirectory=juce::SystemStats::getEnvironmentVariable("NKBLIMIT_TEST_RENDER_DIR",{});
    const auto startText=juce::SystemStats::getEnvironmentVariable("NKBLIMIT_TEST_START_SECONDS","60");
    juce::WavAudioFormat wave;
    auto sourceStream=juce::File(audioPath).createInputStream();
    if(!sourceStream) throw std::runtime_error("could not read test audio");
    std::unique_ptr<juce::AudioFormatReader> reader(wave.createReaderFor(sourceStream.release(),true));
    if(!reader || reader->numChannels!=2 || reader->lengthInSamples>std::numeric_limits<int>::max())
        throw std::runtime_error("expected stereo test WAV under int sample limit");
    const int length=int(reader->lengthInSamples);
    const double rate=reader->sampleRate;
    juce::AudioBuffer<float> source(2,length); reader->read(&source,0,length,0,true,true);
    const int excerptStart=std::clamp(int(startText.getDoubleValue()*rate),0,std::max(0,length-int(30*rate)));
    const int excerptLength=std::min(int(30*rate),length-excerptStart);
    std::cout << "real_audio seconds=" << length/rate << " rate=" << rate
              << " excerpt_start=" << excerptStart/rate << " excerpt_seconds=" << excerptLength/rate << '\n' << std::flush;
    juce::AudioPluginFormatManager manager; manager.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());
    struct Result { double maximum,median,peak; };
    auto render=[&](const juce::String& bundle,const char* label,double drive) -> Result {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        manager.getFormat(0)->findAllTypesForFile(descriptions,juce::File(bundle).getFullPathName());
        if(descriptions.size()!=1) throw std::runtime_error("real audio VST3 scan failed");
        juce::String error; auto plugin=manager.createPluginInstance(*descriptions[0],rate,512,error);
        if(!plugin) throw std::runtime_error(error.toStdString());
        for(auto* p:plugin->getParameters()) {
            if(p->getName(128)=="Input Gain") p->setValueNotifyingHost(float((drive+12)/36));
            if(p->getName(128)=="Output Ceiling") p->setValueNotifyingHost(float(2.5/3));
            if(p->getName(128)=="Oversampling") p->setValueNotifyingHost(1.0f);
        }
        plugin->setProcessingPrecision(juce::AudioProcessor::doublePrecision);
        plugin->setRateAndBufferSizeDetails(rate,512); plugin->prepareToPlay(rate,512);
        const int latency=plugin->getLatencySamples();
        nkb::LoudnessMeter meter; meter.prepare(rate);
        std::vector<double> left,right,levels; left.reserve(std::size_t(length+latency+256)); right.reserve(left.capacity());
        juce::AudioBuffer<double> buffer(2,512); juce::MidiBuffer midi;
        const auto began=std::chrono::steady_clock::now();
        double maxLufs=-240; int nextReadout=int(3*rate),nextProgress=int(30*rate);
        for(int offset=0;offset<length+latency+256;offset+=512) {
            const int n=std::min(512,length+latency+256-offset);
            buffer.setSize(2,n,false,false,true); buffer.clear();
            for(int c=0;c<2;++c) for(int i=0;i<n;++i)
                if(offset+i<length) buffer.setSample(c,i,source.getSample(c,offset+i));
            plugin->processBlock(buffer,midi);
            for(int i=0;i<n;++i) {
                const double l=buffer.getSample(0,i),r=buffer.getSample(1,i);
                if(!std::isfinite(l)||!std::isfinite(r)) throw std::runtime_error("nonfinite music output");
                meter.push(l,r); left.push_back(l); right.push_back(r);
            }
            if(offset+n>=nextReadout) {
                const double level=meter.shortTermLufs(); levels.push_back(level); maxLufs=std::max(maxLufs,level);
                nextReadout+=int(rate*0.1);
            }
            if(offset+n>=nextProgress) {
                std::cout << label << " drive=" << drive << " rendered_seconds=" << (offset+n)/rate << '\n' << std::flush;
                nextProgress+=int(30*rate);
            }
        }
        plugin->releaseResources(); plugin.reset();
        if(!renderDirectory.isEmpty()) {
            const auto folder=juce::File(renderDirectory); folder.createDirectory();
            auto file=folder.getChildFile(juce::String(label)+"-gain"+juce::String(int(drive))+"dB-30sec.wav");
            file.deleteFile(); std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
            if(!stream) throw std::runtime_error("excerpt output stream failed");
            auto writer=wave.createWriterFor(stream,juce::AudioFormatWriterOptions()
                .withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
            if(!writer) throw std::runtime_error("excerpt WAV writer failed");
            juce::AudioBuffer<float> excerpt(2,excerptLength);
            for(int i=0;i<excerptLength;++i) {
                excerpt.setSample(0,i,float(left[std::size_t(excerptStart+latency+i)]));
                excerpt.setSample(1,i,float(right[std::size_t(excerptStart+latency+i)]));
            }
            if(!writer->writeFromAudioSampleBuffer(excerpt,0,excerptLength)) throw std::runtime_error("excerpt write failed");
        }
        std::sort(levels.begin(),levels.end());
        const double median=levels[levels.size()/2];
        std::cout << label << " drive=" << drive << " analyzing_independent_16x_true_peak\n" << std::flush;
        const double peak=20*std::log10(std::max(referenceTruePeak(left),referenceTruePeak(right)));
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
        std::cout << label << " drive=" << drive << " max_LUFS-S=" << maxLufs << " median_LUFS-S=" << median
                  << " dBTP=" << peak << " elapsed_s=" << seconds << '\n' << std::flush;
        return {maxLufs,median,peak};
    };
    bool success=true;
    for(double drive:{16.,24.}) {
        const auto old=render(oldBundle,"old",drive),updated=render(newBundle,"updated",drive);
        std::cout << "REAL_RESULT drive=" << drive << " old_max_LUFS-S=" << old.maximum << " new_max_LUFS-S=" << updated.maximum
            << " old_median_LUFS-S=" << old.median << " new_median_LUFS-S=" << updated.median
            << " median_gain_LU=" << updated.median-old.median << " old_dBTP=" << old.peak << " new_dBTP=" << updated.peak << '\n' << std::flush;
        if(updated.peak> -0.4) success=false;
    }
    return success ? 0 : 1;
}
