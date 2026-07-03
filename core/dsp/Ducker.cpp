#include "dsp/Ducker.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void Ducker::prepare(double sampleRate) {
    attackCoef_  = std::exp(-1.0f / (0.005f * static_cast<float>(sampleRate)));   // 5 ms
    releaseCoef_ = std::exp(-1.0f / (0.250f * static_cast<float>(sampleRate)));   // 250 ms
    reset();
}

void Ducker::reset() { envelope_ = 0.0f; }

void Ducker::setAmount(float amount) { amount_ = std::clamp(amount, 0.0f, 1.0f); }

float Ducker::processGain(float sidechainLevel) {
    float level = std::abs(sidechainLevel);
    if (!std::isfinite(level))
        level = 0.0f;
    const float coef = level > envelope_ ? attackCoef_ : releaseCoef_;
    envelope_ = coef * envelope_ + (1.0f - coef) * level;
    const float duck = std::min(envelope_, 1.0f) * amount_;
    return 1.0f - duck;
}

} // namespace orbit::dsp
