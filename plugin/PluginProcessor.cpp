#include "PluginProcessor.h"

namespace params = orbit::params;

OrbitAudioProcessor::OrbitAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", params::createLayout()) {}

void OrbitAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine_.reset();
}

bool OrbitAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void OrbitAudioProcessor::updateEngineFromParameters() {
    for (int i = 0; i < orbit::OrbitEngine::kNumTaps; ++i) {
        orbit::TapSettings tap;
        tap.enabled = apvts.getRawParameterValue(params::tapEnabledId(i))->load() > 0.5f;
        tap.sync = static_cast<orbit::dsp::SyncDivision>(
            static_cast<int>(apvts.getRawParameterValue(params::tapSyncId(i))->load()));
        tap.timeSeconds = apvts.getRawParameterValue(params::tapTimeId(i))->load() / 1000.0f;
        tap.feedback = apvts.getRawParameterValue(params::tapFeedbackId(i))->load();
        engine_.setTap(i, tap);
    }
    engine_.setDryWet(apvts.getRawParameterValue(params::kDryWetId)->load());
    engine_.setDuckAmount(apvts.getRawParameterValue(params::kDuckId)->load());
}

void OrbitAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    if (auto* playhead = getPlayHead())
        if (const auto position = playhead->getPosition())
            if (const auto bpm = position->getBpm())
                engine_.setBpm(*bpm);

    updateEngineFromParameters();
    engine_.process(buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(), buffer.getNumSamples());
}

void OrbitAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Family-wide versioned state envelope (spec §4 product-family rules).
    auto state = apvts.copyState();
    state.setProperty("product", "orbit", nullptr);
    state.setProperty("stateVersion", 1, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void OrbitAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!tree.isValid())
        return;
    if (tree.hasProperty("product") && tree.getProperty("product").toString() != "orbit")
        return;   // refuse state from a different family product
    apvts.replaceState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OrbitAudioProcessor();
}
