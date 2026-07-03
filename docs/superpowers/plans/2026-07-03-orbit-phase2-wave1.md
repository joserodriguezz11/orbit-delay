# Orbit Phase 2 Wave 1 — Precision, Motion & Filters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade the delay engine with sample-accurate fractional timing, click-free delay-time gliding (automation/tempo-change safe), LFO modulation ("Motion"), and low-cut/high-cut filtering of the wet path — validated end-to-end in the plugin.

**Architecture:** All DSP changes stay in the JUCE-free layers (`core/dsp/`, `plugin/OrbitEngine.*`) and are unit-tested with Catch2. `DelayLine` gains double-precision delay state with a one-pole glide (snap-before-running semantics preserve every existing test) plus an unsmoothed modulation offset. New `OnePole` and `Lfo` core primitives feed engine-level wet filters and Motion. Four new parameters (22 total) wire through cached atomic pointers.

**Tech Stack:** C++17, JUCE 8.0.4, Catch2 v3.7.1, CMake (existing build).

**Phase roadmap:** Wave 2 (separate plan, after this merges) — character modes (Clean/Tape/Grit), reverse, pitch-shifted repeats, freeze, stereo width + ping-pong.

## Global Constraints

- **C++17**; existing pins unchanged (JUCE 8.0.4, Catch2 v3.7.1).
- **`core/` and `plugin/OrbitEngine.*` must not include any JUCE header.**
- **Real-time safety:** no heap allocation, locks, or logging in `processSample`/`processGain`/`process`/`processBlock`.
- **Namespaces:** `orbit::dsp` (core), `orbit` (engine), `orbit::params` (parameters).
- **New parameter IDs (exact):** `mod_depth`, `mod_rate`, `filter_lowcut`, `filter_highcut`. Existing 18 IDs unchanged.
- **Backward compatibility:** every existing test keeps passing unchanged (snap-before-running glide semantics make this possible); v1 session state loads with new params at defaults; `stateVersion` stays `1` (additive change only).
- **Glide time constant:** 50 ms (`DelayLine::kDefaultGlideSeconds = 0.05f`). **Max modulation depth:** ±2 ms (`OrbitEngine::kMaxModSeconds = 0.002f`).
- **Neutral defaults:** `mod_depth` 0, `filter_lowcut` 0 (off), `filter_highcut` 20000 (off) — a default-state plugin sounds identical to Phase 1.
- Work in `/Users/joserodriguez/orbit-delay` on branch `dev/phase-2-wave-1`. Conventional commits. Do not push until delivery.

---

### Task 1: DelayLine — double-precision timing, click-free glide, modulation hook

**Files:**
- Modify: `core/dsp/DelayLine.h` (full replacement below)
- Modify: `core/dsp/DelayLine.cpp` (full replacement below)
- Modify: `tests/core/DelayLineTests.cpp` (add 3 test cases; existing 7 must pass unchanged)

**Interfaces:**
- Consumes: nothing new.
- Produces: `DelayLine` API grows `void setModulationSamples(float samples)` (additive, unsmoothed offset applied at read time) and glide semantics: `setDelaySeconds` **snaps** when called before the first `processSample` after `prepare()`/`reset()`, and **glides** (one-pole, 50 ms) when called while running. `kDefaultGlideSeconds = 0.05f` is public. Everything else keeps its exact Phase 1 signature. Used by `OrbitEngine` (Task 3).

- [ ] **Step 1: Add the failing tests to `tests/core/DelayLineTests.cpp`**

Add `#include <cstddef>` if not present (existing includes: catch2 macros/approx, `<cmath>`, `<limits>`, `<vector>`, `"dsp/DelayLine.h"`). Append:

