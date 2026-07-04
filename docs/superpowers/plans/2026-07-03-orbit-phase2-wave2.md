# Orbit Phase 2 Wave 2 — The Creative Suite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Orbit's headline creative features — character modes (Clean/Tape/Grit), true cross-feedback ping-pong, stereo width, Freeze, per-tap Reverse, and per-tap pitch-shifted repeats (±12 st shimmer) — completing the spec §6 parameter contract (34 params).

**Architecture:** `DelayLine` gains a low-level `read()`/`writeAndAdvance()` API (Task 3) that unlocks cross-channel feedback (ping-pong), freeze (recirculating writes), and the Task 5 read-head modes (reverse chunk playback, dual-head grain pitch). A new `CharacterStage` core primitive colors the wet path. Wet chain order: tap sum → character → low/high-cut → width (mid/side) → duck × wetGain. Everything stays JUCE-free and unit-tested; the processor wires 12 new parameters.

**Tech Stack:** unchanged (C++17, JUCE 8.0.4, Catch2, CMake).

## Global Constraints

- All Wave 1 constraints remain (no JUCE in `core/`/`plugin/OrbitEngine.*`; no allocation/locks/logging in per-sample paths; namespaces; conventional commits).
- **New parameter IDs (exact):** `tap1_reverse`…`tap4_reverse` (Bool, default false), `tap1_pitch`…`tap4_pitch` (Float −12..+12 st, default 0), `character_mode` (Choice {"Clean","Tape","Grit"}, default Clean), `mix_width` (Float 0..2, default 1), `mix_pingpong` (Bool, default false), `freeze_active` (Bool, default false). Total params: 34.
- **Backward compatibility binding:** all 32 existing test cases pass UNCHANGED; neutral defaults (Clean, width 1, ping-pong off, freeze off, reverse off, pitch 0) keep default output bit-identical to Wave 1.
- **Reverse+pitch combination:** if a tap's reverse is on, its pitch is ignored (v1 rule; document in code).
- **Reverse chunk length** = tap delay clamped to [256 samples, (bufferSize−2)/2]. **Pitch grain window** W = clamp(tap delay, 256, 0.1·sr) samples, two heads, triangular crossfade.
- Branch: `dev/phase-2-wave-2` from main. Do not push until delivery.

---

### Task 1: Hardening batch (Wave 1 backlog)

**Files:** Modify `core/dsp/TempoSync.h`, `plugin/Parameters.h`, `plugin/Parameters.cpp`, `core/dsp/OnePole.cpp`, `plugin/OrbitEngine.h`, `plugin/OrbitEngine.cpp`, `core/dsp/DelayLine.cpp`; tests in `tests/core/OnePoleTests.cpp`, `tests/plugin/OrbitEngineTests.cpp`.

**Interfaces produced:** `orbit::dsp::kSyncDivisionNames` (constexpr `const char*` array in TempoSync.h with `static_assert` count == NumDivisions — the enum contract becomes compile-time); `kSyncChoices` in Parameters.h rebuilt from it; OnePole flushes denormals; engine scales Motion depth per tap so short taps never flat-top; filters reset when toggled off; `readFractional` invariant comment.

- [ ] **Step 1 (TDD where applicable): add failing tests**

To `tests/core/OnePoleTests.cpp`:

```cpp
TEST_CASE("OnePole decays to hard zero, not denormals") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::LowPass);
    f.setCutoff(100.0f);
    f.processSample(1.0f);
    float y = 1.0f;
    for (int n = 0; n < 480000; ++n)
        y = f.processSample(0.0f);
    CHECK(y == 0.0f);   // exactly zero after long silence
}
```

To `tests/plugin/OrbitEngineTests.cpp`:

```cpp
TEST_CASE("motion on a very short tap stays finite and does not flat-top hard") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 0.001f;               // 48 samples < max mod (96 samples)
    tap.feedback = 0.5f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    engine.setModulation(1.0f, 8.0f);
    StereoBuffer buf(48000);
    for (int n = 0; n < 48000; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
    engine.process(buf.channels.data(), 2, 48000);
    for (int n = 0; n < 48000; ++n)
        REQUIRE(std::isfinite(buf.left[static_cast<size_t>(n)]));
}
```

