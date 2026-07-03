#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace orbit::params {

inline juce::String tapEnabledId(int tapIndex)  { return "tap" + juce::String(tapIndex + 1) + "_enabled"; }
inline juce::String tapTimeId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_time"; }
inline juce::String tapSyncId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_sync"; }
inline juce::String tapFeedbackId(int tapIndex) { return "tap" + juce::String(tapIndex + 1) + "_feedback"; }

inline constexpr auto kDryWetId = "mix_drywet";
inline constexpr auto kDuckId   = "mix_duck";

inline constexpr auto kModDepthId = "mod_depth";
inline constexpr auto kModRateId  = "mod_rate";
inline constexpr auto kLowCutId   = "filter_lowcut";
inline constexpr auto kHighCutId  = "filter_highcut";

// Index order MUST match orbit::dsp::SyncDivision (core/dsp/TempoSync.h).
inline const juce::StringArray kSyncChoices {
    "Free", "1/1", "1/2", "1/4", "1/8", "1/16",
    "1/4 D", "1/8 D", "1/4 T", "1/8 T"
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace orbit::params
