#pragma once

namespace orbit::dsp {

// Sidechain ducker: follows the dry signal's level and returns a gain (0..1)
// to apply to the wet signal. amount = 0 -> always 1.0 (no ducking).
class Ducker {
public:
    void prepare(double sampleRate);
    void reset();
    void setAmount(float amount);            // clamped to [0, 1]

    // sidechainLevel: absolute level of the dry signal for this sample.
    float processGain(float sidechainLevel);

private:
    float amount_ = 0.0f;
    float envelope_ = 0.0f;
    float attackCoef_ = 0.0f;
    float releaseCoef_ = 0.0f;
};

} // namespace orbit::dsp
