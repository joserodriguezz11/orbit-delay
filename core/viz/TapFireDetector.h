#pragma once
#include <cstdint>

namespace orbit::viz {

// Detects audible repeats on one tap's wet output. Per sample, feed the
// tap's level (max |out| across channels; 0 while the tap is disabled).
// State machine: Idle -> (env crosses kThreshold upward) Measuring
// [kIntensityWindow, capped by hold end] -> emit -> Holding [until hold
// elapses since the crossing] -> Idle. Emitted intensity = peak level seen
// during the measuring window, clamped to [0,1]; emitted fireTime = the
// crossing sample. Envelope at the crossing is ~= the threshold, which is
// why intensity is measured over the window instead.
class TapFireDetector {
public:
    static constexpr float kThreshold = 0.01f;          // -40 dB
    static constexpr float kAttackSeconds = 0.001f;
    static constexpr float kReleaseSeconds = 0.05f;
    static constexpr float kIntensityWindowSeconds = 0.005f;
    static constexpr float kHoldMinSeconds = 0.02f;
    static constexpr float kHoldMaxSeconds = 0.25f;

    void prepare(double sampleRate);                    // resets
    void reset();
    void setTapDelaySeconds(float seconds);             // hold = seconds/2, clamped

    // Advance one sample. Returns true exactly when a fire event should be
    // emitted (measurement window just closed); then fills fireTimeSamples
    // (absolute time of the crossing) and intensity01.
    bool processSample(float level, std::uint64_t timeSamples,
                       std::uint64_t& fireTimeSamples, float& intensity01);

private:
    double sampleRate_ = 44100.0;
    float attackCoef_ = 1.0f, releaseCoef_ = 1.0f;
    float env_ = 0.0f;
    int holdSamples_ = 0, intensityWindowSamples_ = 0;
    int samplesSinceFire_ = -1;    // -1 = idle/armed; >=0 counts from crossing
    bool emitted_ = false;         // this fire's event already pushed?
    float peakSinceFire_ = 0.0f;
    std::uint64_t fireTime_ = 0;
};

} // namespace orbit::viz
