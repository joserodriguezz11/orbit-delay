# Orbit Phase 2 Wave 3 — Visualization Feed Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the engine-side visualization feed from spec §6 — lock-free per-tap fire events, RMS/peak levels, and duck gain — plus a JUCE-free terminal demo proving it live.

**Architecture:** New JUCE-free `orbit::viz` module (`core/viz/`): `VizFeed` (SPSC event ring + seqlock level snapshot) and `TapFireDetector` (envelope + threshold + re-trigger hold). `OrbitEngine` owns one `VizFeed`, feeds detectors/accumulators in its per-sample loop, publishes the snapshot once per block. A `viz-demo` CLI target renders the feed in the terminal. The feed observes the signal path and never modifies it.

**Tech Stack:** unchanged (C++17, JUCE 8.0.4 [plugin layer only], Catch2, CMake).

**Spec:** `docs/superpowers/specs/2026-07-04-orbit-viz-feed-design.md` (binding).

## Global Constraints

- No JUCE anywhere in `core/`, `plugin/OrbitEngine.*`, or `tools/`.
- No allocation/locks/logging in audio-thread paths after `prepare()`. (`VizFeed::prepare` allocates; `push/pop/write/read` never do.)
- No new plugin parameters; `stateVersion` stays 1; auval must still report `34 Global Scope Parameters` + VALIDATION SUCCEEDED.
- All 49 existing test cases pass UNCHANGED; default audio output bit-identical (feed is observe-only).
- Namespace `orbit::viz`. Trailing-underscore members. Conventional commits.
- Binding constants: ring capacity 256; attack 1 ms; release 50 ms (envelope AND RMS smoothing); fire threshold 0.01 (−40 dB); intensity window 5 ms; hold = tapDelay/2 clamped [20 ms, 250 ms].
- Branch: `dev/phase-2-wave-3` from main (created in Task 1). Do not push until delivery.

---

### Task 1: VizFeed — SPSC event ring + seqlock level snapshot

**Files:**
- Create: `core/viz/VizFeed.h`, `core/viz/VizFeed.cpp`, `tests/core/VizFeedTests.cpp`
- Modify: `core/CMakeLists.txt` (add VizFeed.cpp), `tests/CMakeLists.txt` (add VizFeedTests.cpp)

**Interfaces produced (binding for Tasks 3–4):**

```cpp
// core/viz/VizFeed.h
#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace orbit::viz {

struct TapFireEvent {
    std::uint32_t tapIndex = 0;
    std::uint64_t timeSamples = 0;   // absolute engine sample count of the threshold crossing
    float intensity01 = 0.0f;        // clamped [0, 1]
};

struct LevelSnapshot {
    float inRms = 0.0f, inPeak = 0.0f;
    float outRms = 0.0f, outPeak = 0.0f;
    float duckGain = 1.0f;           // linear; 1 = no ducking
};

// Single-producer (audio thread) / single-consumer (UI thread) feed.
// prepare()/reset() are NOT thread-safe: call with audio stopped.
class VizFeed {
public:
    static constexpr std::size_t kEventCapacity = 256;

    void prepare();                          // allocates ring storage, then reset()
    void reset();                            // drops queued events, zeroes snapshot

    bool pushEvent(const TapFireEvent& e);   // producer; false = ring full, event dropped
    bool popEvent(TapFireEvent& out);        // consumer; false = empty

    void writeLevels(const LevelSnapshot& s); // producer, once per block
    LevelSnapshot readLevels() const;         // consumer; retries on torn read

private:
    std::vector<TapFireEvent> ring_;
    std::atomic<std::uint64_t> head_ { 0 };  // next write slot (producer-owned)
    std::atomic<std::uint64_t> tail_ { 0 };  // next read slot (consumer-owned)

    // Seqlock: version_ odd = write in progress; fields relaxed-atomic so a
    // racing read is unordered, not UB — version_ acquire/release publishes.
    std::atomic<std::uint32_t> version_ { 0 };
    std::atomic<float> inRms_ { 0.0f }, inPeak_ { 0.0f };
    std::atomic<float> outRms_ { 0.0f }, outPeak_ { 0.0f };
    std::atomic<float> duckGain_ { 1.0f };
};

} // namespace orbit::viz
```

**Implementation (verbatim, `core/viz/VizFeed.cpp`):**