```cpp
TEST_CASE("DelayLine keeps sub-sample precision at large delays and high rates") {
    DelayLine line;
    line.prepare(192000.0, 4.0f);
    line.setDelaySeconds(100000.5f / 192000.0f);
    std::vector<float> out;
    for (int n = 0; n < 100050; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    CHECK(out[100000] == Approx(0.5f).margin(1e-3f));
    CHECK(out[100001] == Approx(0.5f).margin(1e-3f));
}

TEST_CASE("DelayLine glides between delay times instead of jumping") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);     // snaps: not running yet
    for (int n = 0; n < 500; ++n)
        line.processSample(0.0f);                 // now running
    line.setDelaySeconds(200.0f / 48000.0f);     // must glide, not jump
    const auto out = impulseResponse(line, 260);
    float early = 0.0f, late = 0.0f;
    for (int n = 0; n < 130; ++n)
        early += std::abs(out[static_cast<size_t>(n)]);
    for (int n = 190; n < 210; ++n)
        late += std::abs(out[static_cast<size_t>(n)]);
    CHECK(early > 0.5f);    // echo still lands near the old time right after the change
    CHECK(late < 0.05f);    // and NOT at the new time yet — no instant jump
}

TEST_CASE("DelayLine modulation offset shifts the read position") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);
    line.setModulationSamples(10.0f);            // effective delay 110 samples
    std::vector<float> out;
    for (int n = 0; n < 150; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    CHECK(out[110] == Approx(1.0f));
    CHECK(std::abs(out[100]) < 1e-6f);
}
```

- [ ] **Step 2: Build and verify the new tests fail**

Run: `cmake --build build --target orbit_tests -j8`
Expected: FAIL — `setModulationSamples` not a member (compile error). (The glide test would also fail at runtime against Phase 1 code, which jumps instantly.)

- [ ] **Step 3: Replace `core/dsp/DelayLine.h`**

```cpp
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
```

- [ ] **Step 4: Replace `core/dsp/DelayLine.cpp`**

```cpp
#include "dsp/DelayLine.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void DelayLine::prepare(double sampleRate, float maxDelaySeconds) {
    sampleRate_ = sampleRate;
    const auto maxSamples =
        static_cast<std::size_t>(std::ceil(sampleRate * maxDelaySeconds)) + 2;
    buffer_.assign(maxSamples, 0.0f);
    glideCoef_ = std::exp(-1.0 / (static_cast<double>(kDefaultGlideSeconds) * sampleRate));
    writePos_ = 0;
    targetDelaySamples_ = 1.0;
    currentDelaySamples_ = 1.0;
    modSamples_ = 0.0f;
    running_ = false;
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_ = 0;
    currentDelaySamples_ = targetDelaySamples_;
    running_ = false;
}

void DelayLine::setDelaySeconds(float seconds) {
    const double maxDelay =
        static_cast<double>(buffer_.size() - 2) / sampleRate_;
    const double clamped = std::clamp(static_cast<double>(seconds), 0.0, maxDelay);
    targetDelaySamples_ = std::max(1.0, clamped * sampleRate_);
    if (!running_)
        currentDelaySamples_ = targetDelaySamples_;   // snap during configuration
}

void DelayLine::setFeedback(float amount) {
    feedback_ = std::clamp(amount, 0.0f, kMaxFeedback);
}

void DelayLine::setModulationSamples(float samples) {
    modSamples_ = samples;
}

float DelayLine::readFractional() const {
    const std::size_t size = buffer_.size();
    const double maxDelay = static_cast<double>(size - 2);
    const double effective = std::clamp(
        currentDelaySamples_ + static_cast<double>(modSamples_), 1.0, maxDelay);
    const auto intDelay = static_cast<std::size_t>(effective);
    const float frac = static_cast<float>(effective - static_cast<double>(intDelay));
    const std::size_t newer = (writePos_ + size - intDelay) % size;
    const std::size_t older = (newer + size - 1) % size;
    return buffer_[newer] * (1.0f - frac) + buffer_[older] * frac;
}

float DelayLine::processSample(float input) {
    running_ = true;
    currentDelaySamples_ =
        targetDelaySamples_ + (currentDelaySamples_ - targetDelaySamples_) * glideCoef_;
    const float out = readFractional();
    float next = input + out * feedback_;
    // Flush denormal-range and non-finite values to hard zero so feedback
    // tails die cleanly and a bad input sample can't poison the line.
    if (!std::isfinite(next) || std::abs(next) < 1.0e-12f)
        next = 0.0f;
    buffer_[writePos_] = next;
    writePos_ = (writePos_ + 1) % buffer_.size();
    return out;
}

} // namespace orbit::dsp
```

