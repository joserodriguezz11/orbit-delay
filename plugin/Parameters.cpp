#include "Parameters.h"
#include "OrbitEngine.h"

namespace orbit::params {

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    // Enum-order contract: kSyncChoices is built from kSyncDivisionNames and
    // enforced at compile time by the static_assert in core/dsp/TempoSync.h.
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    for (int i = 0; i < orbit::OrbitEngine::kNumTaps; ++i) {
        const auto n = juce::String(i + 1);
        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(tapEnabledId(i), 1), "Tap " + n + " On", i == 0));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(tapTimeId(i), 1), "Tap " + n + " Time",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 0.0f, 0.35f), 350.0f));
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(tapSyncId(i), 1), "Tap " + n + " Sync", kSyncChoices, 0));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(tapFeedbackId(i), 1), "Tap " + n + " Feedback",
            juce::NormalisableRange<float>(0.0f, 0.98f), 0.35f));
        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(tapReverseId(i), 1), "Tap " + n + " Reverse", false));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(tapPitchId(i), 1), "Tap " + n + " Pitch",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f));   // semitone snap
    }

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDryWetId, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDuckId, 1), "Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kWidthId, 1), "Width",
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(kPingPongId, 1), "Ping-Pong", false));

    // Enum-order contract: kCharacterChoices is built from kCharacterModeNames
    // and enforced at compile time by the static_assert in CharacterStage.h.
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(kCharacterModeId, 1), "Character", kCharacterChoices, 0));

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kModDepthId, 1), "Motion Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kModRateId, 1), "Motion Rate",
        juce::NormalisableRange<float>(0.1f, 8.0f, 0.0f, 0.5f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kLowCutId, 1), "Low Cut",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 0.0f, 0.4f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kHighCutId, 1), "High Cut",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.4f), 20000.0f));

    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(kFreezeId, 1), "Freeze", false));

    return { parameters.begin(), parameters.end() };
}

} // namespace orbit::params
