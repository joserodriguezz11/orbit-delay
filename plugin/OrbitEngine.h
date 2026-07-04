#pragma once
#include <array>
#include "dsp/DelayLine.h"
#include "dsp/Ducker.h"
#include "dsp/Lfo.h"
#include "dsp/OnePole.h"
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
    static constexpr float kMaxModSeconds = 0.002f;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setTap(int index, const TapSettings& settings);
    void setDryWet(float mix01);
    void setDuckAmount(float amount01);
    void setBpm(double bpm);
    void setModulation(float depth01, float rateHz);  // depth 0..1 -> up to +/-2 ms
    void setFilters(float lowCutHz, float highCutHz); // lowCut <= 0 off; highCut >= 20000 off
    void setPingPong(bool enabled);                   // stereo only: feedback crosses channels
    void setWidth(float width01to2);                  // wet mid/side width: 0 mono, 1 pass, 2 wide
    void setFreeze(bool enabled);                     // recirculate lines at unity; input stops entering

    // In-place processing. channelData must have >= numChannels pointers.
    void process(float* const* channelData, int numChannels, int numSamples);

private:
    void applyTapTime(int index);
    void applyTapTimes();

    std::array<std::array<dsp::DelayLine, kMaxChannels>, kNumTaps> lines_;
    std::array<TapSettings, kNumTaps> taps_;
    // Per-tap Motion headroom: scales LFO depth so short taps get
    // proportionally less modulation instead of clamp flat-topping.
    std::array<float, kNumTaps> modScale_ {};
    dsp::Ducker ducker_;
    dsp::Lfo lfo_;
    float modDepthSamples_ = 0.0f;
    float lowCutHz_ = 0.0f;
    float highCutHz_ = 20000.0f;
    std::array<dsp::OnePole, kMaxChannels> lowCutFilters_;
    std::array<dsp::OnePole, kMaxChannels> highCutFilters_;
    float dryGain_ = 1.0f;
    float wetGain_ = 0.0f;
    float mix_ = 0.3f;
    bool pingPong_ = false;
    float width_ = 1.0f;
    bool frozen_ = false;
    double bpm_ = 120.0;
    double sampleRate_ = 44100.0;
    int numChannels_ = kMaxChannels;
};

} // namespace orbit
