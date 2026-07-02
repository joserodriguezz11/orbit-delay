#include "dsp/DelayLine.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void DelayLine::prepare(double sampleRate, float maxDelaySeconds) {
    sampleRate_ = sampleRate;
    const auto maxSamples =
        static_cast<std::size_t>(std::ceil(sampleRate * maxDelaySeconds)) + 2;
    buffer_.assign(maxSamples, 0.0f);
    writePos_ = 0;
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_ = 0;
}

void DelayLine::setDelaySeconds(float seconds) {
    const float maxDelay =
        static_cast<float>(buffer_.size() - 2) / static_cast<float>(sampleRate_);
    seconds = std::clamp(seconds, 0.0f, maxDelay);
    delaySamples_ = std::max(1.0f, seconds * static_cast<float>(sampleRate_));
}

void DelayLine::setFeedback(float amount) {
    feedback_ = std::clamp(amount, 0.0f, kMaxFeedback);
}

float DelayLine::readFractional() const {
    const auto size = static_cast<float>(buffer_.size());
    float readPos = static_cast<float>(writePos_) - delaySamples_;
    while (readPos < 0.0f)
        readPos += size;
    const auto i0 = static_cast<std::size_t>(readPos) % buffer_.size();
    const auto i1 = (i0 + 1) % buffer_.size();
    const float frac = readPos - std::floor(readPos);
    return buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);
}

float DelayLine::processSample(float input) {
    const float out = readFractional();
    float next = input + out * feedback_;
    // Flush denormal-range values to hard zero so feedback tails die cleanly.
    if (std::abs(next) < 1.0e-12f)
        next = 0.0f;
    buffer_[writePos_] = next;
    writePos_ = (writePos_ + 1) % buffer_.size();
    return out;
}

} // namespace orbit::dsp