- [ ] **Step 2: implement**

1. `core/dsp/TempoSync.h`: add `#include <cstddef>` and, below the enum:

```cpp
// Display names for SyncDivision, index-aligned with the enum. The
// static_assert makes the enum<->choice-list contract compile-time.
inline constexpr const char* kSyncDivisionNames[] = {
    "Free", "1/1", "1/2", "1/4", "1/8", "1/16",
    "1/4 D", "1/8 D", "1/4 T", "1/8 T"
};
static_assert(sizeof(kSyncDivisionNames) / sizeof(kSyncDivisionNames[0])
                  == static_cast<std::size_t>(SyncDivision::NumDivisions),
              "kSyncDivisionNames must track SyncDivision exactly");
```

2. `plugin/Parameters.h`: add `#include "dsp/TempoSync.h"`; replace the `kSyncChoices` literal list with:

```cpp
inline const juce::StringArray kSyncChoices = [] {
    juce::StringArray choices;
    for (auto* name : orbit::dsp::kSyncDivisionNames)
        choices.add(name);
    return choices;
}();
```

`plugin/Parameters.cpp`: the `jassert` is now redundant — replace it with a comment pointing at the static_assert in TempoSync.h, and drop the now-unneeded include if unused otherwise.

3. `core/dsp/OnePole.cpp` `processSample`: after the state update add

```cpp
    if (!(std::abs(state_) >= 1.0e-12f))   // denormal/NaN flush, matches DelayLine policy
        state_ = 0.0f;
```

(add `<cmath>` if missing; note `!(x >= t)` catches NaN too).

4. `plugin/OrbitEngine.*` — per-tap Motion headroom scale: add member `std::array<float, kNumTaps> modScale_ {};` computed in `applyTapTime(index)`:

```cpp
    const float delaySamples = seconds * static_cast<float>(sampleRate_);
    const float maxMod = kMaxModSeconds * static_cast<float>(sampleRate_);
    modScale_[static_cast<size_t>(index)] =
        maxMod > 0.0f ? std::min(1.0f, std::max(0.0f, delaySamples - 2.0f) / maxMod) : 0.0f;
```

and in `process()` apply per tap: `line.setModulationSamples(modOffset * modScale_[t])`. The LFO keeps its sine shape; short taps just get proportionally less depth — no clamp flat-topping.

5. `plugin/OrbitEngine.cpp` `setFilters`: when a filter transitions to off (`lowCutHz <= 0` or `highCutHz >= 20000`) call `reset()` on that filter pair so re-enable starts from clean state.

6. `core/dsp/DelayLine.cpp` `readFractional`: add the invariant comment above the clamp:

```cpp
    // INVARIANT (do not weaken): effective is clamped to [1, size-2] so that
    // `newer` trails the write head by >= 1 sample and `older` = newer-1 stays
    // in valid history given the +2 headroom allocated in prepare(). All read
    // modes added later must preserve this bound.
```

- [ ] **Step 3: build, run all tests (expect green incl. 2 new), commit**

```bash
cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests
git add core/ plugin/ tests/
git commit -m "fix: wave-1 hardening — compile-time sync contract, denormal flush, mod headroom, filter reset"
```

---

### Task 2: CharacterStage (core) — Clean / Tape / Grit

**Files:** Create `core/dsp/CharacterStage.h`, `core/dsp/CharacterStage.cpp`, `tests/core/CharacterStageTests.cpp`; modify `core/CMakeLists.txt`, `tests/CMakeLists.txt`.

**Interfaces produced:** `orbit::dsp::CharacterStage` — `enum class Mode { Clean = 0, Tape, Grit, NumModes }`, `void prepare(double sampleRate)`, `void reset()`, `void setMode(Mode)`, `float processSample(float input)`. Enum order is a contract with `character_mode` choice (Task 6). Per-channel instance; RT-safe.

