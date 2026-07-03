#pragma once

namespace orbit::dsp {

// Sine LFO in [-1, 1]. Real-time safe.
class Lfo {
public:
    void prepare(double sampleRate);
    void reset();
    void setRate(float hz);            // clamped to [0.01, 20]
    float processSample();

private:
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;               // cycles, [0, 1)
    double increment_ = 0.0;
};

} // namespace orbit::dsp
