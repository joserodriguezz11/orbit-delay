#include "dsp/OnePole.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void OnePole::prepare(double sampleRate, Mode mode) {
    sampleRate_ = sampleRate;
    mode_ = mode;
    reset();
}

void OnePole::reset() { state_ = 0.0f; }

void OnePole::setCutoff(float hz) {
    const float maxHz = static_cast<float>(0.45 * sampleRate_);
    hz = std::clamp(hz, 1.0f, maxHz);
    coef_ = 1.0f - std::exp(static_cast<float>(-2.0 * 3.14159265358979323846 * hz / sampleRate_));
}

float OnePole::processSample(float input) {
    state_ += coef_ * (input - state_);
    if (!(std::abs(state_) >= 1.0e-12f))   // denormal/NaN flush, matches DelayLine policy
        state_ = 0.0f;
    return mode_ == Mode::LowPass ? state_ : input - state_;
}

} // namespace orbit::dsp
