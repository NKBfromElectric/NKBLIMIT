// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"
// Both precision engines are owned here; processing never constructs DSP objects.
NkblimitProcessor::NkblimitProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
      .withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"NKBLIMIT",layout()) {
    static_assert(std::atomic<float>::is_always_lock_free);
    inputParam = state.getRawParameterValue("inputGain");
    ceilingParam = state.getRawParameterValue("ceiling");
    oversamplingParam = state.getRawParameterValue("oversampling");
}
juce::AudioProcessorValueTreeState::ParameterLayout NkblimitProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGain",1},"Input Gain",
        juce::NormalisableRange<float>(-12.0f,24.0f,0.01f),0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ceiling",1},"Output Ceiling",
        juce::NormalisableRange<float>(-3.0f,0.0f,0.01f),-0.5f,
        juce::AudioParameterFloatAttributes().withLabel("dBTP")));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"oversampling",1},
        "Oversampling",juce::StringArray{"4x","8x"},0));
    return p;
}
nkb::Parameters NkblimitProcessor::parameters() const noexcept {
    return {inputParam->load(std::memory_order_relaxed),ceilingParam->load(std::memory_order_relaxed),
            oversamplingParam->load(std::memory_order_relaxed) >= 0.5f};
}
void NkblimitProcessor::prepareToPlay(double rate,int block) {
    preparedRate = rate; preparedBlock = std::max(block,1);
    floatEngine.prepare(rate,preparedBlock,parameters());
    doubleEngine.prepare(rate,preparedBlock,parameters());
    jassert(floatEngine.latency() == doubleEngine.latency());
    setLatencySamples(floatEngine.latency());
    meters.input.store(-240); meters.output.store(-240); meters.truePeak.store(-240); meters.reduction.store(0);
}
void NkblimitProcessor::reset() {
    if (preparedRate > 0) { floatEngine.reset(parameters()); doubleEngine.reset(parameters()); }
}
bool NkblimitProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
template<typename T> void NkblimitProcessor::process(juce::AudioBuffer<T>& b,nkb::OversamplingEngine<T>& engine) {
    juce::ScopedNoDenormals noDenormals;
    if (preparedRate <= 0 || b.getNumChannels() != 2) { b.clear(); return; }
    engine.process(b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples(),parameters());
    const auto& m = engine.measurements;
    meters.input.store(float(nkb::gainToDb(m.input)),std::memory_order_relaxed);
    meters.output.store(float(nkb::gainToDb(m.output)),std::memory_order_relaxed);
    meters.truePeak.store(float(nkb::gainToDb(m.truePeak)),std::memory_order_relaxed);
    meters.reduction.store(float(m.reductionDb),std::memory_order_relaxed);
}
void NkblimitProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) { midi.clear(); process(b,floatEngine); }
void NkblimitProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer& midi) { midi.clear(); process(b,doubleEngine); }
juce::AudioProcessorEditor* NkblimitProcessor::createEditor() { return new NkblimitEditor(*this); }
void NkblimitProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto tree = state.copyState(); tree.setProperty("schemaVersion",1,nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml,data);
}
void NkblimitProcessor::setStateInformation(const void* data,int size) {
    if (auto xml = getXmlFromBinary(data,size))
        if (xml->hasTagName(state.state.getType()) && xml->getIntAttribute("schemaVersion",1) == 1)
            state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NkblimitProcessor(); }
