#include "PluginProcessor.h"

namespace params = orbit::params;

OrbitAudioProcessor::OrbitAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", params::createLayout()) {
    for (int i = 0; i < orbit::OrbitEngine::kNumTaps; ++i) {
        tapParams_[static_cast<size_t>(i)].enabled  = apvts.getRawParameterValue(params::tapEnabledId(i));
        tapParams_[static_cast<size_t>(i)].time     = apvts.getRawParameterValue(params::tapTimeId(i));
        tapParams_[static_cast<size_t>(i)].sync     = apvts.getRawParameterValue(params::tapSyncId(i));
        tapParams_[static_cast<size_t>(i)].feedback = apvts.getRawParameterValue(params::tapFeedbackId(i));
        tapParams_[static_cast<size_t>(i)].reverse  = apvts.getRawParameterValue(params::tapReverseId(i));
        tapParams_[static_cast<size_t>(i)].pitch    = apvts.getRawParameterValue(params::tapPitchId(i));
    }
    dryWetParam_ = apvts.getRawParameterValue(params::kDryWetId);
    duckParam_   = apvts.getRawParameterValue(params::kDuckId);
    widthParam_    = apvts.getRawParameterValue(params::kWidthId);
    pingPongParam_ = apvts.getRawParameterValue(params::kPingPongId);
    characterModeParam_ = apvts.getRawParameterValue(params::kCharacterModeId);
    modDepthParam_ = apvts.getRawParameterValue(params::kModDepthId);
    modRateParam_  = apvts.getRawParameterValue(params::kModRateId);
    lowCutParam_   = apvts.getRawParameterValue(params::kLowCutId);
    highCutParam_  = apvts.getRawParameterValue(params::kHighCutId);
    freezeParam_   = apvts.getRawParameterValue(params::kFreezeId);

    // Production default user-preset directory (no override).
    presetManager_ = std::make_unique<orbit::PresetManager>(apvts);
}

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
        const auto& p = tapParams_[static_cast<size_t>(i)];
        orbit::TapSettings tap;
        tap.enabled = p.enabled->load() > 0.5f;
        tap.sync = static_cast<orbit::dsp::SyncDivision>(static_cast<int>(p.sync->load()));
        tap.timeSeconds = p.time->load() / 1000.0f;
        tap.feedback = p.feedback->load();
        tap.reverse = p.reverse->load() > 0.5f;
        tap.pitchSemitones = p.pitch->load();
        engine_.setTap(i, tap);
    }
    engine_.setDryWet(dryWetParam_->load());
    engine_.setDuckAmount(duckParam_->load());
    engine_.setWidth(widthParam_->load());
    engine_.setPingPong(pingPongParam_->load() > 0.5f);
    engine_.setCharacterMode(static_cast<orbit::dsp::CharacterStage::Mode>(
        static_cast<int>(characterModeParam_->load())));
    engine_.setModulation(modDepthParam_->load(), modRateParam_->load());
    engine_.setFilters(lowCutParam_->load(), highCutParam_->load());
    engine_.setFreeze(freezeParam_->load() > 0.5f);
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
    // Migration switch point: v1 is current. When stateVersion 2 exists,
    // transform older trees here before replaceState.
    const int loadedVersion = static_cast<int>(tree.getProperty("stateVersion", 1));
    juce::ignoreUnused(loadedVersion);
    apvts.replaceState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OrbitAudioProcessor();
}
