#pragma once
#include <array>
#include <cstdint>
#include "dsp/CharacterStage.h"
#include "dsp/DelayLine.h"
#include "dsp/Ducker.h"
#include "dsp/Lfo.h"
#include "dsp/OnePole.h"
#include "dsp/TempoSync.h"
#include "viz/TapFireDetector.h"
#include "viz/VizFeed.h"

namespace orbit {

struct TapSettings {
    bool enabled = false;
    dsp::SyncDivision sync = dsp::SyncDivision::Free;
    float timeSeconds = 0.35f;   // used when sync == Free
    float feedback = 0.35f;
    bool reverse = false;              // chunked backward playback
    float pitchSemitones = 0.0f;       // repeat pitch shift; 0 off; ignored while reverse
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
    void setCharacterMode(dsp::CharacterStage::Mode mode); // wet coloring; Clean is true bypass

    // In-place processing. channelData must have >= numChannels pointers.
    void process(float* const* channelData, int numChannels, int numSamples);

    // Visualization feed (observer only — never affects the signal path).
    // Producer side is fed by process(); consumer side is the UI thread.
    //
    // Consumer contract (Phase 3 UI):
    // - Single consumer thread. Once per frame: drain popEvent() until it
    //   returns false, and call readLevels() once.
    // - Event timestamps and LevelSnapshot::now are the same absolute sample
    //   counter — monotonic across reset(), REBASED TO 0 by prepare().
    //   Event age = now - event.timeSamples.
    // - reset() is snapshot-only: it re-publishes a zero snapshot (duckGain 1,
    //   now 0 — a one-block transient until the next process() republishes
    //   the true counter) and leaves queued events poppable.
    // - Stale-epoch events: re-prepare keeps queued events while prepare()
    //   rebases the sample counter to 0, so events queued before a
    //   prepareToPlay can carry old-epoch timestamps where
    //   now - event.timeSamples underflows uint64 — the UI consumer must
    //   discard events with timeSamples > now.
    // - Snapshot peaks are per-block maxima (a 30 Hz poller sees the latest
    //   block only); RMS is 50 ms one-pole smoothed.
    // - Events may drop when the ring is full (newest dropped); animation
    //   data is disposable by design.
    //
    // PHASE 3 PRECONDITIONS (see final review in .superpowers/sdd/progress.md):
    // (a) RESOLVED — VizFeed::reset() is snapshot-only and reader-safe (uses
    //     the writeLevels seqlock; never touches head_/tail_), and
    //     VizFeed::prepare() allocates only on first call, so engine
    //     prepare()/reset() are safe against a concurrently polling reader.
    // (b) RESOLVED — LevelSnapshot::now carries the engine sample counter
    //     (published at block end), so the UI can compute event age.
    viz::VizFeed& vizFeed() { return vizFeed_; }
    const viz::VizFeed& vizFeed() const { return vizFeed_; }

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
    std::array<dsp::CharacterStage, kMaxChannels> characterStages_;
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

    // Viz observers (no effect on audio).
    viz::VizFeed vizFeed_;
    std::array<viz::TapFireDetector, kNumTaps> fireDetectors_ {};
    std::uint64_t timeSamples_ = 0;
    float inMs_ = 0.0f, outMs_ = 0.0f, msCoef_ = 0.0f;  // 50 ms one-pole mean-square
};

} // namespace orbit