Correctness notes for the reviewer: integer/fractional split happens per read from a `double` (exact for all integers below 2^53), eliminating the float-ULP quantization at large buffer positions; interpolation weighting `buffer[newer]*(1-frac) + buffer[older]*frac` is algebraically identical to Phase 1's `buffer[i0] + frac'*(buffer[i1]-buffer[i0])` mapping.

- [ ] **Step 5: Build and run — ALL DelayLine tests (10) plus everything else must pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass, including the 7 pre-existing DelayLine cases unchanged.

- [ ] **Step 6: Commit**

```bash
git add core/dsp/DelayLine.h core/dsp/DelayLine.cpp tests/core/DelayLineTests.cpp
git commit -m "feat(core): double-precision delay timing with click-free glide and mod offset"
```

---

### Task 2: OnePole filter + Lfo (core primitives)

**Files:**
- Create: `core/dsp/OnePole.h`, `core/dsp/OnePole.cpp`
- Create: `core/dsp/Lfo.h`, `core/dsp/Lfo.cpp`
- Create: `tests/core/OnePoleTests.cpp`, `tests/core/LfoTests.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt` (add the new sources)

**Interfaces:**
- Consumes: nothing.
- Produces: `orbit::dsp::OnePole` — `enum class Mode { LowPass, HighPass }`, `void prepare(double sampleRate, Mode mode)`, `void reset()`, `void setCutoff(float hz)` (clamped to [1, 0.45·sr]), `float processSample(float input)`. `orbit::dsp::Lfo` — `void prepare(double sampleRate)`, `void reset()`, `void setRate(float hz)` (clamped to [0.01, 20]), `float processSample()` returning a sine in [-1, 1]. Used by `OrbitEngine` (Task 3).

- [ ] **Step 1: Write the failing tests**

`tests/core/OnePoleTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/OnePole.h"

using Catch::Approx;
using orbit::dsp::OnePole;

TEST_CASE("OnePole low-pass passes DC") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::LowPass);
    f.setCutoff(1000.0f);
    float y = 0.0f;
    for (int n = 0; n < 48000; ++n)
        y = f.processSample(1.0f);
    CHECK(y == Approx(1.0f).margin(1e-3f));
}

TEST_CASE("OnePole high-pass rejects DC") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::HighPass);
    f.setCutoff(100.0f);
    float y = 1.0f;
    for (int n = 0; n < 48000; ++n)
        y = f.processSample(1.0f);
    CHECK(std::abs(y) < 0.01f);
}

TEST_CASE("OnePole low-pass attenuates Nyquist-rate alternation") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::LowPass);
    f.setCutoff(1000.0f);
    float sumAbs = 0.0f;
    for (int n = 0; n < 4800; ++n) {
        const float x = (n % 2 == 0) ? 1.0f : -1.0f;
        const float y = f.processSample(x);
        if (n >= 4700) sumAbs += std::abs(y);
    }
    CHECK(sumAbs / 100.0f < 0.2f);
}
```

`tests/core/LfoTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/Lfo.h"

using Catch::Approx;
using orbit::dsp::Lfo;

