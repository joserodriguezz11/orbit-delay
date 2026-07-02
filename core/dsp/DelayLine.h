#pragma once
#include <cstddef>
#include <vector>

namespace orbit::dsp {

// Single-channel fractional delay line with built-in feedback.
// Real-time safe after prepare(): processSample never allocates.
class DelayLine {
public:
    static constexpr float kMaxFeedback = 0.98f;

    void prepare(double sampleRate, float maxDelaySeconds);
    void reset();

    void setDelaySeconds(float seconds);   // clamped to [one sample, maxDelaySeconds]
    void setFeedback(float amount);        // clamped to [0, kMaxFeedback]

    // Reads the delayed sample, writes input + feedback into the line,
    // advances the write head. Call exactly once per sample.
    float processSample(float input);

private:
    float readFractional() const;

    std::vector<float> buffer_;
    std::size_t writePos_ = 0;
    float delaySamples_ = 0.0f;
    float feedback_ = 0.0f;
    double sampleRate_ = 44100.0;
};

} // namespace orbit::dsp
