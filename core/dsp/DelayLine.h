#pragma once
#include <cstddef>
#include <vector>

namespace orbit::dsp {

// Single-channel fractional delay line with built-in feedback, click-free
// delay-time gliding, and an unsmoothed modulation offset for LFO use.
// Real-time safe after prepare(): processSample never allocates.
class DelayLine {
public:
    static constexpr float kMaxFeedback = 0.98f;
    static constexpr float kDefaultGlideSeconds = 0.05f;

    void prepare(double sampleRate, float maxDelaySeconds);
    void reset();

    // Snaps when called before processing starts (after prepare/reset);
    // glides (one-pole, kDefaultGlideSeconds) when called while running.
    void setDelaySeconds(float seconds);        // clamped to [one sample, maxDelaySeconds]
    void setFeedback(float amount);             // clamped to [0, kMaxFeedback]
    void setModulationSamples(float samples);   // additive read offset, unsmoothed (LFO path)

    // Reads the delayed sample, writes input + feedback, advances.
    // Call exactly once per sample.
    float processSample(float input);

private:
    float readFractional() const;

    std::vector<float> buffer_;
    std::size_t writePos_ = 0;
    double targetDelaySamples_ = 1.0;
    double currentDelaySamples_ = 1.0;
    double glideCoef_ = 0.0;
    float modSamples_ = 0.0f;
    float feedback_ = 0.0f;
    double sampleRate_ = 44100.0;
    bool running_ = false;
};

} // namespace orbit::dsp