```cpp
#include "viz/VizFeed.h"

namespace orbit::viz {

void VizFeed::prepare() {
    ring_.assign(kEventCapacity, TapFireEvent {});
    reset();
}

void VizFeed::reset() {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    version_.store(0, std::memory_order_relaxed);
    inRms_.store(0.0f, std::memory_order_relaxed);
    inPeak_.store(0.0f, std::memory_order_relaxed);
    outRms_.store(0.0f, std::memory_order_relaxed);
    outPeak_.store(0.0f, std::memory_order_relaxed);
    duckGain_.store(1.0f, std::memory_order_relaxed);
}

bool VizFeed::pushEvent(const TapFireEvent& e) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (head - tail >= kEventCapacity)
        return false;                                  // full: drop, never block
    ring_[static_cast<std::size_t>(head % kEventCapacity)] = e;
    head_.store(head + 1, std::memory_order_release);
    return true;
}

bool VizFeed::popEvent(TapFireEvent& out) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    const auto head = head_.load(std::memory_order_acquire);
    if (tail == head)
        return false;                                  // empty
    out = ring_[static_cast<std::size_t>(tail % kEventCapacity)];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

void VizFeed::writeLevels(const LevelSnapshot& s) {
    const auto v = version_.load(std::memory_order_relaxed);
    version_.store(v + 1, std::memory_order_release);  // odd: write in progress
    inRms_.store(s.inRms, std::memory_order_relaxed);
    inPeak_.store(s.inPeak, std::memory_order_relaxed);
    outRms_.store(s.outRms, std::memory_order_relaxed);
    outPeak_.store(s.outPeak, std::memory_order_relaxed);
    duckGain_.store(s.duckGain, std::memory_order_relaxed);
    version_.store(v + 2, std::memory_order_release);  // even: published
}

LevelSnapshot VizFeed::readLevels() const {
    LevelSnapshot s;
    for (;;) {
        const auto v1 = version_.load(std::memory_order_acquire);
        if (v1 & 1u)
            continue;                                  // writer mid-flight
        s.inRms = inRms_.load(std::memory_order_relaxed);
        s.inPeak = inPeak_.load(std::memory_order_relaxed);
        s.outRms = outRms_.load(std::memory_order_relaxed);
        s.outPeak = outPeak_.load(std::memory_order_relaxed);
        s.duckGain = duckGain_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (version_.load(std::memory_order_relaxed) == v1)
            return s;                                  // consistent
    }
}

} // namespace orbit::viz
```

- [ ] **Step 1: create branch**

```bash
git checkout main && git checkout -b dev/phase-2-wave-3
```

