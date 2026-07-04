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
    static constexpr int kReverseCrossfadeSamples = 128;

    // Normal reads at the (glided + modulated) delay; Reverse plays the
    // just-written history backwards in chunks (glide/mod offsets ignored).
    enum class ReadMode { Normal = 0, Reverse };

    void prepare(double sampleRate, float maxDelaySeconds);
    void reset();

    void setReadMode(ReadMode mode);
    void setPitchSemitones(float semitones);    // clamped [-12, +12]; 0 disables; ignored while Reverse

    // Snaps when called before processing starts (after prepare/reset);
    // glides (one-pole, kDefaultGlideSeconds) when called while running.
    void setDelaySeconds(float seconds);        // clamped to [one sample, maxDelaySeconds]
    void setFeedback(float amount);             // clamped to [0, kMaxFeedback]
    void setModulationSamples(float samples);   // additive read offset, unsmoothed (LFO path)

    // Split per-sample API (for cross-feedback topologies: read all lines
    // first, then write). Per sample, call read() at most once before exactly
    // one writeAndAdvance().
    //   read()            — delayed sample at the current effective position
    //                       (glide + modulation applied); does NOT advance.
    //   writeAndAdvance() — owns the per-sample state update: flushes NaN/
    //                       denormals, writes the value, advances the write
    //                       head AND the glide state, and sets running.
    // Note: the caller applies feedback itself (value = input + out * fb).
    float read() const;
    void writeAndAdvance(float value);

    // Reads the delayed sample, writes input + feedback, advances.
    // Call exactly once per sample. Exactly equivalent to
    // { out = read(); writeAndAdvance(input + out * feedback()); }.
    float processSample(float input);

    float feedback() const { return feedback_; }

private:
    float readFractional() const;
    float readAt(double effectiveDelay) const;   // invariant-clamped fractional read
    float readReverse() const;
    float readPitched() const;
    void updateModeGeometry();                   // reverse chunk length + pitch grain window

    std::vector<float> buffer_;
    std::size_t writePos_ = 0;
    double targetDelaySamples_ = 1.0;
    double currentDelaySamples_ = 1.0;
    double glideCoef_ = 0.0;
    float modSamples_ = 0.0f;
    float feedback_ = 0.0f;
    double sampleRate_ = 44100.0;
    bool running_ = false;
    ReadMode readMode_ = ReadMode::Normal;
    std::size_t reverseChunkLen_ = 256;          // L: recomputed on target-delay changes only
    std::size_t reverseCounter_ = 0;             // j in [0, L)
    float pitchSemitones_ = 0.0f;
    double pitchRatio_ = 1.0;                    // 2^(semitones/12); exactly 1.0 disables
    double pitchWindowSamples_ = 256.0;          // W: grain window
    double grainPhase_ = 0.0;                    // p0 in [0, 1); p1 = p0 + 0.5 mod 1
};

} // namespace orbit::dsp
