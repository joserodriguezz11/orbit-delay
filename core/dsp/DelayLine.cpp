#include "dsp/DelayLine.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void DelayLine::prepare(double sampleRate, float maxDelaySeconds) {
    sampleRate_ = sampleRate;
    const auto maxSamples =
        static_cast<std::size_t>(std::ceil(sampleRate * maxDelaySeconds)) + 2;
    buffer_.assign(maxSamples, 0.0f);
    glideCoef_ = std::exp(-1.0 / (static_cast<double>(kDefaultGlideSeconds) * sampleRate));
    writePos_ = 0;
    targetDelaySamples_ = 1.0;
    currentDelaySamples_ = 1.0;
    modSamples_ = 0.0f;
    running_ = false;
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_ = 0;
    currentDelaySamples_ = targetDelaySamples_;
    running_ = false;
}

void DelayLine::setDelaySeconds(float seconds) {
    const double maxDelay =
        static_cast<double>(buffer_.size() - 2) / sampleRate_;
    const double clamped = std::clamp(static_cast<double>(seconds), 0.0, maxDelay);
    targetDelaySamples_ = std::max(1.0, clamped * sampleRate_);
    if (!running_)
        currentDelaySamples_ = targetDelaySamples_;   // snap during configuration
}

void DelayLine::setFeedback(float amount) {
    feedback_ = std::clamp(amount, 0.0f, kMaxFeedback);
}

void DelayLine::setModulationSamples(float samples) {
    modSamples_ = samples;
}

float DelayLine::readFractional() const {
    const std::size_t size = buffer_.size();
    const double maxDelay = static_cast<double>(size - 2);
    // INVARIANT (do not weaken): effective is clamped to [1, size-2] so that
    // `newer` trails the write head by >= 1 sample and `older` = newer-1 stays
    // in valid history given the +2 headroom allocated in prepare(). All read
    // modes added later must preserve this bound.
    const double effective = std::clamp(
        currentDelaySamples_ + static_cast<double>(modSamples_), 1.0, maxDelay);
    const auto intDelay = static_cast<std::size_t>(effective);
    const float frac = static_cast<float>(effective - static_cast<double>(intDelay));
    const std::size_t newer = (writePos_ + size - intDelay) % size;
    const std::size_t older = (newer + size - 1) % size;
    return buffer_[newer] * (1.0f - frac) + buffer_[older] * frac;
}

float DelayLine::processSample(float input) {
    running_ = true;
    currentDelaySamples_ =
        targetDelaySamples_ + (currentDelaySamples_ - targetDelaySamples_) * glideCoef_;
    const float out = readFractional();
    float next = input + out * feedback_;
    // Flush denormal-range and non-finite values to hard zero so feedback
    // tails die cleanly and a bad input sample can't poison the line.
    if (!std::isfinite(next) || std::abs(next) < 1.0e-12f)
        next = 0.0f;
    buffer_[writePos_] = next;
    writePos_ = (writePos_ + 1) % buffer_.size();
    return out;
}

} // namespace orbit::dsp