- [ ] **Step 2 (TDD): add failing tests** — `tests/core/VizFeedTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include "viz/VizFeed.h"

using Catch::Approx;
using orbit::viz::LevelSnapshot;
using orbit::viz::TapFireEvent;
using orbit::viz::VizFeed;

TEST_CASE("event ring: FIFO order, wraparound, drop-when-full") {
    VizFeed feed;
    feed.prepare();
    TapFireEvent e;
    REQUIRE_FALSE(feed.popEvent(e));                    // starts empty
    for (std::uint32_t round = 0; round < 3; ++round) { // 3 rounds forces wraparound
        for (std::uint32_t i = 0; i < VizFeed::kEventCapacity; ++i)
            REQUIRE(feed.pushEvent({ i, round, 0.5f }));
        REQUIRE_FALSE(feed.pushEvent({ 999, 999, 1.0f })); // full: dropped
        for (std::uint32_t i = 0; i < VizFeed::kEventCapacity; ++i) {
            REQUIRE(feed.popEvent(e));
            CHECK(e.tapIndex == i);                     // order kept, dropped event absent
            CHECK(e.timeSamples == round);
        }
        REQUIRE_FALSE(feed.popEvent(e));
    }
}

TEST_CASE("event ring: two-thread stress loses nothing below capacity") {
    VizFeed feed;
    feed.prepare();
    constexpr std::uint32_t kTotal = 100000;
    std::atomic<bool> done { false };
    std::vector<std::uint32_t> seen;
    seen.reserve(kTotal);
    std::thread consumer([&] {
        TapFireEvent e;
        while (!done.load() || feed.popEvent(e)) {
            if (e.intensity01 != 0.0f) { seen.push_back(e.tapIndex); e.intensity01 = 0.0f; }
            else if (feed.popEvent(e)) { seen.push_back(e.tapIndex); e.intensity01 = 0.0f; }
        }
    });
    for (std::uint32_t i = 0; i < kTotal; ++i)
        while (!feed.pushEvent({ i, 0, 1.0f }))         // spin on full: consumer drains
            std::this_thread::yield();
    done.store(true);
    consumer.join();
    REQUIRE(seen.size() == kTotal);
    for (std::uint32_t i = 0; i < kTotal; ++i)
        REQUIRE(seen[i] == i);                          // no loss, no dupes, no tears
}

TEST_CASE("level snapshot: write then read round-trips") {
    VizFeed feed;
    feed.prepare();
    LevelSnapshot def = feed.readLevels();
    CHECK(def.duckGain == 1.0f);
    CHECK(def.inRms == 0.0f);
    feed.writeLevels({ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f });
    const auto s = feed.readLevels();
    CHECK(s.inRms == 0.1f);
    CHECK(s.inPeak == 0.2f);
    CHECK(s.outRms == 0.3f);
    CHECK(s.outPeak == 0.4f);
    CHECK(s.duckGain == 0.5f);
}

TEST_CASE("level snapshot: reader never sees a torn write") {
    VizFeed feed;
    feed.prepare();
    // Seed before spawning the writer: the prepare() default {0,0,0,0,1}
    // does NOT satisfy the all-equal invariant, and the reader may run
    // before the writer's first write. (Amended during implementation —
    // the original test failed deterministically on that startup race.)
    feed.writeLevels({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });
    std::atomic<bool> stop { false };
    std::thread writer([&] {
        float x = 0.25f;
        while (!stop.load()) {
            // all five fields carry the same value: any mix = a torn read
            feed.writeLevels({ x, x, x, x, x });
            x += 0.25f;
        }
    });
    bool torn = false;
    for (int i = 0; i < 200000 && !torn; ++i) {
        const auto s = feed.readLevels();
        torn = !(s.inRms == s.inPeak && s.inRms == s.outRms
                 && s.inRms == s.outPeak && s.inRms == s.duckGain);
    }
    stop.store(true);
    writer.join();
    REQUIRE_FALSE(torn);   // assert only after join: a REQUIRE throw must
                           // not destroy a joinable thread (SIGABRT)
}
```

- [ ] **Step 3: verify RED** — `cmake --build build --target orbit_tests -j8` fails: `viz/VizFeed.h` not found.
- [ ] **Step 4: implement** — the two files verbatim above; add `viz/VizFeed.cpp` to `core/CMakeLists.txt` sources and `VizFeedTests.cpp` to `tests/CMakeLists.txt` (follow how `dsp/CharacterStage.cpp` / `CharacterStageTests.cpp` were added).
- [ ] **Step 5: verify GREEN** — full suite: 53 cases, all pass, pristine output.
- [ ] **Step 6: commit**

```bash
git add core/ tests/
git commit -m "feat(core): VizFeed — lock-free event ring and seqlock level snapshot"
```

---

### Task 2: TapFireDetector

**Files:**
- Create: `core/viz/TapFireDetector.h`, `core/viz/TapFireDetector.cpp`, `tests/core/TapFireDetectorTests.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces produced (binding for Task 3):**

```cpp
// core/viz/TapFireDetector.h
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
```

**Implementation (verbatim, `core/viz/TapFireDetector.cpp`):**

```cpp
#include "viz/TapFireDetector.h"
#include <algorithm>
#include <cmath>

namespace orbit::viz {

namespace {
float onePoleCoef(float seconds, double sampleRate) {
    // Same form as Ducker: fraction of the remaining distance per sample.
    return 1.0f - std::exp(-1.0f / (seconds * static_cast<float>(sampleRate)));
}
} // namespace

void TapFireDetector::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    attackCoef_ = onePoleCoef(kAttackSeconds, sampleRate);
    releaseCoef_ = onePoleCoef(kReleaseSeconds, sampleRate);
    intensityWindowSamples_ =
        static_cast<int>(kIntensityWindowSeconds * static_cast<float>(sampleRate));
    setTapDelaySeconds(0.0f);   // hold floors at kHoldMinSeconds until told otherwise
    reset();
}

void TapFireDetector::reset() {
    env_ = 0.0f;
    samplesSinceFire_ = -1;
    emitted_ = false;
    peakSinceFire_ = 0.0f;
    fireTime_ = 0;
}

