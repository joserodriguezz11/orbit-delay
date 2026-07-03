#include "dsp/CharacterStage.h"
#include <cmath>

namespace orbit::dsp {

namespace {
constexpr float kTapeDrive = 1.5f;
constexpr float kTapeCutoffHz = 7500.0f;
constexpr float kGritDrive = 1.8f;
constexpr float kGritCutoffHz = 3500.0f;
constexpr float kGritNoiseLevel = 0.003f;
} // namespace

void CharacterStage::prepare(double sampleRate) {
    tapeLp_.prepare(sampleRate, OnePole::Mode::LowPass);
    tapeLp_.setCutoff(kTapeCutoffHz);
    gritLp_.prepare(sampleRate, OnePole::Mode::LowPass);
    gritLp_.setCutoff(kGritCutoffHz);
    // Ducker coefficient formula, inlined: 10 ms attack / 200 ms release.
    attackCoef_  = std::exp(-1.0f / (0.010f * static_cast<float>(sampleRate)));
    releaseCoef_ = std::exp(-1.0f / (0.200f * static_cast<float>(sampleRate)));
    reset();
}

void CharacterStage::reset() {
    tapeLp_.reset();
    gritLp_.reset();
    envelope_ = 0.0f;
    rngState_ = kNoiseSeed;   // deterministic noise across resets
}

void CharacterStage::setMode(Mode mode) { mode_ = mode; }

float CharacterStage::nextNoise() {
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return static_cast<float>(rngState_) * (2.0f / 4294967296.0f) - 1.0f;
}

float CharacterStage::processSample(float input) {
    switch (mode_) {
    case Mode::Clean:
    default:
        return input;   // true bypass

    case Mode::Tape: {
        // Normalized by the drive (unity slope at 0) so low levels stay linear.
        const float sat = std::tanh(kTapeDrive * input) / kTapeDrive;
        return tapeLp_.processSample(sat);
    }

    case Mode::Grit: {
        const float sat =
            input / (1.0f + std::abs(kGritDrive * input)) * (1.0f + kGritDrive);
        const float filtered = gritLp_.processSample(sat);

        // Gated noise floor: 10 ms / 200 ms follower of |input| scales the
        // noise, so a silent input yields exactly zero output.
        float level = std::abs(input);
        if (!std::isfinite(level))
            level = 0.0f;
        const float coef = level > envelope_ ? attackCoef_ : releaseCoef_;
        envelope_ = coef * envelope_ + (1.0f - coef) * level;

        return filtered + nextNoise() * kGritNoiseLevel * envelope_;
    }
    }
}

} // namespace orbit::dsp