TEST_CASE("Lfo produces a bounded sine with the requested period") {
    Lfo lfo;
    lfo.prepare(48000.0);
    lfo.setRate(1.0f);                       // 1 Hz -> period 48000 samples
    float atQuarter = 0.0f, atHalf = 0.0f, maxAbs = 0.0f;
    for (int n = 0; n < 48000; ++n) {
        const float v = lfo.processSample();
        if (n == 11999) atQuarter = v;       // ~quarter period -> +1 peak
        if (n == 23999) atHalf = v;          // ~half period -> zero crossing
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    CHECK(atQuarter == Approx(1.0f).margin(1e-2f));
    CHECK(std::abs(atHalf) < 1e-2f);
    CHECK(maxAbs <= 1.0f + 1e-4f);
}

TEST_CASE("Lfo reset restarts the phase") {
    Lfo lfo;
    lfo.prepare(48000.0);
    lfo.setRate(2.0f);
    const float first = lfo.processSample();
    for (int n = 0; n < 1000; ++n)
        lfo.processSample();
    lfo.reset();
    CHECK(lfo.processSample() == Approx(first));
}
```

Add `core/OnePoleTests.cpp` and `core/LfoTests.cpp` to `orbit_tests` sources in `tests/CMakeLists.txt`.

- [ ] **Step 2: Verify failure**

Run: `cmake --build build --target orbit_tests -j8`
Expected: FAIL — `'dsp/OnePole.h' file not found`

- [ ] **Step 3: Implement**

`core/dsp/OnePole.h`:

```cpp
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
```

`core/dsp/OnePole.cpp`:

```cpp
#include "dsp/OnePole.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void OnePole::prepare(double sampleRate, Mode mode) {
    sampleRate_ = sampleRate;
    mode_ = mode;
    reset();
}

void OnePole::reset() { state_ = 0.0f; }

void OnePole::setCutoff(float hz) {
    const float maxHz = static_cast<float>(0.45 * sampleRate_);
    hz = std::clamp(hz, 1.0f, maxHz);
    coef_ = 1.0f - std::exp(static_cast<float>(-2.0 * 3.14159265358979323846 * hz / sampleRate_));
}

float OnePole::processSample(float input) {
    state_ += coef_ * (input - state_);
    return mode_ == Mode::LowPass ? state_ : input - state_;
}

} // namespace orbit::dsp
```

`core/dsp/Lfo.h`:

```cpp
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
```

`core/dsp/Lfo.cpp`:

```cpp
#include "dsp/Lfo.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void Lfo::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reset();
}

void Lfo::reset() { phase_ = 0.0; }

void Lfo::setRate(float hz) {
    const double clamped = std::clamp(static_cast<double>(hz), 0.01, 20.0);
    increment_ = clamped / sampleRate_;
}

float Lfo::processSample() {
    const float value =
        static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * phase_));
    phase_ += increment_;
    if (phase_ >= 1.0)
        phase_ -= 1.0;
    return value;
}

} // namespace orbit::dsp
```

Add `dsp/OnePole.cpp` and `dsp/Lfo.cpp` to `orbit_core` sources in `core/CMakeLists.txt`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add core/ tests/
git commit -m "feat(core): one-pole filter and sine LFO primitives"
```

---

### Task 3: OrbitEngine — Motion, wet filters, equal-power mix, tempo-glide regression

**Files:**
- Modify: `plugin/OrbitEngine.h`
- Modify: `plugin/OrbitEngine.cpp`
- Modify: `tests/plugin/OrbitEngineTests.cpp` (add 4 test cases; existing 4 must pass unchanged)

**Interfaces:**
- Consumes: `DelayLine::setModulationSamples` (Task 1), `OnePole`, `Lfo` (Task 2).
- Produces: `OrbitEngine` grows `void setModulation(float depth01, float rateHz)`, `void setFilters(float lowCutHz, float highCutHz)` (lowCut ≤ 0 = off; highCut ≥ 20000 = off), and `static constexpr float kMaxModSeconds = 0.002f`. Dry/wet becomes equal-power (`dryGain = sqrt(1-mix)`, `wetGain = sqrt(mix)`) — existing tests use mix 1.0 and comparative energy, so they are unaffected. Used by the processor (Task 4).

- [ ] **Step 1: Add the failing tests to `tests/plugin/OrbitEngineTests.cpp`**

Append (file already includes catch2, `<array>`, `<algorithm>`, `<cmath>`, `<memory>`, `<vector>`, `"OrbitEngine.h"`; the `StereoBuffer` helper already exists):

