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
    reverseCounter_ = 0;
    grainPhase_ = 0.0;
    updateModeGeometry();
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_ = 0;
    currentDelaySamples_ = targetDelaySamples_;
    running_ = false;
    reverseCounter_ = 0;
    grainPhase_ = 0.0;
}

void DelayLine::setDelaySeconds(float seconds) {
    const double maxDelay =
        static_cast<double>(buffer_.size() - 2) / sampleRate_;
    const double clamped = std::clamp(static_cast<double>(seconds), 0.0, maxDelay);
    targetDelaySamples_ = std::max(1.0, clamped * sampleRate_);
    if (!running_)
        currentDelaySamples_ = targetDelaySamples_;   // snap during configuration
    updateModeGeometry();
}

void DelayLine::setReadMode(ReadMode mode) {
    // Accepted v1 behavior: toggling Reverse mid-stream jumps the read head
    // (~one delay length), so a click is expected at the switch. A mode-switch
    // crossfade is a known Wave-3 candidate.
    if (mode == readMode_)
        return;
    readMode_ = mode;
    reverseCounter_ = 0;   // new mode starts at a chunk boundary
    updateModeGeometry();
}

void DelayLine::setPitchSemitones(float semitones) {
    pitchSemitones_ = std::clamp(semitones, -12.0f, 12.0f);
    // exp2(0) == 1.0 exactly, so semitones == 0 re-enables the Normal fast path.
    pitchRatio_ = std::exp2(static_cast<double>(pitchSemitones_) / 12.0);
    updateModeGeometry();
}

void DelayLine::updateModeGeometry() {
    // Recomputed on configuration changes only — never per sample.
    if (buffer_.size() < 4)
        return;   // not prepared yet
    const auto rounded = static_cast<std::size_t>(std::llround(targetDelaySamples_));
    const std::size_t minChunk = 256;
    const std::size_t maxChunk = std::max(minChunk, (buffer_.size() - 2) / 2);
    reverseChunkLen_ = std::clamp(rounded, minChunk, maxChunk);
    if (reverseCounter_ >= reverseChunkLen_)
        reverseCounter_ = 0;
    const double minWindow = 256.0;
    const double maxWindow = std::max(minWindow, 0.1 * sampleRate_);
    pitchWindowSamples_ = std::clamp(targetDelaySamples_, minWindow, maxWindow);
}

void DelayLine::setFeedback(float amount) {
    feedback_ = std::clamp(amount, 0.0f, kMaxFeedback);
}

void DelayLine::setModulationSamples(float samples) {
    modSamples_ = samples;
}

float DelayLine::readAt(double effectiveDelay) const {
    const std::size_t size = buffer_.size();
    const double maxDelay = static_cast<double>(size - 2);
    // INVARIANT (do not weaken): effective is clamped to [1, size-2] so that
    // `newer` trails the write head by >= 1 sample and `older` = newer-1 stays
    // in valid history given the +2 headroom allocated in prepare(). All read
    // modes must go through this clamp.
    const double effective = std::clamp(effectiveDelay, 1.0, maxDelay);
    const auto intDelay = static_cast<std::size_t>(effective);
    const float frac = static_cast<float>(effective - static_cast<double>(intDelay));
    const std::size_t newer = (writePos_ + size - intDelay) % size;
    const std::size_t older = (newer + size - 1) % size;
    return buffer_[newer] * (1.0f - frac) + buffer_[older] * frac;
}

float DelayLine::readFractional() const {
    return readAt(currentDelaySamples_ + static_cast<double>(modSamples_));
}

float DelayLine::readReverse() const {
    // Chunked backward playback: within a chunk of length L the counter j
    // maps to effective delay d(j) = 2j + 1, so the read head sweeps the
    // just-written chunk backwards while the write head moves forward.
    // Glide/mod offsets are ignored — chunk timing is authoritative.
    // With feedback, reverse repeats alternate direction (each pass re-reverses
    // the previous one) — accepted, musical behavior (plan-documented).
    const double j = static_cast<double>(reverseCounter_);
    const float newer = readAt(2.0 * j + 1.0);
    if (reverseCounter_ >= static_cast<std::size_t>(kReverseCrossfadeSamples))
        return newer;
    // First C samples of a chunk: crossfade linearly from the OLD trajectory
    // continued (d = 2(L + j) + 1) into the new chunk — click-free boundary.
    const double continued = 2.0 * (static_cast<double>(reverseChunkLen_) + j) + 1.0;
    const float older = readAt(continued);
    const float wNew = static_cast<float>(j) / static_cast<float>(kReverseCrossfadeSamples);
    return older * (1.0f - wNew) + newer * wNew;
}

float DelayLine::readPitched() const {
    // Dual-head grain reader: two heads half a window apart in phase, each
    // adding e_i = p_i * W of extra delay on top of the glide/mod position.
    // Triangular weights silence a head exactly where its phase wraps.
    const double base = currentDelaySamples_ + static_cast<double>(modSamples_);
    const double p0 = grainPhase_;
    double p1 = p0 + 0.5;
    if (p1 >= 1.0)
        p1 -= 1.0;
    float w0 = 1.0f - std::abs(2.0f * static_cast<float>(p0) - 1.0f);
    float w1 = 1.0f - std::abs(2.0f * static_cast<float>(p1) - 1.0f);
    const float sum = w0 + w1;
    if (sum > 1.0e-6f) {   // complementary phases keep sum ~1; normalize defensively
        w0 /= sum;
        w1 /= sum;
    } else {
        w0 = w1 = 0.5f;
    }
    return w0 * readAt(base + p0 * pitchWindowSamples_)
         + w1 * readAt(base + p1 * pitchWindowSamples_);
}

float DelayLine::read() const {
    // Normal + no pitch is the exact pre-existing code path (bit-identical).
    if (readMode_ == ReadMode::Normal && pitchRatio_ == 1.0)
        return readFractional();
    if (readMode_ == ReadMode::Reverse)   // pitch is ignored while Reverse
        return readReverse();
    return readPitched();
}

void DelayLine::writeAndAdvance(float value) {
    running_ = true;
    // Glide advances here (after the read) — one-sample phase shift in glide
    // trajectories vs. the pre-split code; constant-target output is identical.
    currentDelaySamples_ =
        targetDelaySamples_ + (currentDelaySamples_ - targetDelaySamples_) * glideCoef_;
    // Read-mode state advances here — read() stays const and side-effect free.
    if (readMode_ == ReadMode::Reverse) {
        if (++reverseCounter_ >= reverseChunkLen_)
            reverseCounter_ = 0;
    } else if (pitchRatio_ != 1.0) {
        grainPhase_ += (1.0 - pitchRatio_) / pitchWindowSamples_;
        // Wrap into [0, 1) for either delta direction. On a negative delta the
        // floor-wrap can transiently land exactly on 1.0 (e.g. -eps -> 1.0 - eps
        // rounding up); that's harmless — readAt clamps, and the next advance
        // re-wraps it into range.
        grainPhase_ -= std::floor(grainPhase_);
    }
    // Flush denormal-range and non-finite values to hard zero so feedback
    // tails die cleanly and a bad input sample can't poison the line.
    if (!std::isfinite(value) || std::abs(value) < 1.0e-12f)
        value = 0.0f;
    buffer_[writePos_] = value;
    writePos_ = (writePos_ + 1) % buffer_.size();
}

float DelayLine::processSample(float input) {
    const float out = read();
    writeAndAdvance(input + out * feedback_);
    return out;
}

} // namespace orbit::dsp
