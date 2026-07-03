#include "Parameters.h"
#include "OrbitEngine.h"

namespace orbit::params {

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
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
    }

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDryWetId, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDuckId, 1), "Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    return { parameters.begin(), parameters.end() };
}

} // namespace orbit::params