void TapFireDetector::setTapDelaySeconds(float seconds) {
    const float hold = std::clamp(seconds * 0.5f, kHoldMinSeconds, kHoldMaxSeconds);
    holdSamples_ = static_cast<int>(hold * static_cast<float>(sampleRate_));
}

bool TapFireDetector::processSample(float level, std::uint64_t timeSamples,
                                    std::uint64_t& fireTimeSamples, float& intensity01) {
    const float prevEnv = env_;
    const float coef = level > env_ ? attackCoef_ : releaseCoef_;
    env_ += (level - env_) * coef;
    if (!(std::abs(env_) >= 1.0e-12f))   // denormal/NaN flush, house policy
        env_ = 0.0f;

    if (samplesSinceFire_ < 0) {                                  // idle/armed
        if (prevEnv < kThreshold && env_ >= kThreshold) {         // upward crossing
            samplesSinceFire_ = 0;
            emitted_ = false;
            peakSinceFire_ = level;
            fireTime_ = timeSamples;
        }
        return false;
    }

    ++samplesSinceFire_;
    if (!emitted_) {
        peakSinceFire_ = std::max(peakSinceFire_, level);
        const int windowEnd = std::min(intensityWindowSamples_, holdSamples_);
        if (samplesSinceFire_ >= windowEnd) {                     // window closes: emit
            emitted_ = true;
            fireTimeSamples = fireTime_;
            intensity01 = std::clamp(peakSinceFire_, 0.0f, 1.0f);
            return true;
        }
    }
    if (samplesSinceFire_ >= holdSamples_)
        samplesSinceFire_ = -1;                                   // hold over: re-arm
    return false;
}

} // namespace orbit::viz
```

- [ ] **Step 1 (TDD): add failing tests** — `tests/core/TapFireDetectorTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>
#include "viz/TapFireDetector.h"

using orbit::viz::TapFireDetector;

namespace {
// Runs `input` through a detector prepared at 48 kHz with the given tap
// delay; returns (fireTime, intensity) pairs.
std::vector<std::pair<std::uint64_t, float>>
run(const std::vector<float>& input, float tapDelaySeconds) {
    TapFireDetector det;
    det.prepare(48000.0);
    det.setTapDelaySeconds(tapDelaySeconds);
    std::vector<std::pair<std::uint64_t, float>> fires;
    for (std::size_t n = 0; n < input.size(); ++n) {
        std::uint64_t t = 0;
        float intensity = 0.0f;
        if (det.processSample(input[n], static_cast<std::uint64_t>(n), t, intensity))
            fires.emplace_back(t, intensity);
    }
    return fires;
}
} // namespace

TEST_CASE("silence produces zero fires") {
    CHECK(run(std::vector<float>(96000, 0.0f), 0.25f).empty());
}

TEST_CASE("one echo burst = exactly one fire, timestamped at the crossing") {
    std::vector<float> in(48000, 0.0f);
    for (int n = 10000; n < 10240; ++n) in[static_cast<size_t>(n)] = 0.8f; // 5 ms burst
    const auto fires = run(in, 0.25f);
    REQUIRE(fires.size() == 1);
    CHECK(fires[0].first >= 10000);
    CHECK(fires[0].first < 10100);       // 1 ms attack: crossing within ~2 ms
    CHECK(fires[0].second > 0.7f);       // intensity ~ burst peak
}

TEST_CASE("hold prevents double-fires inside one repeat") {
    std::vector<float> in(48000, 0.0f);
    for (int n = 10000; n < 10240; ++n) in[static_cast<size_t>(n)] = 0.8f;
    for (int n = 10400; n < 10640; ++n) in[static_cast<size_t>(n)] = 0.7f;  // flutter 3ms later
    CHECK(run(in, 0.25f).size() == 1);   // hold (125 ms) swallows the flutter
}

TEST_CASE("successive repeats each fire, louder repeat = higher intensity") {
    std::vector<float> in(96000, 0.0f);
    const int kGap = 24000;              // 500 ms apart, hold = 125 ms
    const float levels[] = { 0.9f, 0.45f, 0.2f };
    for (int r = 0; r < 3; ++r)
        for (int n = 0; n < 240; ++n)
            in[static_cast<size_t>(10000 + r * kGap + n)] = levels[r];
    const auto fires = run(in, 0.25f);
    REQUIRE(fires.size() == 3);
    CHECK(fires[0].second > fires[1].second);
    CHECK(fires[1].second > fires[2].second);
}