```cpp
TEST_CASE("changing the host tempo retunes a synced tap by gliding") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Quarter;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);                       // 120 BPM default -> 24000 samples
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer settle(48000);                  // start running at the old tempo
    engine.process(settle.channels.data(), 2, 48000);

    engine.setBpm(90.0);                         // quarter note -> 32000 samples
    StereoBuffer glide(192000);                  // 4 s: glide fully settles
    engine.process(glide.channels.data(), 2, 192000);

    StereoBuffer probe(32100);
    probe.left[0] = 1.0f;
    probe.right[0] = 1.0f;
    engine.process(probe.channels.data(), 2, 32100);
    CHECK(probe.left[32000] == Approx(1.0f).margin(1e-2f));
}

TEST_CASE("motion depth changes the wet output, zero depth does not") {
    auto renderWithDepth = [](float depth) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 100.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);
        engine.setDuckAmount(0.0f);
        engine.setModulation(depth, 4.0f);
        StereoBuffer buf(9600);
        for (int n = 0; n < 9600; ++n)
            buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
                std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(n) / 48000.0f);
        engine.process(buf.channels.data(), 2, 9600);
        return buf.left;
    };

    const auto unmodulated = renderWithDepth(0.0f);
    const auto unmodulated2 = renderWithDepth(0.0f);
    const auto modulated = renderWithDepth(0.5f);

    double diffZero = 0.0, diffMod = 0.0;
    for (int n = 0; n < 9600; ++n) {
        diffZero += std::abs(unmodulated[static_cast<size_t>(n)] - unmodulated2[static_cast<size_t>(n)]);
        diffMod += std::abs(unmodulated[static_cast<size_t>(n)] - modulated[static_cast<size_t>(n)]);
        REQUIRE(std::isfinite(modulated[static_cast<size_t>(n)]));
    }
    CHECK(diffZero == Approx(0.0));
    CHECK(diffMod > 1.0);
}

TEST_CASE("low-cut filter drains a sustained wet signal") {
    auto tailEnergy = [](float lowCutHz) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 50.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);
        engine.setDuckAmount(0.0f);
        engine.setFilters(lowCutHz, 20000.0f);
        StereoBuffer buf(4800);
        std::fill(buf.left.begin(), buf.left.end(), 1.0f);
        std::fill(buf.right.begin(), buf.right.end(), 1.0f);
        engine.process(buf.channels.data(), 2, 4800);
        double sum = 0.0;
        for (int n = 4000; n < 4800; ++n)
            sum += std::abs(buf.left[static_cast<size_t>(n)]);
        return sum;
    };
    CHECK(tailEnergy(500.0f) < 0.1 * tailEnergy(0.0f));
}

TEST_CASE("equal-power mix keeps full-wet and full-dry exact") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setDryWet(0.0f);                      // full dry: output == input
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(64);
    buf.left[10] = 0.75f;
    buf.right[10] = 0.75f;
    engine.process(buf.channels.data(), 2, 64);
    CHECK(buf.left[10] == Approx(0.75f));
}
```

- [ ] **Step 2: Verify failure**

Run: `cmake --build build --target orbit_tests -j8`
Expected: FAIL — `setModulation`/`setFilters` not members (compile error).

- [ ] **Step 3: Implement the engine changes**

In `plugin/OrbitEngine.h`: add includes `"dsp/Lfo.h"`, `"dsp/OnePole.h"`; add public constant and methods; add members. Full new public/private sections (keep everything not shown here as-is):

```cpp
    static constexpr float kMaxModSeconds = 0.002f;   // add next to existing constants

    void setModulation(float depth01, float rateHz);  // depth 0..1 -> up to +/-2 ms
    void setFilters(float lowCutHz, float highCutHz); // lowCut <= 0 off; highCut >= 20000 off
```

New private members (alongside existing ones):

```cpp
    dsp::Lfo lfo_;
    float modDepthSamples_ = 0.0f;
    float lowCutHz_ = 0.0f;
    float highCutHz_ = 20000.0f;
    std::array<dsp::OnePole, kMaxChannels> lowCutFilters_;
    std::array<dsp::OnePole, kMaxChannels> highCutFilters_;
    float dryGain_ = 1.0f;
    float wetGain_ = 0.0f;
```