**Algorithm (implement exactly):**
- **Clean:** return input unchanged (true bypass).
- **Tape:** `sat = std::tanh(1.5f * x) / 1.5f` then one-pole LP at 7500 Hz (internal OnePole). (normalization corrected from tanh(1.5) to 1.5 during implementation — the original denominator fails the binding low-level linearity test `outSmall ≈ 0.1 ± 0.02`; adjudicated at Task 2 review)
- **Grit:** `sat = x / (1.0f + std::abs(1.8f * x))` scaled by `(1.0f + 1.8f)` (normalized soft-clip, harder knee), then one-pole LP at 3500 Hz, then add gated noise: `noise = (xorshift32 float in ±1) * 0.003f * envelope` where `envelope` is a 10 ms/200 ms follower of `|input|` (reuse the Ducker's coefficient formula inline; noise is exactly 0 when input is silent so a default-state plugin stays silent). xorshift32 seeded with a fixed constant (deterministic tests): `state = state ^ (state << 13); state ^= state >> 17; state ^= state << 5;` mapped to [-1, 1].

- [ ] **Step 1: failing tests** (`tests/core/CharacterStageTests.cpp`):

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/CharacterStage.h"

using Catch::Approx;
using orbit::dsp::CharacterStage;

TEST_CASE("Clean mode is a true bypass") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Clean);
    for (int n = 0; n < 100; ++n) {
        const float x = std::sin(0.1f * static_cast<float>(n)) * 1.3f;
        REQUIRE(stage.processSample(x) == x);
    }
}

TEST_CASE("Tape mode saturates softly and stays bounded") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Tape);
    float outAt2 = 0.0f;
    for (int n = 0; n < 100; ++n)
        outAt2 = stage.processSample(2.0f);
    CHECK(outAt2 < 1.2f);
    CHECK(outAt2 > 0.5f);
    stage.reset();
    float outSmall = 0.0f;
    for (int n = 0; n < 100; ++n)
        outSmall = stage.processSample(0.1f);
    CHECK(outSmall == Approx(0.1f).margin(0.02f));   // near-linear at low level
}

TEST_CASE("Tape mode rolls off highs harder than Clean") {
    auto nyquistGain = [](CharacterStage::Mode mode) {
        CharacterStage stage;
        stage.prepare(48000.0);
        stage.setMode(mode);
        float sumAbs = 0.0f;
        for (int n = 0; n < 4800; ++n) {
            const float y = stage.processSample((n % 2 == 0) ? 0.5f : -0.5f);
            if (n >= 4700) sumAbs += std::abs(y);
        }
        return sumAbs;
    };
    CHECK(nyquistGain(CharacterStage::Mode::Tape) < 0.5f * nyquistGain(CharacterStage::Mode::Clean));
}

TEST_CASE("Grit noise is gated: silence in, silence out") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Grit);
    for (int n = 0; n < 48000; ++n)
        REQUIRE(stage.processSample(0.0f) == 0.0f);
}

