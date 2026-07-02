#pragma once
#include <array>
#include "dsp/DelayLine.h"
#include "dsp/Ducker.h"
#include "dsp/TempoSync.h"

namespace orbit {

struct TapSettings {
    bool enabled = false;
    dsp::SyncDivision sync = dsp::SyncDivision::Free;
    float timeSeconds = 0.35f;   // used when sync == Free
    float feedback = 0.35f;
};

// Orbit's 4-tap delay topology. Pure C++ (no JUCE) so it unit-tests headlessly.
// Real-time safe after prepare().
class OrbitEngine {
public:
    static constexpr int kNumTaps = 4;
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxDelaySeconds = 4.0f;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setTap(int index, const TapSettings& settings);
    void setDryWet(float mix01);
    void setDuckAmount(float amount01);
    void setBpm(double bpm);

    // In-place processing. channelData must have >= numChannels pointers.
    void process(float* const* channelData, int numChannels, int numSamples);

private:
    void applyTapTime(int index);
    void applyTapTimes();

    std::array<std::array<dsp::DelayLine, kMaxChannels>, kNumTaps> lines_;
    std::array<TapSettings, kNumTaps> taps_;
    dsp::Ducker ducker_;
    float mix_ = 0.3f;
    double bpm_ = 120.0;
    double sampleRate_ = 44100.0;
    int numChannels_ = kMaxChannels;
};

} // namespace orbit