TEST_CASE("sub-threshold signal never fires") {
    CHECK(run(std::vector<float>(48000, 0.005f), 0.25f).empty());  // -46 dB < -40 dB
}
```

- [ ] **Step 2: verify RED** (header not found), **Step 3: implement verbatim above + CMake wiring**, **Step 4: GREEN** — 58 cases pass, pristine.
- [ ] **Step 5: commit**

```bash
git add core/ tests/
git commit -m "feat(core): TapFireDetector — envelope-gated repeat-fire events"
```

---

### Task 3: OrbitEngine integration

**Files:**
- Modify: `plugin/OrbitEngine.h`, `plugin/OrbitEngine.cpp`
- Test: `tests/plugin/OrbitEngineTests.cpp`

**Interfaces:**
- Consumes: `viz::VizFeed`, `viz::TapFireDetector` (Tasks 1–2, exact APIs above).
- Produces: `viz::VizFeed& OrbitEngine::vizFeed();` and `const viz::VizFeed& vizFeed() const;` — the contract Task 4's demo and the Phase 3 UI consume.

**Wiring (implementer latitude on exact code placement; behavior binding):**

1. `OrbitEngine.h`: include `"viz/VizFeed.h"` and `"viz/TapFireDetector.h"`; members `viz::VizFeed vizFeed_; std::array<viz::TapFireDetector, kNumTaps> fireDetectors_ {}; std::uint64_t timeSamples_ = 0; float inMs_ = 0.0f, outMs_ = 0.0f, msCoef_ = 0.0f;` plus the two `vizFeed()` accessors.
2. `prepare()`: `vizFeed_.prepare(); timeSamples_ = 0; inMs_ = outMs_ = 0.0f;` prepare each detector at the sample rate; `msCoef_` = the Task 2 one-pole formula at 50 ms. `reset()`: `vizFeed_.reset();` reset detectors and accumulators.
3. `setTap(index, tap)` / wherever tap seconds resolve (`applyTapTime`): forward resolved seconds to `fireDetectors_[index].setTapDelaySeconds(...)`; on `enabled == false`, `reset()` that detector.
4. Per sample in `process()` (after `outs[]` and `duckGain` exist, before/without touching any signal math):
   - `inLevel` = mean |dry| across channels for RMS, max for peak; same for the final output sample (post mix — read `channelData[ch][n]` after it is written).
   - `inMs_ += (in*in - inMs_) * msCoef_;` same for `outMs_` (use the per-channel mean of squares).
   - Per tap `t`: `level` = tap enabled ? max |outs[ch]| across channels : 0; call detector; on `true`, `vizFeed_.pushEvent({ t, fireTime, intensity })`.
   - Track per-block peaks; `++timeSamples_`.
5. End of `process()`: `vizFeed_.writeLevels({ std::sqrt(inMs_), inBlockPeak, std::sqrt(outMs_), outBlockPeak, lastDuckGain })`.

- [ ] **Step 1 (TDD): add failing tests** — `tests/plugin/OrbitEngineTests.cpp`:

```cpp
TEST_CASE("viz feed reports tap fires with correct tap index and sane timing") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 4800.0f / 48000.0f;   // 100 ms
    tap.feedback = 0.0f;
    engine.setTap(2, tap);                   // tap index 2 on purpose
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(24000);
    for (int n = 0; n < 240; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] = 0.8f;
    engine.process(buf.channels.data(), 2, 24000);
    orbit::viz::TapFireEvent e;
    REQUIRE(engine.vizFeed().popEvent(e));
    CHECK(e.tapIndex == 2u);
    CHECK(e.timeSamples >= 4800u);           // echo lands at ~100 ms
    CHECK(e.timeSamples < 5200u);
    CHECK(e.intensity01 > 0.5f);
    REQUIRE_FALSE(engine.vizFeed().popEvent(e));  // one echo, one event
}

TEST_CASE("viz snapshot tracks levels and duck gain") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 0.05f;
    tap.feedback = 0.5f;
    engine.setTap(0, tap);
    engine.setDryWet(0.5f);
    engine.setDuckAmount(1.0f);              // full ducking
    StereoBuffer buf(48000);
    for (int n = 0; n < 48000; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
    engine.process(buf.channels.data(), 2, 48000);
    const auto s = engine.vizFeed().readLevels();
    CHECK(s.inRms > 0.2f);                   // 0.5-amp sine RMS ~= 0.35
    CHECK(s.inRms < 0.5f);
    CHECK(s.inPeak > 0.4f);
    CHECK(s.outRms > 0.0f);
    CHECK(s.duckGain < 0.9f);                // loud input + full duck => audible reduction
}

