#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/CharacterStage.h"
#include "dsp/TempoSync.h"

namespace orbit::params {

inline juce::String tapEnabledId(int tapIndex)  { return "tap" + juce::String(tapIndex + 1) + "_enabled"; }
inline juce::String tapTimeId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_time"; }
inline juce::String tapSyncId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_sync"; }
inline juce::String tapFeedbackId(int tapIndex) { return "tap" + juce::String(tapIndex + 1) + "_feedback"; }
inline juce::String tapReverseId(int tapIndex)  { return "tap" + juce::String(tapIndex + 1) + "_reverse"; }
inline juce::String tapPitchId(int tapIndex)    { return "tap" + juce::String(tapIndex + 1) + "_pitch"; }

inline constexpr auto kDryWetId   = "mix_drywet";
inline constexpr auto kDuckId     = "mix_duck";
inline constexpr auto kWidthId    = "mix_width";
inline constexpr auto kPingPongId = "mix_pingpong";

inline constexpr auto kCharacterModeId = "character_mode";
inline constexpr auto kFreezeId        = "freeze_active";

inline constexpr auto kModDepthId = "mod_depth";
inline constexpr auto kModRateId  = "mod_rate";
inline constexpr auto kLowCutId   = "filter_lowcut";
inline constexpr auto kHighCutId  = "filter_highcut";

// Built from orbit::dsp::kSyncDivisionNames so the index order matches
// orbit::dsp::SyncDivision by construction (static_assert in TempoSync.h).
inline const juce::StringArray kSyncChoices = [] {
    juce::StringArray choices;
    for (auto* name : orbit::dsp::kSyncDivisionNames)
        choices.add(name);
    return choices;
}();

// Built from orbit::dsp::kCharacterModeNames so the index order matches
// orbit::dsp::CharacterStage::Mode by construction (static_assert in
// CharacterStage.h).
inline const juce::StringArray kCharacterChoices = [] {
    juce::StringArray choices;
    for (auto* name : orbit::dsp::kCharacterModeNames)
        choices.add(name);
    return choices;
}();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace orbit::params
