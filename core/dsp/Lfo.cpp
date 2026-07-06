#include "dsp/Lfo.h"
#include "dsp/Constants.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void Lfo::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reset();
}

void Lfo::reset() { phase_ = 0.0; }

void Lfo::setRate(float hz) {
    const double clamped = std::clamp(static_cast<double>(hz), 0.01, 20.0);
    increment_ = clamped / sampleRate_;
}

float Lfo::processSample() {
    const float value =
        static_cast<float>(std::sin(2.0 * kPi * phase_));
    phase_ += increment_;
    if (phase_ >= 1.0)
        phase_ -= 1.0;
    return value;
}

} // namespace orbit::dsp