TEST_CASE("Grit adds a noise floor while signal is present") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Grit);
    // steady input -> settled output would be constant without noise; variance implies noise
    float prev = stage.processSample(0.5f);
    float maxDelta = 0.0f;
    for (int n = 0; n < 4800; ++n) {
        const float y = stage.processSample(0.5f);
        if (n > 4000) maxDelta = std::max(maxDelta, std::abs(y - prev));
        prev = y;
    }
    CHECK(maxDelta > 1e-4f);
    CHECK(maxDelta < 0.05f);
}
```

- [ ] **Step 2: verify RED** (header not found), **Step 3: implement per the algorithm above** (header + cpp, wire into `core/CMakeLists.txt` + `tests/CMakeLists.txt`), **Step 4: all green**, **Step 5: commit**

```bash
git commit -m "feat(core): CharacterStage — clean/tape/grit coloring"
```

---

### Task 3: DelayLine read/write split + engine ping-pong & width

**Files:** Modify `core/dsp/DelayLine.h`, `core/dsp/DelayLine.cpp`, `plugin/OrbitEngine.h`, `plugin/OrbitEngine.cpp`; tests in `tests/core/DelayLineTests.cpp`, `tests/plugin/OrbitEngineTests.cpp`.

**Interfaces produced:**
- `DelayLine`: `float read() const;` (current effective read position — glide + mod applied; does NOT advance) and `void writeAndAdvance(float value);` (applies the NaN/denormal flush, writes, advances write head AND the glide state, sets running). `processSample(input)` is rewritten as exactly `{ const float out = read(); writeAndAdvance(input + out * feedback_); return out; }`. Note: this moves the per-sample glide update from before-read to after-read — a one-sample phase shift in glide trajectories; all existing tests tolerate it (constant-target tests are bit-identical; the glide test uses 130-sample windows).
- `OrbitEngine`: `void setPingPong(bool)`, `void setWidth(float width01to2)`. Ping-pong (stereo only): per tap, feedback crosses channels — `lineL.writeAndAdvance(inL + outR * fb); lineR.writeAndAdvance(inR + outL * fb);`. Width: after character+filters, mid/side on the wet pair: `mid = (wetL+wetR)*0.5f; side = (wetL-wetR)*0.5f * width_; wetL = mid+side; wetR = mid-side;` (width 1 = passthrough; skip when `channels < 2`). The engine's per-sample loop is restructured to read all taps/channels first, then write — required for cross-feedback; with ping-pong off, writes use the same channel's out (behavior identical to before).

- [ ] **Step 1: failing tests**

`tests/core/DelayLineTests.cpp`:

```cpp
TEST_CASE("read/writeAndAdvance compose to processSample behavior") {
    DelayLine a, b;
    a.prepare(48000.0, 1.0f);
    b.prepare(48000.0, 1.0f);
    a.setDelaySeconds(10.0f / 48000.0f);
    b.setDelaySeconds(10.0f / 48000.0f);
    a.setFeedback(0.5f);
    for (int n = 0; n < 60; ++n) {
        const float x = (n == 0) ? 1.0f : 0.0f;
        const float viaProcess = a.processSample(x);
        const float out = b.read();
        b.writeAndAdvance(x + out * 0.5f);
        REQUIRE(viaProcess == out);
    }
}
```

`tests/plugin/OrbitEngineTests.cpp`:

```cpp
TEST_CASE("ping-pong bounces the echo between channels") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 100.0f / 48000.0f;
    tap.feedback = 0.7f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    engine.setPingPong(true);
    StereoBuffer buf(350);
    buf.left[0] = 1.0f;                       // impulse on LEFT only
    engine.process(buf.channels.data(), 2, 350);
    CHECK(buf.right[100] == Approx(0.0f).margin(1e-4f));   // 1st echo: left
    CHECK(buf.left[100] == Approx(1.0f).margin(1e-2f));
    CHECK(std::abs(buf.right[200]) > 0.5f);                 // 2nd echo: crossed to right
    CHECK(std::abs(buf.left[200]) < 1e-3f);
    CHECK(std::abs(buf.left[300]) > 0.3f);                  // 3rd echo: back to left
}