In `plugin/OrbitEngine.cpp`:

`prepare()` additions (after `ducker_.prepare`):

```cpp
    lfo_.prepare(sampleRate);
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        lowCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::HighPass);
        highCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::LowPass);
    }
```

`reset()` additions: `lfo_.reset();` and reset all four filters.

`setDryWet` becomes equal-power:

```cpp
void OrbitEngine::setDryWet(float mix01) {
    mix_ = std::clamp(mix01, 0.0f, 1.0f);
    dryGain_ = std::sqrt(1.0f - mix_);
    wetGain_ = std::sqrt(mix_);
}
```

New setters:

```cpp
void OrbitEngine::setModulation(float depth01, float rateHz) {
    const float depth = std::clamp(depth01, 0.0f, 1.0f);
    modDepthSamples_ = depth * kMaxModSeconds * static_cast<float>(sampleRate_);
    lfo_.setRate(rateHz);
}

void OrbitEngine::setFilters(float lowCutHz, float highCutHz) {
    lowCutHz_ = lowCutHz;
    highCutHz_ = highCutHz;
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        lowCutFilters_[static_cast<size_t>(ch)].setCutoff(lowCutHz);
        highCutFilters_[static_cast<size_t>(ch)].setCutoff(highCutHz);
    }
}
```

`process()` inner loop becomes:

```cpp
    for (int n = 0; n < numSamples; ++n) {
        float dryLevel = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            dryLevel += std::abs(channelData[ch][n]);
        dryLevel /= static_cast<float>(channels);
        const float duckGain = ducker_.processGain(dryLevel);

        const float modOffset = lfo_.processSample() * modDepthSamples_;

        for (int ch = 0; ch < channels; ++ch) {
            const float dry = channelData[ch][n];
            float wet = 0.0f;
            for (int t = 0; t < kNumTaps; ++t) {
                auto& line = lines_[static_cast<size_t>(t)][static_cast<size_t>(ch)];
                line.setModulationSamples(modOffset);
                const float tapOut = line.processSample(dry);
                if (taps_[static_cast<size_t>(t)].enabled)
                    wet += tapOut;
            }
            if (lowCutHz_ > 0.0f)
                wet = lowCutFilters_[static_cast<size_t>(ch)].processSample(wet);
            if (highCutHz_ < 20000.0f)
                wet = highCutFilters_[static_cast<size_t>(ch)].processSample(wet);
            channelData[ch][n] = dry * dryGain_ + wet * wetGain_ * duckGain;
        }
    }
```

Backward-compatibility notes for the reviewer: default `modDepthSamples_ = 0` and filter defaults (0 / 20000) skip both new paths; equal-power gains at mix 1.0 are (0, 1) and at 0.0 are (1, 0) — identical to the linear mix at the extremes the existing tests use; the ducking test compares energies of two otherwise-identical engines, so the sqrt curve cancels.

- [ ] **Step 4: Run tests — all 8 engine cases (4 old + 4 new) and everything else must pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add plugin/OrbitEngine.h plugin/OrbitEngine.cpp tests/plugin/OrbitEngineTests.cpp
git commit -m "feat(plugin): motion LFO, wet low/high-cut filters, equal-power mix, tempo glide"
```

---

### Task 4: Parameters, processor wiring, validation gates

**Files:**
- Modify: `plugin/Parameters.h`, `plugin/Parameters.cpp`
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `OrbitEngine::setModulation`, `setFilters` (Task 3).
- Produces: 4 new parameter IDs (`mod_depth`, `mod_rate`, `filter_lowcut`, `filter_highcut` — 22 total), cached pointers extended, `setStateInformation` reads `stateVersion` into a documented migration switch point (still version 1), runtime assertion tying `kSyncChoices.size()` to `SyncDivision::NumDivisions`.

- [ ] **Step 1: Add IDs to `plugin/Parameters.h`** (below the existing `kDuckId`):

```cpp
inline constexpr auto kModDepthId = "mod_depth";
inline constexpr auto kModRateId  = "mod_rate";
inline constexpr auto kLowCutId   = "filter_lowcut";
inline constexpr auto kHighCutId  = "filter_highcut";
```

- [ ] **Step 2: Extend `plugin/Parameters.cpp` layout** (after the `kDuckId` push_back, inside `createLayout`):

```cpp
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kModDepthId, 1), "Motion Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kModRateId, 1), "Motion Rate",
        juce::NormalisableRange<float>(0.1f, 8.0f, 0.0f, 0.5f), 0.5f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kLowCutId, 1), "Low Cut",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 0.0f, 0.4f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kHighCutId, 1), "High Cut",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 0.0f, 0.4f), 20000.0f));