TEST_CASE("viz feed stays silent for disabled taps and empty input") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    StereoBuffer buf(24000);                  // all zeros, all taps disabled
    engine.process(buf.channels.data(), 2, 24000);
    orbit::viz::TapFireEvent e;
    CHECK_FALSE(engine.vizFeed().popEvent(e));
    const auto s = engine.vizFeed().readLevels();
    CHECK(s.inRms == 0.0f);
    CHECK(s.outPeak == 0.0f);
}
```

(Include `"viz/VizFeed.h"` at the top of the test file if not pulled in via the engine header.)

- [ ] **Step 2: verify RED** (no `vizFeed()` member), **Step 3: implement per the wiring above**, **Step 4: GREEN — all 61 cases, every pre-existing case unchanged**, **Step 5: commit**

```bash
git add plugin/ tests/
git commit -m "feat(plugin): engine emits viz feed — tap fires, levels, duck gain"
```

---

### Task 4: viz-demo terminal tool + final gates

**Files:**
- Create: `tools/CMakeLists.txt`, `tools/viz_demo.cpp`
- Modify: root `CMakeLists.txt` (add `add_subdirectory(tools)` next to `tests`)

**Interfaces:**
- Consumes: `OrbitEngine::vizFeed()`, `viz::TapFireEvent`, `viz::LevelSnapshot` (Task 3). No JUCE.

**Behavior (binding; rendering details = implementer latitude):**
- 48 kHz stereo, 480-sample blocks (10 ms). Config: tap 1 = 375 ms feedback 0.55, tap 2 = 750 ms feedback 0.35, ping-pong on, duck 0.8, dry/wet 0.5.
- Input: one 0.9-amplitude 240-sample burst at the start of each second, amplitude alternating 0.9 / 0.5. Duration 10 s, then print a summary line (total fires per tap) and `return 0`.
- Each block: process, drain `popEvent` (print one line per fire: time in seconds, tap number, intensity bar), `readLevels()`, redraw a status line (`\r`): IN/OUT meter bars (RMS as bar, peak as marker), duck gain percentage. Pace with `std::this_thread::sleep_for(10ms)` — host side only.
- `tools/CMakeLists.txt` mirrors how `tests/` links the engine (same target link as `orbit_tests` minus Catch2). Target name `viz-demo`, executable `viz_demo`.

- [ ] **Step 1: implement the tool** (no TDD — it is a demo binary; the feed logic it exercises is already unit-tested).
- [ ] **Step 2: run it** — `cmake --build build --target viz-demo -j8 && ./build/tools/viz_demo`. Expected: fire lines for taps 1/2 (tap 2 roughly half as often), meters moving, duck dipping on each burst, exit 0. Paste a short excerpt of the output in the report.
- [ ] **Step 3: full gates**

```bash
cmake --build build --target orbit_tests OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone -j8
./build/tests/orbit_tests                 # 61 cases green
auval -v aufx Orb1 Orba                   # 34 Global Scope Parameters, VALIDATION SUCCEEDED
./scripts/run_pluginval.sh                # SUCCESS
```

- [ ] **Step 4: commit**

```bash
git add tools/ CMakeLists.txt
git commit -m "feat(tools): viz-demo — live terminal readout of the visualization feed"
```

---

## Self-Review Notes

- Spec coverage: §2 transport (T1), §3 detection incl. the 5 ms intensity window and crossing timestamp (T2), §4 engine integration incl. always-on + accessor (T3), §5 demo (T4), §6 tests (T1–T3), §7 constraints (global block). Presets/backlog intentionally out of scope.
- Type consistency: `TapFireEvent{tapIndex,timeSamples,intensity01}` and `LevelSnapshot{inRms,inPeak,outRms,outPeak,duckGain}` used identically in T1 code, T3 tests, T4 demo. `processSample(level, time, fireTime&, intensity&)` signature matches between T2 header and tests.
- Known accepted behaviors (document in code, don't fix): a sustained pad above threshold fires once, not periodically (no re-crossing); intensity reports ~5 ms after the crossing; ring overflow drops the newest events.