TEST_CASE("width narrows or spreads the wet stereo image") {
    auto sideEnergy = [](float width) {
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
        engine.setWidth(width);
        StereoBuffer buf(200);
        buf.left[0] = 1.0f;                   // left-only impulse -> wet has side content
        engine.process(buf.channels.data(), 2, 200);
        double side = 0.0;
        for (int n = 0; n < 200; ++n)
            side += std::abs(buf.left[static_cast<size_t>(n)] - buf.right[static_cast<size_t>(n)]);
        return side;
    };
    const auto normal = sideEnergy(1.0f);
    CHECK(sideEnergy(0.0f) == Approx(0.0).margin(1e-6));    // mono-ized wet
    CHECK(sideEnergy(2.0f) > 1.5 * normal);                  // spread
}
```

Ping-pong trace justifying the expectations (include as a comment for the reviewer): impulse L at n=0 → both lines get their channel's dry; L line holds the impulse. Echo 1 (n=100): outL=1, outR=0 → heard LEFT; cross-write puts fb·outL into R line. Echo 2 (n=200): outR=0.7 → RIGHT. Echo 3 (n=300): back LEFT at 0.49.

- [ ] **Step 2: verify RED** (no `read()`/`setPingPong` members), **Step 3: implement**, **Step 4: ALL tests green (every pre-existing case unchanged — ping-pong off must reproduce the old topology exactly)**, **Step 5: commit**

```bash
git commit -m "feat: DelayLine read/write split; engine ping-pong and stereo width"
```

---

### Task 4: Freeze

**Files:** Modify `plugin/OrbitEngine.h`, `plugin/OrbitEngine.cpp`; tests in `tests/plugin/OrbitEngineTests.cpp`.

**Interfaces produced:** `OrbitEngine::setFreeze(bool)`. While frozen, every line recirculates: per sample `out = line.read(); line.writeAndAdvance(out);` — input stops entering the lines, the captured audio loops at unity forever (flush keeps it clean), taps keep their positions, dry path/ducking unaffected. Unfreeze resumes normal writes seamlessly.

- [ ] **Step 1: failing test**

```cpp
TEST_CASE("freeze loops the captured audio indefinitely and ignores new input") {
    auto renderFrozen = [](float postFreezeInput) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 100.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);                       // wet only: dry removed
        engine.setDuckAmount(0.0f);
        StereoBuffer capture(100);
        for (int n = 0; n < 100; ++n)                 // deterministic "audio" to capture
            capture.left[static_cast<size_t>(n)] = capture.right[static_cast<size_t>(n)] =
                std::sin(0.37f * static_cast<float>(n));
        engine.process(capture.channels.data(), 2, 100);
        engine.setFreeze(true);
        StereoBuffer frozen(1000);
        std::fill(frozen.left.begin(), frozen.left.end(), postFreezeInput);
        std::fill(frozen.right.begin(), frozen.right.end(), postFreezeInput);
        engine.process(frozen.channels.data(), 2, 1000);
        return frozen.left;
    };

    const auto quiet = renderFrozen(0.0f);
    const auto loud = renderFrozen(0.9f);
    double loopEnergyFirst = 0.0, loopEnergyLast = 0.0;
    for (int n = 0; n < 500; ++n)
        loopEnergyFirst += std::abs(quiet[static_cast<size_t>(n)]);
    for (int n = 500; n < 1000; ++n)
        loopEnergyLast += std::abs(quiet[static_cast<size_t>(n)]);
    CHECK(loopEnergyFirst > 1.0);                                   // something is looping
    CHECK(loopEnergyLast == Approx(loopEnergyFirst).epsilon(0.05)); // no decay
    for (int n = 0; n < 1000; ++n)                                  // input doesn't leak into wet
        REQUIRE(quiet[static_cast<size_t>(n)] == Approx(loud[static_cast<size_t>(n)]).margin(1e-4f));
}
```

- [ ] **Step 2: RED** (no `setFreeze`), **Step 3: implement** (a `frozen_` flag branching the write in the per-sample loop; ping-pong's cross-write is bypassed while frozen — each line rewrites its own read), **Step 4: all green**, **Step 5: commit**

```bash
git commit -m "feat(plugin): freeze — infinite recirculating capture"
```

---

### Task 5: Per-tap Reverse and pitch-shifted repeats (DelayLine read modes)

**Files:** Modify `core/dsp/DelayLine.h`, `core/dsp/DelayLine.cpp`, `plugin/OrbitEngine.h`, `plugin/OrbitEngine.cpp` (plumb per-tap settings); tests in `tests/core/DelayLineTests.cpp`, `tests/plugin/OrbitEngineTests.cpp`.

> **Implementer latitude:** this is the one task where the plan specifies the algorithm and the tests, not verbatim code. You own the internals; the API, the math below, the invariant comment from Task 1, and the tests are binding. If the specified crossfade constants produce test failures, tune constants — never weaken tests.

**API (binding):**

```cpp
// DelayLine additions
enum class ReadMode { Normal = 0, Reverse };
void setReadMode(ReadMode mode);
void setPitchSemitones(float semitones);   // clamped [-12, +12]; 0 disables; ignored while Reverse
```

`TapSettings` gains `bool reverse = false; float pitchSemitones = 0.0f;` and `OrbitEngine::setTap` forwards them to all the tap's lines.

**Reverse algorithm (binding):** chunked backward playback. Chunk length `L = clamp(round(targetDelaySamples), 256, (size-2)/2)`, recomputed when the target delay changes (not per sample). A per-line counter `j = 0..L-1`: effective read delay `d(j) = 2*j + 1` (plays the just-written chunk backwards — classic real-time reverse). At each chunk boundary crossfade over `C = 128` samples: during the first C samples of a chunk, crossfade linearly from the OLD trajectory continued (`d = 2*(L + j) + 1`) to the new one, reading both positions. All reads go through a `readAt(double effectiveDelay)` helper that enforces the Task 1 invariant clamp [1, size−2]. Feedback still writes `input + out*feedback` (repeats alternate direction — accepted, musical). Glide/mod offsets are ignored in Reverse mode (chunk timing is authoritative).

**Pitch algorithm (binding):** dual-head grain reader (delay-line pitch shifter). Rate `r = 2^(semitones/12)`. Window `W = clamp(targetDelaySamples, 256, 0.1*sr)` samples. Two heads with phases `p0, p1 = p0 + 0.5 (mod 1)`; per sample each head's extra delay is `e_i = p_i * W`, phases advance by `(1 - r)/W` per sample (wrapping mod 1 — for r>1 phases run backward, that's fine with fmod-style wrap into [0,1)). Head weight is triangular: `w_i = 1 - |2*p_i - 1|`, normalized so `w0 + w1 = 1` (they are complementary for exactly-0.5-offset phases; still normalize defensively). `out = w0 * readAt(base + e0) + w1 * readAt(base + e1)` where `base` = glide/mod effective delay. `semitones == 0` must take the exact Normal fast path (bit-identical output).

- [ ] **Step 1: failing tests**

`tests/core/DelayLineTests.cpp`:

```cpp
TEST_CASE("Reverse mode plays chunks backwards") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(400.0f / 48000.0f);
    line.setReadMode(DelayLine::ReadMode::Reverse);
    std::vector<float> out;
    for (int n = 0; n < 1200; ++n) {
        const float ramp = static_cast<float>(n) / 1200.0f;   // ascending input
        out.push_back(line.processSample(ramp));
    }
    int descending = 0, counted = 0;
    for (int n = 560; n < 780; ++n) {                          // inside a chunk, clear of crossfades
        ++counted;
        if (out[static_cast<size_t>(n + 1)] < out[static_cast<size_t>(n)]) ++descending;
    }
    CHECK(descending > counted * 9 / 10);   // ascending in, descending out
}

