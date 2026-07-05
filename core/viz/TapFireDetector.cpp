#include "viz/TapFireDetector.h"
#include <algorithm>
#include <cmath>

namespace orbit::viz {

namespace {
float onePoleCoef(float seconds, double sampleRate) {
    // Same form as Ducker: fraction of the remaining distance per sample.
    return 1.0f - std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}
} // namespace

void TapFireDetector::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    attackCoef_ = onePoleCoef(kAttackSeconds, sampleRate);
    releaseCoef_ = onePoleCoef(kReleaseSeconds, sampleRate);
    intensityWindowSamples_ =
        static_cast<int>(kIntensityWindowSeconds * static_cast<float>(sampleRate));
    setTapDelaySeconds(0.0f);   // hold floors at kHoldMinSeconds until told otherwise
    reset();
}

void TapFireDetector::reset() {
    env_ = 0.0f;
    samplesSinceFire_ = -1;
    emitted_ = false;
    peakSinceFire_ = 0.0f;
    fireTime_ = 0;
}

void TapFireDetector::setTapDelaySeconds(float seconds) {
    const float hold = std::clamp(seconds * 0.5f, kHoldMinSeconds, kHoldMaxSeconds);
    holdSamples_ = static_cast<int>(hold * static_cast<float>(sampleRate_));
}

bool TapFireDetector::processSample(float level, std::uint64_t timeSamples,
                                    std::uint64_t& fireTimeSamples, float& intensity01) {
    const float prevEnv = env_;
    const float coef = level > env_ ? attackCoef_ : releaseCoef_;
    env_ += (level - env_) * coef;
    if (!(std::abs(env_) >= 1.0e-12f))   // denormal/NaN flush, house policy
        env_ = 0.0f;

    if (samplesSinceFire_ < 0) {                                  // idle/armed
        if (prevEnv < kThreshold && env_ >= kThreshold) {         // upward crossing
            samplesSinceFire_ = 0;
            emitted_ = false;
            peakSinceFire_ = level;
            fireTime_ = timeSamples;
        }
        return false;
    }

    ++samplesSinceFire_;
    if (!emitted_) {
        peakSinceFire_ = std::max(peakSinceFire_, level);
        const int windowEnd = std::min(intensityWindowSamples_, holdSamples_);
        if (samplesSinceFire_ >= windowEnd) {                     // window closes: emit
            emitted_ = true;
            fireTimeSamples = fireTime_;
            intensity01 = std::clamp(peakSinceFire_, 0.0f, 1.0f);
            return true;
        }
    }
    if (samplesSinceFire_ >= holdSamples_)
        samplesSinceFire_ = -1;                                   // hold over: re-arm
    return false;
}

} // namespace orbit::viz
