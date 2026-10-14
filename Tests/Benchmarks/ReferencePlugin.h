// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <iostream>
#include <iomanip>
inline float publicNormalizedValue(juce::AudioProcessorParameter& p,double engineering) {
    const auto matches=[&](float n) { return n>=0 && n<=1 &&
        std::abs(p.getText(n,256).getDoubleValue()-engineering)<.006; };
    if(matches(p.getValue())) return p.getValue();
    const float parsed=p.getValueForText(juce::String(engineering,4));
    if(matches(parsed)) return parsed;
    // Some legacy VST3 controllers return engineering values in their
    // string-to-normalised conversion. Find the public display-text mapping
    // instead, and require a successful display round-trip.
    const bool ascending=p.getText(.25f,256).getDoubleValue()<p.getText(.75f,256).getDoubleValue();
    float low=0,high=1;
    for(int i=0;i<32;++i) {
        const float mid=(low+high)*.5f;
        if(matches(mid)) return mid;
        const double value=p.getText(mid,256).getDoubleValue();
        if((value<engineering)==ascending) low=mid; else high=mid;
    }
    const float value=(low+high)*.5f;
    if(!matches(value)) throw std::runtime_error("reference public parameter conversion failed: " + p.getName(256).toStdString());
    return value;
}
inline void configureOzoneReference(juce::AudioPluginInstance& plugin,double drive) {
    int required=0;
    for(auto* p:plugin.getParameters()) {
        const auto name=p->getName(256);
        bool set=false;
        float value=0;
        if(name=="EQ: Bypass" || name=="IMG: Bypass") { value=1; set=true; }
        else if(name=="Global Bypass" || name=="MAX: Bypass" || name=="MAX: Enable Transient Shaping") { value=0; set=true; }
        else if(name=="MAX: Mode" || name=="MAX: True Peak Limiting") { value=1; set=true; ++required; }
        else if(name=="MAX: Threshold") { value=publicNormalizedValue(*p,-drive); set=true; ++required; }
        else if(name=="MAX: Ceiling") { value=publicNormalizedValue(*p,-.5); set=true; ++required; }
        else if(name=="MAX: Character") { value=publicNormalizedValue(*p,2); set=true; ++required; }
        else if(name=="MAX: Stereo Unlink Amt" || name=="MAX: Transient Shaping Amt") { value=publicNormalizedValue(*p,0); set=true; }
        else if(name.startsWith("GAIN:")) { value=publicNormalizedValue(*p,0); set=true; }
        if(set) {
            p->beginChangeGesture(); p->setValueNotifyingHost(value); p->endChangeGesture();
            std::cout << "REF_PARAM " << std::quoted(name.toStdString()) << '='
                << std::quoted(p->getText(p->getValue(),256).toStdString()) << " normalized=" << p->getValue() << '\n';
            if(name=="MAX: Threshold" && std::abs(p->getText(p->getValue(),256).getDoubleValue()+drive)>.01)
                throw std::runtime_error("reference threshold did not round-trip");
            if(name=="MAX: Ceiling" && std::abs(p->getText(p->getValue(),256).getDoubleValue()+.5)>.01)
                throw std::runtime_error("reference ceiling did not round-trip");
        }
    }
    if(required!=5) throw std::runtime_error("reference maximizer controls not found");
}
// Inspect public host parameters only. No commercial code or assets are read.
inline int inspectReferencePlugin(const juce::String& path) {
    juce::AudioPluginFormatManager formats;
    formats.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());
    juce::OwnedArray<juce::PluginDescription> descriptions;
    formats.getFormat(0)->findAllTypesForFile(descriptions,juce::File(path).getFullPathName());
    if(descriptions.isEmpty()) throw std::runtime_error("reference plugin scan failed");
    juce::String error;
    auto plugin=formats.createPluginInstance(*descriptions[0],48000,512,error);
    if(!plugin) throw std::runtime_error(error.toStdString());
    std::cout << "reference " << descriptions[0]->name << " version=" << descriptions[0]->version << '\n';
    for(int i=0;i<plugin->getParameters().size();++i) {
        auto* p=plugin->getParameters()[i];
        std::cout << i << '\t' << std::quoted(p->getName(256).toStdString()) << '\t'
            << std::quoted(p->getLabel().toStdString()) << '\t' << p->getValue() << '\t'
            << std::quoted(p->getText(p->getValue(),256).toStdString()) << '\t'
            << p->getNumSteps() << '\n';
    }
    return 0;
}
