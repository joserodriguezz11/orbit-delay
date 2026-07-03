#pragma once

#include <cstdint>
#include "dsp/OnePole.h"

namespace orbit::dsp {

// Wet-path coloring stage: Clean (bypass), Tape (soft tanh saturation + LP),
// Grit (hard-knee soft-clip + darker LP + level-gated noise floor).
// Per-channel instance; processSample is RT-safe (no allocation, xorshift +
// arithmetic only). Enum order is a contract with the character_mode
// parameter choice (Task 6) — do not reorder.
class CharacterStage {
public:
    enum class Mode { Clean = 0, Tape, Grit, NumModes };

    void prepare(double sampleRate);
    void reset();
    void setMode(Mode mode);
    float processSample(float input);

private:
    float nextNoise();   // xorshift32 mapped to [-1, 1], deterministic seed

    Mode mode_ = Mode::Clean;
    OnePole tapeLp_;     // 7500 Hz low-pass after tape saturation
    OnePole gritLp_;     // 3500 Hz low-pass after grit soft-clip

    // Gated-noise envelope follower (Ducker coefficient formula, inlined).
    float envelope_ = 0.0f;
    float attackCoef_ = 0.0f;    // 10 ms
    float releaseCoef_ = 0.0f;   // 200 ms

    std::uint32_t rngState_ = kNoiseSeed;
    static constexpr std::uint32_t kNoiseSeed = 0x9E3779B9u;
};

} // namespace orbit::dsp