TEST_CASE("Reverse mode with feedback stays bounded") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(400.0f / 48000.0f);
    line.setReadMode(DelayLine::ReadMode::Reverse);
    line.setFeedback(0.9f);
    for (int n = 0; n < 96000; ++n) {
        const float y = line.processSample(n < 400 ? 0.5f : 0.0f);
        REQUIRE(std::isfinite(y));
        REQUIRE(std::abs(y) < 10.0f);
    }
}

TEST_CASE("Pitch +12 doubles the frequency of repeats") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(4800.0f / 48000.0f);
    line.setPitchSemitones(12.0f);
    std::vector<float> out;
    for (int n = 0; n < 14400; ++n) {
        const float x = std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
        out.push_back(line.processSample(x));
    }
    int crossings = 0;
    for (int n = 9600; n < 14399; ++n)                          // settled window, 0.1 s
        if ((out[static_cast<size_t>(n)] >= 0.0f) != (out[static_cast<size_t>(n + 1)] >= 0.0f))
            ++crossings;
    // 220 Hz doubled to 440 Hz -> ~88 crossings per 4800 samples; grain crossfades tolerated
    CHECK(crossings > 74);
    CHECK(crossings < 102);
}

TEST_CASE("Pitch 0 is bit-identical to Normal mode") {
    DelayLine a, b;
    a.prepare(48000.0, 1.0f);
    b.prepare(48000.0, 1.0f);
    a.setDelaySeconds(100.0f / 48000.0f);
    b.setDelaySeconds(100.0f / 48000.0f);
    b.setPitchSemitones(0.0f);
    for (int n = 0; n < 500; ++n) {
        const float x = std::sin(0.05f * static_cast<float>(n));
        REQUIRE(a.processSample(x) == b.processSample(x));
    }
}
```

`tests/plugin/OrbitEngineTests.cpp`:

```cpp
TEST_CASE("per-tap reverse and pitch flow through TapSettings") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 400.0f / 48000.0f;
    tap.feedback = 0.0f;
    tap.reverse = true;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(1200);
    for (int n = 0; n < 1200; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            static_cast<float>(n) / 1200.0f;
    engine.process(buf.channels.data(), 2, 1200);
    int descending = 0, counted = 0;
    for (int n = 560; n < 780; ++n) {
        ++counted;
        if (buf.left[static_cast<size_t>(n + 1)] < buf.left[static_cast<size_t>(n)]) ++descending;
    }
    CHECK(descending > counted * 9 / 10);
}
```

- [ ] **Step 2: RED**, **Step 3: implement** (Normal path must remain the exact existing code path — guard the new modes behind `readMode_ != Normal || pitchRatio_ != 1`), **Step 4: ALL tests green including every pre-existing case bit-identical**, **Step 5: commit**

```bash
git commit -m "feat: per-tap reverse playback and grain pitch-shifted repeats"
```

---

### Task 6: Parameters (34), processor wiring, gates

**Files:** Modify `plugin/Parameters.h`, `plugin/Parameters.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`.

**Interfaces produced:** IDs per Global Constraints (`tapN_reverse`, `tapN_pitch`, `character_mode` with choices built from `CharacterStage::Mode` order {"Clean","Tape","Grit"}, `mix_width` 0..2 default 1, `mix_pingpong`, `freeze_active`). Engine gets `setCharacterMode(dsp::CharacterStage::Mode)` (wet path per channel: tap sum → character → filters → width — Task 2's stage plumbed here if not already). Processor extends `TapParamPointers` with `reverse`/`pitch` and adds 4 global cached pointers; `updateEngineFromParameters` forwards everything. Layout count: 34.

- [ ] **Step 1:** add IDs + layout entries (Bool `tapN_reverse` default false; Float `tapN_pitch` −12..+12 step 1.0 default 0 — use `NormalisableRange<float>(-12.0f, 12.0f, 1.0f)` for semitone snapping; Choice `character_mode` {"Clean","Tape","Grit"} default 0; Float `mix_width` 0..2 default 1; Bool `mix_pingpong` default false; Bool `freeze_active` default false).
- [ ] **Step 2:** plumb `CharacterStage` into the engine wet chain (per channel, between tap sum and filters) with `setCharacterMode`; neutral default Clean.
- [ ] **Step 3:** extend cached pointers + `updateEngineFromParameters` (reverse/pitch into `TapSettings`; mode cast `static_cast<dsp::CharacterStage::Mode>(int)`; width/pingpong/freeze setters). Zero string building in processBlock.
- [ ] **Step 4: full gates**

```bash
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone orbit_tests -j8
./build/tests/orbit_tests
auval -v aufx Orb1 Orba          # expect 34 Global Scope Parameters, VALIDATION SUCCEEDED
./scripts/run_pluginval.sh       # expect SUCCESS
```

- [ ] **Step 5: commit**

```bash
git commit -m "feat(plugin): creative-suite parameters — reverse, pitch, character, width, ping-pong, freeze"
```

---

## Self-Review Notes

- Spec §5/§6 coverage after this wave: Echo Pad engine ✓ (pad itself is UI/Phase 3), character modes ✓, reverse ✓, pitch ✓, freeze ✓, Motion ✓ (W1), mix section complete (dry/wet, width, ping-pong, ducking) ✓, filters ✓ (W1), presets = Phase 3. UI contract §6 parameter list is now fully implemented.
- Type consistency: `TapSettings{enabled, sync, timeSeconds, feedback, reverse, pitchSemitones}` (T5) ↔ processor (T6); `CharacterStage::Mode` order ↔ `character_mode` choices (T2/T6); `read()/writeAndAdvance` (T3) ← freeze (T4) and ping-pong (T3).
- Known accepted behaviors (document, don't fix): reverse repeats alternate direction with feedback; reverse ignores pitch and Motion; freeze bypasses cross-feedback.