```

Also add at the top of `createLayout` (with `#include "dsp/TempoSync.h"` added to the includes):

```cpp
    // Enum-order contract: the sync choice list must track SyncDivision exactly.
    jassert(kSyncChoices.size()
            == static_cast<int>(orbit::dsp::SyncDivision::NumDivisions));
```

- [ ] **Step 3: Extend the processor**

`plugin/PluginProcessor.h` — add four cached pointers below `duckParam_`:

```cpp
    std::atomic<float>* modDepthParam_ = nullptr;
    std::atomic<float>* modRateParam_ = nullptr;
    std::atomic<float>* lowCutParam_ = nullptr;
    std::atomic<float>* highCutParam_ = nullptr;
```

`plugin/PluginProcessor.cpp` — constructor additions (after `duckParam_` line):

```cpp
    modDepthParam_ = apvts.getRawParameterValue(params::kModDepthId);
    modRateParam_  = apvts.getRawParameterValue(params::kModRateId);
    lowCutParam_   = apvts.getRawParameterValue(params::kLowCutId);
    highCutParam_  = apvts.getRawParameterValue(params::kHighCutId);
```

`updateEngineFromParameters()` additions (after the duck line):

```cpp
    engine_.setModulation(modDepthParam_->load(), modRateParam_->load());
    engine_.setFilters(lowCutParam_->load(), highCutParam_->load());
```

`setStateInformation` — after the product check, before `replaceState`:

```cpp
    // Migration switch point: v1 is current. When stateVersion 2 exists,
    // transform older trees here before replaceState.
    const int loadedVersion = static_cast<int>(tree.getProperty("stateVersion", 1));
    juce::ignoreUnused(loadedVersion);
```

- [ ] **Step 4: Full gate re-run**

Run:
```bash
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone orbit_tests -j8
./build/tests/orbit_tests
auval -v aufx Orb1 Orba
./scripts/run_pluginval.sh
```
Expected: clean build; all unit tests pass; `AU VALIDATION SUCCEEDED`; pluginval `ALL TESTS PASSED` / SUCCESS. (If auval fails with a stale-cache error, `killall -9 AudioComponentRegistrar` and retry once.)

- [ ] **Step 5: Commit**

```bash
git add plugin/
git commit -m "feat(plugin): motion and filter parameters, stateVersion switch point, sync-choice assertion"
```

---

## Self-Review Notes

- **Deferred-item coverage:** readFractional precision split ✓ (T1); delay-time smoothing + bpm-change regression ✓ (T1/T3); equal-power crossfade ✓ (T3); stateVersion migration switch ✓ (T4); kSyncChoices↔enum assertion ✓ (T4). Remaining Phase-1 deferrals (pre-prepare guards, tail-length policy, state round-trip test) stay on the Phase 2 Wave 2 / Phase 3 list.
- **Type consistency:** `setModulationSamples(float)` (T1) ← engine per-sample call (T3); `OnePole::processSample(float)` / `Lfo::processSample()` (T2) ← T3; new param IDs (T4) match Global Constraints.
- **Backward compatibility argument:** snap-before-running preserves every existing DelayLine/engine test; neutral defaults preserve the Phase 1 sound; APVTS supplies defaults for absent params in old sessions.
