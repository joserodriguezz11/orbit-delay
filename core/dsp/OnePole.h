#pragma once

namespace orbit::dsp {

// One-pole filter for gentle low-cut / high-cut shaping of the wet path.
// Real-time safe; state is a single float.
class OnePole {
public:
    enum class Mode { LowPass, HighPass };

    void prepare(double sampleRate, Mode mode);
    void reset();
    void setCutoff(float hz);          // clamped to [1, 0.45 * sampleRate]
    float processSample(float input);

private:
    Mode mode_ = Mode::LowPass;
    double sampleRate_ = 44100.0;
    float coef_ = 1.0f;
    float state_ = 0.0f;
};

} // namespace orbit::dsp
