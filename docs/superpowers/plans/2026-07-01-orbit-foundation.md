# Orbit Foundation (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A loadable, DAW-tested VST3/AU/Standalone delay plugin with a 4-tap tempo-syncable delay engine, sidechain ducking, and a full host-automatable parameter layer — the foundation every later phase builds on.

**Architecture:** Pure-C++ (JUCE-free) DSP lives in `core/` (product-family library) and `plugin/OrbitEngine` (Orbit's 4-tap topology), both fully unit-tested with Catch2. A thin JUCE `AudioProcessor` shell binds an APVTS parameter layer to the engine and serializes versioned state. UI is a temporary `GenericAudioProcessorEditor` (real UI arrives in a later phase from Jose's design).

**Tech Stack:** C++17, JUCE 8.0.4 (CMake FetchContent), Catch2 v3.7.1, CMake ≥ 3.22, pluginval.

**Phase roadmap (later plans, not in this document):**
- Phase 2 — Character modes (Clean/Tape/Grit), reverse, pitch-shifted repeats, freeze, modulation, repeat filters
- Phase 3 — Preset system + factory presets + UI implementation (after Jose's Claude Design handoff)
- Phase 4 — Installers, code signing, licensing, AAX (after Avid approval)

## Global Constraints

- **C++17** everywhere; JUCE pinned to tag `8.0.4`; Catch2 pinned to `v3.7.1`.
- **Real-time safety:** no heap allocation, locks, or logging inside `processSample`, `process`, or `processBlock`. All buffers sized in `prepare`.
- **`core/` and `plugin/OrbitEngine.*` must not include any JUCE header** — they are pure C++ and compile into the test binary without JUCE.
- **Namespaces:** `orbit::dsp` (core DSP), `orbit` (engine), `orbit::params` (parameter layer).
- **Parameter IDs (exact):** `tap1_enabled`…`tap4_enabled`, `tap1_time`…`tap4_time` (ms), `tap1_sync`…`tap4_sync`, `tap1_feedback`…`tap4_feedback`, `mix_drywet`, `mix_duck`. (These implement the spec §6 contract; the spec's dotted names `tap[1-4].time` etc. refer to these IDs.)
- **Feedback hard clamp:** 0.98 (`DelayLine::kMaxFeedback`). **Max delay:** 4.0 s (`OrbitEngine::kMaxDelaySeconds`).
- **Formats this phase:** VST3, AU, Standalone. No AAX yet.
- **All branding/product identity lives in `cmake/Branding.cmake`** — no company/product strings hardcoded anywhere else.
- Work in `/Users/joserodriguez/orbit-delay` on branch `main` (or feature branches merged by PR if branch protection is enabled). Conventional commits.

---

### Task 1: CMake + JUCE skeleton plugin (builds, loads, passes audio)

**Files:**
- Create: `cmake/Branding.cmake`
- Create: `CMakeLists.txt`
- Create: `core/CMakeLists.txt`
- Create: `core/dsp/Placeholder.cpp` (removed in Task 3)
- Create: `plugin/CMakeLists.txt`
- Create: `plugin/PluginProcessor.h`
- Create: `plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: CMake targets `orbit_core` (static lib) and `OrbitDelay` (plugin, formats VST3/AU/Standalone); class `OrbitAudioProcessor : juce::AudioProcessor` (passthrough, replaced in Task 7); branding variables `ORBIT_COMPANY_NAME`, `ORBIT_MANUFACTURER_CODE`, `ORBIT_PRODUCT_NAME`, `ORBIT_PLUGIN_CODE`, `ORBIT_BUNDLE_ID`.

- [ ] **Step 1: Write `cmake/Branding.cmake`**

```cmake
# Single source of truth for brand/product identity (spec §4).
# Brand name is TBD — when decided, this is the ONLY file to change.
set(ORBIT_COMPANY_NAME "Orbit Audio")       # placeholder brand
set(ORBIT_MANUFACTURER_CODE "Orba")         # 4 chars, exactly one uppercase first letter
set(ORBIT_PRODUCT_NAME "Orbit")
set(ORBIT_PLUGIN_CODE "Orb1")               # 4 chars, unique per product
set(ORBIT_BUNDLE_ID "com.orbitaudio.orbit")
```

- [ ] **Step 2: Write root `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
include(cmake/Branding.cmake)
project(OrbitDelay VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(core)
add_subdirectory(plugin)

enable_testing()
add_subdirectory(tests)
```

Note: `tests/` doesn't exist until Task 2 — create an empty `tests/CMakeLists.txt` containing only a comment line `# populated in Task 2` so configuration succeeds.

- [ ] **Step 3: Write `core/CMakeLists.txt` and placeholder source**

```cmake
add_library(orbit_core STATIC
    dsp/Placeholder.cpp)
target_include_directories(orbit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(orbit_core PUBLIC cxx_std_17)
```

`core/dsp/Placeholder.cpp` (deleted in Task 3 when real sources land):

```cpp
// Temporary translation unit so orbit_core links before real DSP exists (Task 3+).
namespace orbit::dsp { int placeholder() { return 0; } }
```

- [ ] **Step 4: Write the skeleton passthrough processor**

`plugin/PluginProcessor.h`:

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Skeleton passthrough processor — replaced with the real engine wiring in Task 7.
class OrbitAudioProcessor : public juce::AudioProcessor {
public:
    OrbitAudioProcessor()
        : AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitAudioProcessor)
};
```

`plugin/PluginProcessor.cpp`:

```cpp
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OrbitAudioProcessor();
}
```

- [ ] **Step 5: Write `plugin/CMakeLists.txt`**

```cmake
juce_add_plugin(OrbitDelay
    COMPANY_NAME "${ORBIT_COMPANY_NAME}"
    BUNDLE_ID "${ORBIT_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE "${ORBIT_MANUFACTURER_CODE}"
    PLUGIN_CODE "${ORBIT_PLUGIN_CODE}"
    PRODUCT_NAME "${ORBIT_PRODUCT_NAME}"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(OrbitDelay PRIVATE
    PluginProcessor.cpp)

target_compile_definitions(OrbitDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OrbitDelay
    PRIVATE
        orbit_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

- [ ] **Step 6: Configure and build**

Run:
```bash
cd /Users/joserodriguez/orbit-delay
cmake -B build -DCMAKE_BUILD_TYPE=Debug        # first run downloads JUCE — several minutes
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone -j8
```
Expected: build completes with no errors.

- [ ] **Step 7: Verify artefacts exist**

Run:
```bash
ls "build/plugin/OrbitDelay_artefacts/Debug/VST3/Orbit.vst3" \
   "build/plugin/OrbitDelay_artefacts/Debug/AU/Orbit.component" \
   "build/plugin/OrbitDelay_artefacts/Debug/Standalone/Orbit.app"
```
Expected: all three paths listed (COPY_PLUGIN_AFTER_BUILD also installs VST3/AU to `~/Library/Audio/Plug-Ins/`).

- [ ] **Step 8: Commit**

```bash
git add cmake/ CMakeLists.txt core/ plugin/ tests/
git commit -m "feat: CMake + JUCE skeleton plugin (VST3/AU/Standalone, passthrough)"
```

---

### Task 2: Catch2 test harness

**Files:**
- Modify: `tests/CMakeLists.txt` (replace the Task 1 stub)
- Create: `tests/core/SanityTests.cpp`

**Interfaces:**
- Consumes: `orbit_core` target from Task 1.
- Produces: test target `orbit_tests`; the pattern every later test file follows (`#include <catch2/catch_test_macros.hpp>`, files under `tests/core/` and `tests/plugin/`).

- [ ] **Step 1: Write the failing (non-building) test harness**

`tests/CMakeLists.txt`:

```cmake
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(orbit_tests
    core/SanityTests.cpp)

target_link_libraries(orbit_tests PRIVATE
    orbit_core
    Catch2::Catch2WithMain)

target_include_directories(orbit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)

include(Catch)
catch_discover_tests(orbit_tests)
```

`tests/core/SanityTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("test harness runs") {
    CHECK(1 + 1 == 2);
}
```

- [ ] **Step 2: Build and run tests**

Run:
```bash
cmake -B build && cmake --build build --target orbit_tests -j8
./build/tests/orbit_tests
```
Expected: `All tests passed (1 assertion in 1 test case)`

- [ ] **Step 3: Commit**

```bash
git add tests/
git commit -m "test: add Catch2 harness with sanity test"
```

---

### Task 3: DelayLine — fractional delay with feedback (core)

**Files:**
- Create: `core/dsp/DelayLine.h`
- Create: `core/dsp/DelayLine.cpp`
- Create: `tests/core/DelayLineTests.cpp`
- Modify: `core/CMakeLists.txt` (add source, drop placeholder)
- Modify: `tests/CMakeLists.txt` (add test file)
- Delete: `core/dsp/Placeholder.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `orbit::dsp::DelayLine` with `void prepare(double sampleRate, float maxDelaySeconds)`, `void reset()`, `void setDelaySeconds(float)`, `void setFeedback(float)` (clamped to `kMaxFeedback = 0.98f`), `float processSample(float input)` (reads delayed sample, writes input + feedback, advances — call once per sample). Used by `OrbitEngine` (Task 6).

- [ ] **Step 1: Write the failing tests**

`tests/core/DelayLineTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/DelayLine.h"

using Catch::Approx;
using orbit::dsp::DelayLine;

static std::vector<float> impulseResponse(DelayLine& line, int numSamples) {
    std::vector<float> out;
    out.reserve(static_cast<size_t>(numSamples));
    for (int n = 0; n < numSamples; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    return out;
}

TEST_CASE("DelayLine delays an impulse by the configured whole-sample time") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);
    const auto out = impulseResponse(line, 150);
    CHECK(out[100] == Approx(1.0f));
    for (int n = 0; n < 100; ++n)
        REQUIRE(out[static_cast<size_t>(n)] == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("DelayLine interpolates fractional delay times") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.5f / 48000.0f);
    const auto out = impulseResponse(line, 20);
    CHECK(out[10] == Approx(0.5f).margin(1e-4f));
    CHECK(out[11] == Approx(0.5f).margin(1e-4f));
}

TEST_CASE("DelayLine feedback produces geometrically decaying repeats") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(0.5f);
    const auto out = impulseResponse(line, 45);
    CHECK(out[10] == Approx(1.0f));
    CHECK(out[20] == Approx(0.5f));
    CHECK(out[30] == Approx(0.25f));
    CHECK(out[40] == Approx(0.125f));
}

TEST_CASE("DelayLine clamps feedback and never blows up") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(2.0f);   // must clamp to 0.98
    float maxAbs = 0.0f;
    for (int n = 0; n < 48000; ++n) {
        const float y = line.processSample(n == 0 ? 1.0f : 0.0f);
        REQUIRE(std::isfinite(y));
        maxAbs = std::max(maxAbs, std::abs(y));
    }
    CHECK(maxAbs <= 1.5f);
}

TEST_CASE("DelayLine flushes denormal-range feedback tails to hard zero") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(0.5f);
    std::vector<float> out;
    for (int n = 0; n < 20000; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    for (int n = 19000; n < 20000; ++n)
        REQUIRE(out[static_cast<size_t>(n)] == 0.0f);   // exactly zero, not denormal dust
}
```

- [ ] **Step 2: Wire the test file in and verify it fails to build**

In `tests/CMakeLists.txt`, change the sources list of `orbit_tests` to:

```cmake
add_executable(orbit_tests
    core/SanityTests.cpp
    core/DelayLineTests.cpp)
```

Run: `cmake -B build && cmake --build build --target orbit_tests -j8`
Expected: FAIL — `fatal error: 'dsp/DelayLine.h' file not found`

- [ ] **Step 3: Implement DelayLine**

`core/dsp/DelayLine.h`:

```cpp
#pragma once
#include <cstddef>
#include <vector>

namespace orbit::dsp {

// Single-channel fractional delay line with built-in feedback.
// Real-time safe after prepare(): processSample never allocates.
class DelayLine {
public:
    static constexpr float kMaxFeedback = 0.98f;

    void prepare(double sampleRate, float maxDelaySeconds);
    void reset();

    void setDelaySeconds(float seconds);   // clamped to [0, maxDelaySeconds]
    void setFeedback(float amount);        // clamped to [0, kMaxFeedback]

    // Reads the delayed sample, writes input + feedback into the line,
    // advances the write head. Call exactly once per sample.
    float processSample(float input);

private:
    float readFractional() const;

    std::vector<float> buffer_;
    std::size_t writePos_ = 0;
    float delaySamples_ = 0.0f;
    float feedback_ = 0.0f;
    double sampleRate_ = 44100.0;
};

} // namespace orbit::dsp
```

`core/dsp/DelayLine.cpp`:

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
    writePos_ = 0;
}

void DelayLine::reset() {
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePos_ = 0;
}

void DelayLine::setDelaySeconds(float seconds) {
    const float maxDelay =
        static_cast<float>(buffer_.size() - 2) / static_cast<float>(sampleRate_);
    seconds = std::clamp(seconds, 0.0f, maxDelay);
    delaySamples_ = seconds * static_cast<float>(sampleRate_);
}

void DelayLine::setFeedback(float amount) {
    feedback_ = std::clamp(amount, 0.0f, kMaxFeedback);
}

float DelayLine::readFractional() const {
    const auto size = static_cast<float>(buffer_.size());
    float readPos = static_cast<float>(writePos_) - delaySamples_;
    while (readPos < 0.0f)
        readPos += size;
    const auto i0 = static_cast<std::size_t>(readPos) % buffer_.size();
    const auto i1 = (i0 + 1) % buffer_.size();
    const float frac = readPos - std::floor(readPos);
    return buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);
}

float DelayLine::processSample(float input) {
    const float out = readFractional();
    float next = input + out * feedback_;
    // Flush denormal-range values to hard zero so feedback tails die cleanly.
    if (std::abs(next) < 1.0e-12f)
        next = 0.0f;
    buffer_[writePos_] = next;
    writePos_ = (writePos_ + 1) % buffer_.size();
    return out;
}

} // namespace orbit::dsp
```

In `core/CMakeLists.txt`, replace the sources:

```cmake
add_library(orbit_core STATIC
    dsp/DelayLine.cpp)
```

Delete `core/dsp/Placeholder.cpp`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests "[!benchmark]"`
Expected: all test cases pass (sanity + 5 DelayLine cases).

- [ ] **Step 5: Commit**

```bash
git add core/ tests/
git commit -m "feat(core): fractional DelayLine with feedback, clamping, denormal flush"
```

---

### Task 4: TempoSync — musical divisions to seconds (core)

**Files:**
- Create: `core/dsp/TempoSync.h`
- Create: `core/dsp/TempoSync.cpp`
- Create: `tests/core/TempoSyncTests.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt` (add the new sources)

**Interfaces:**
- Consumes: nothing.
- Produces: `enum class orbit::dsp::SyncDivision { Free = 0, Whole, Half, Quarter, Eighth, Sixteenth, QuarterDotted, EighthDotted, QuarterTriplet, EighthTriplet, NumDivisions }` and `float orbit::dsp::divisionToSeconds(SyncDivision, double bpm)`. **The enum order is a contract:** Task 7's `AudioParameterChoice` index casts directly to this enum — never reorder.

- [ ] **Step 1: Write the failing tests**

`tests/core/TempoSyncTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/TempoSync.h"

using Catch::Approx;
using orbit::dsp::SyncDivision;
using orbit::dsp::divisionToSeconds;

TEST_CASE("straight divisions at 120 BPM") {
    CHECK(divisionToSeconds(SyncDivision::Whole, 120.0) == Approx(2.0f));
    CHECK(divisionToSeconds(SyncDivision::Half, 120.0) == Approx(1.0f));
    CHECK(divisionToSeconds(SyncDivision::Quarter, 120.0) == Approx(0.5f));
    CHECK(divisionToSeconds(SyncDivision::Eighth, 120.0) == Approx(0.25f));
    CHECK(divisionToSeconds(SyncDivision::Sixteenth, 120.0) == Approx(0.125f));
}

TEST_CASE("dotted divisions are 1.5x straight") {
    CHECK(divisionToSeconds(SyncDivision::QuarterDotted, 120.0) == Approx(0.75f));
    CHECK(divisionToSeconds(SyncDivision::EighthDotted, 120.0) == Approx(0.375f));
}

TEST_CASE("triplet divisions are 2/3 straight") {
    CHECK(divisionToSeconds(SyncDivision::QuarterTriplet, 120.0) == Approx(0.5f * 2.0f / 3.0f));
    CHECK(divisionToSeconds(SyncDivision::EighthTriplet, 120.0) == Approx(0.25f * 2.0f / 3.0f));
}

TEST_CASE("Free and invalid tempi return zero") {
    CHECK(divisionToSeconds(SyncDivision::Free, 120.0) == 0.0f);
    CHECK(divisionToSeconds(SyncDivision::Quarter, 0.0) == 0.0f);
    CHECK(divisionToSeconds(SyncDivision::Quarter, -60.0) == 0.0f);
}
```

Add `core/TempoSyncTests.cpp` to the `orbit_tests` sources in `tests/CMakeLists.txt`.

- [ ] **Step 2: Verify failure**

Run: `cmake --build build --target orbit_tests -j8`
Expected: FAIL — `'dsp/TempoSync.h' file not found`

- [ ] **Step 3: Implement TempoSync**

`core/dsp/TempoSync.h`:

```cpp
#pragma once

namespace orbit::dsp {

// Order is a contract with the tapN_sync AudioParameterChoice (Task 7).
// Append new divisions at the end; NEVER reorder.
enum class SyncDivision {
    Free = 0,
    Whole,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    QuarterDotted,
    EighthDotted,
    QuarterTriplet,
    EighthTriplet,
    NumDivisions
};

// Delay time in seconds for a division at the given tempo.
// Free (or a non-positive bpm) returns 0 — caller falls back to free time.
float divisionToSeconds(SyncDivision division, double bpm);

} // namespace orbit::dsp
```

`core/dsp/TempoSync.cpp`:

```cpp
#include "dsp/TempoSync.h"

namespace orbit::dsp {

float divisionToSeconds(SyncDivision division, double bpm) {
    if (bpm <= 0.0 || division == SyncDivision::Free)
        return 0.0f;

    const float beat = static_cast<float>(60.0 / bpm);   // one quarter note
    switch (division) {
        case SyncDivision::Whole:          return 4.0f * beat;
        case SyncDivision::Half:           return 2.0f * beat;
        case SyncDivision::Quarter:        return beat;
        case SyncDivision::Eighth:         return 0.5f * beat;
        case SyncDivision::Sixteenth:      return 0.25f * beat;
        case SyncDivision::QuarterDotted:  return 1.5f * beat;
        case SyncDivision::EighthDotted:   return 0.75f * beat;
        case SyncDivision::QuarterTriplet: return beat * 2.0f / 3.0f;
        case SyncDivision::EighthTriplet:  return 0.5f * beat * 2.0f / 3.0f;
        default:                           return 0.0f;
    }
}

} // namespace orbit::dsp
```

Add `dsp/TempoSync.cpp` to `orbit_core` sources in `core/CMakeLists.txt`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add core/ tests/
git commit -m "feat(core): tempo sync divisions incl. dotted and triplet"
```

---

### Task 5: Ducker — sidechain gain reduction (core)

**Files:**
- Create: `core/dsp/Ducker.h`
- Create: `core/dsp/Ducker.cpp`
- Create: `tests/core/DuckerTests.cpp`
- Modify: `core/CMakeLists.txt`, `tests/CMakeLists.txt` (add the new sources)

**Interfaces:**
- Consumes: nothing.
- Produces: `orbit::dsp::Ducker` with `void prepare(double sampleRate)`, `void reset()`, `void setAmount(float)` (0..1), `float processGain(float sidechainLevel)` — call once per sample with the dry signal's absolute level; returns the gain (0..1) to multiply the wet signal by. Fixed musical ballistics: 5 ms attack, 250 ms release.

- [ ] **Step 1: Write the failing tests**

`tests/core/DuckerTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/Ducker.h"

using Catch::Approx;
using orbit::dsp::Ducker;

TEST_CASE("Ducker leaves gain at 1.0 when amount is zero") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(0.0f);
    float gain = 1.0f;
    for (int n = 0; n < 4800; ++n)
        gain = d.processGain(1.0f);
    CHECK(gain == Approx(1.0f));
}

TEST_CASE("Ducker strongly reduces gain on sustained loud input at full amount") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(1.0f);
    float gain = 1.0f;
    for (int n = 0; n < 4800; ++n)   // 100 ms of loud dry signal
        gain = d.processGain(1.0f);
    CHECK(gain < 0.3f);
}

TEST_CASE("Ducker recovers toward 1.0 after the input goes silent") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(1.0f);
    for (int n = 0; n < 4800; ++n)
        d.processGain(1.0f);
    float gain = 0.0f;
    for (int n = 0; n < 48000; ++n)  // 1 s of silence
        gain = d.processGain(0.0f);
    CHECK(gain > 0.9f);
}
```

Add `core/DuckerTests.cpp` to `orbit_tests` sources.

- [ ] **Step 2: Verify failure**

Run: `cmake --build build --target orbit_tests -j8`
Expected: FAIL — `'dsp/Ducker.h' file not found`

- [ ] **Step 3: Implement Ducker**

`core/dsp/Ducker.h`:

```cpp
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
```

`core/dsp/Ducker.cpp`:

```cpp
#include "dsp/Ducker.h"
#include <algorithm>
#include <cmath>

namespace orbit::dsp {

void Ducker::prepare(double sampleRate) {
    attackCoef_  = std::exp(-1.0f / (0.005f * static_cast<float>(sampleRate)));   // 5 ms
    releaseCoef_ = std::exp(-1.0f / (0.250f * static_cast<float>(sampleRate)));   // 250 ms
    reset();
}

void Ducker::reset() { envelope_ = 0.0f; }

void Ducker::setAmount(float amount) { amount_ = std::clamp(amount, 0.0f, 1.0f); }

float Ducker::processGain(float sidechainLevel) {
    const float level = std::abs(sidechainLevel);
    const float coef = level > envelope_ ? attackCoef_ : releaseCoef_;
    envelope_ = coef * envelope_ + (1.0f - coef) * level;
    const float duck = std::min(envelope_, 1.0f) * amount_;
    return 1.0f - duck;
}

} // namespace orbit::dsp
```

Add `dsp/Ducker.cpp` to `orbit_core` sources.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add core/ tests/
git commit -m "feat(core): sidechain ducker with musical ballistics"
```

---

### Task 6: OrbitEngine — the 4-tap topology (plugin, JUCE-free)

**Files:**
- Create: `plugin/OrbitEngine.h`
- Create: `plugin/OrbitEngine.cpp`
- Create: `tests/plugin/OrbitEngineTests.cpp`
- Modify: `tests/CMakeLists.txt` (add engine source + test file)
- Modify: `plugin/CMakeLists.txt` (add `OrbitEngine.cpp` to plugin sources)

**Interfaces:**
- Consumes: `orbit::dsp::DelayLine`, `orbit::dsp::Ducker`, `orbit::dsp::SyncDivision`, `orbit::dsp::divisionToSeconds` (Tasks 3–5).
- Produces: `orbit::TapSettings { bool enabled; dsp::SyncDivision sync; float timeSeconds; float feedback; }` and `orbit::OrbitEngine` with `static constexpr int kNumTaps = 4`, `static constexpr float kMaxDelaySeconds = 4.0f`, `void prepare(double sampleRate, int maxBlockSize, int numChannels)`, `void reset()`, `void setTap(int, const TapSettings&)`, `void setDryWet(float)`, `void setDuckAmount(float)`, `void setBpm(double)`, `void process(float* const* channelData, int numChannels, int numSamples)` (in-place). Used by `OrbitAudioProcessor` (Task 7).

- [ ] **Step 1: Write the failing tests**

`tests/plugin/OrbitEngineTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <cmath>
#include <vector>
#include "OrbitEngine.h"

using Catch::Approx;
using orbit::OrbitEngine;
using orbit::TapSettings;
namespace dsp = orbit::dsp;

namespace {
struct StereoBuffer {
    explicit StereoBuffer(int numSamples)
        : left(static_cast<size_t>(numSamples), 0.0f),
          right(static_cast<size_t>(numSamples), 0.0f) {
        channels[0] = left.data();
        channels[1] = right.data();
    }
    std::vector<float> left, right;
    std::array<float*, 2> channels {};
};
} // namespace

TEST_CASE("single enabled tap echoes an impulse at the tap time") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 100.0f / 48000.0f;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);     // wet only
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(256);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 256);

    CHECK(buf.left[100] == Approx(1.0f));
    CHECK(buf.right[100] == Approx(1.0f));
    CHECK(std::abs(buf.left[0]) < 1e-6f);   // dry removed at mix = 1
    CHECK(std::abs(buf.left[50]) < 1e-6f);
}

TEST_CASE("all taps disabled yields silence at full wet") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(256);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 256);

    for (int n = 0; n < 256; ++n) {
        REQUIRE(std::abs(buf.left[static_cast<size_t>(n)]) < 1e-6f);
        REQUIRE(std::abs(buf.right[static_cast<size_t>(n)]) < 1e-6f);
    }
}

TEST_CASE("tempo-synced tap lands on the musical grid") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setBpm(120.0);                    // quarter note = 0.5 s = 24000 samples
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Quarter;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(24100);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 24100);

    CHECK(buf.left[24000] == Approx(1.0f));
}

TEST_CASE("ducking reduces wet energy while the dry signal is loud") {
    auto makeEngine = [](float duckAmount) {
        auto engine = std::make_unique<OrbitEngine>();
        engine->prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 50.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine->setTap(0, tap);
        engine->setDryWet(0.5f);
        engine->setDuckAmount(duckAmount);
        return engine;
    };

    auto energyWithDuck = [&](float duckAmount) {
        auto engine = makeEngine(duckAmount);
        StereoBuffer buf(4800);
        std::fill(buf.left.begin(), buf.left.end(), 1.0f);
        std::fill(buf.right.begin(), buf.right.end(), 1.0f);
        engine->process(buf.channels.data(), 2, 4800);
        double sum = 0.0;
        for (float v : buf.left) sum += std::abs(v);
        return sum;
    };

    CHECK(energyWithDuck(1.0f) < energyWithDuck(0.0f) * 0.9);
}
```

In `tests/CMakeLists.txt`, add both the engine source and the test file to `orbit_tests`:

```cmake
add_executable(orbit_tests
    core/SanityTests.cpp
    core/DelayLineTests.cpp
    core/TempoSyncTests.cpp
    core/DuckerTests.cpp
    plugin/OrbitEngineTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/OrbitEngine.cpp)
```

- [ ] **Step 2: Verify failure**

Run: `cmake -B build && cmake --build build --target orbit_tests -j8`
Expected: FAIL — `'OrbitEngine.h' file not found`

- [ ] **Step 3: Implement OrbitEngine**

`plugin/OrbitEngine.h`:

```cpp
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
```

`plugin/OrbitEngine.cpp`:

```cpp
#include "OrbitEngine.h"
#include <algorithm>
#include <cmath>

namespace orbit {

void OrbitEngine::prepare(double sampleRate, int maxBlockSize, int numChannels) {
    (void) maxBlockSize;
    sampleRate_ = sampleRate;
    numChannels_ = std::clamp(numChannels, 1, kMaxChannels);
    for (auto& tapLines : lines_)
        for (auto& line : tapLines)
            line.prepare(sampleRate, kMaxDelaySeconds);
    ducker_.prepare(sampleRate);
    applyTapTimes();
}

void OrbitEngine::reset() {
    for (auto& tapLines : lines_)
        for (auto& line : tapLines)
            line.reset();
    ducker_.reset();
}

void OrbitEngine::setTap(int index, const TapSettings& settings) {
    if (index < 0 || index >= kNumTaps)
        return;
    taps_[static_cast<size_t>(index)] = settings;
    for (auto& line : lines_[static_cast<size_t>(index)])
        line.setFeedback(settings.feedback);
    applyTapTime(index);
}

void OrbitEngine::setDryWet(float mix01) { mix_ = std::clamp(mix01, 0.0f, 1.0f); }

void OrbitEngine::setDuckAmount(float amount01) { ducker_.setAmount(amount01); }

void OrbitEngine::setBpm(double bpm) {
    if (bpm > 0.0 && bpm != bpm_) {
        bpm_ = bpm;
        applyTapTimes();
    }
}

void OrbitEngine::applyTapTime(int index) {
    const auto& tap = taps_[static_cast<size_t>(index)];
    const float seconds = tap.sync == dsp::SyncDivision::Free
        ? tap.timeSeconds
        : dsp::divisionToSeconds(tap.sync, bpm_);
    for (auto& line : lines_[static_cast<size_t>(index)])
        line.setDelaySeconds(seconds);
}

void OrbitEngine::applyTapTimes() {
    for (int i = 0; i < kNumTaps; ++i)
        applyTapTime(i);
}

void OrbitEngine::process(float* const* channelData, int numChannels, int numSamples) {
    const int channels = std::clamp(numChannels, 1, numChannels_);
    for (int n = 0; n < numSamples; ++n) {
        float dryLevel = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            dryLevel += std::abs(channelData[ch][n]);
        dryLevel /= static_cast<float>(channels);
        const float duckGain = ducker_.processGain(dryLevel);

        for (int ch = 0; ch < channels; ++ch) {
            const float dry = channelData[ch][n];
            float wet = 0.0f;
            for (int t = 0; t < kNumTaps; ++t) {
                // Disabled taps keep processing (buffers stay warm -> no clicks
                // on re-enable) but don't contribute to the mix.
                const float tapOut =
                    lines_[static_cast<size_t>(t)][static_cast<size_t>(ch)].processSample(dry);
                if (taps_[static_cast<size_t>(t)].enabled)
                    wet += tapOut;
            }
            channelData[ch][n] = dry * (1.0f - mix_) + wet * mix_ * duckGain;
        }
    }
}

} // namespace orbit
```

In `plugin/CMakeLists.txt`, extend the plugin sources:

```cmake
target_sources(OrbitDelay PRIVATE
    PluginProcessor.cpp
    OrbitEngine.cpp)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add plugin/ tests/
git commit -m "feat(plugin): 4-tap OrbitEngine with tempo sync and ducking"
```

---

### Task 7: Parameter layer + processor wiring (APVTS, versioned state)

**Files:**
- Create: `plugin/Parameters.h`
- Create: `plugin/Parameters.cpp`
- Modify: `plugin/PluginProcessor.h` (replace skeleton entirely)
- Modify: `plugin/PluginProcessor.cpp` (replace skeleton entirely)
- Modify: `plugin/CMakeLists.txt` (add `Parameters.cpp`)

**Interfaces:**
- Consumes: `orbit::OrbitEngine`, `orbit::TapSettings` (Task 6); `orbit::dsp::SyncDivision` enum order (Task 4).
- Produces: `orbit::params::createLayout()` returning `juce::AudioProcessorValueTreeState::ParameterLayout`; ID helpers `tapEnabledId(int)`, `tapTimeId(int)`, `tapSyncId(int)`, `tapFeedbackId(int)` (0-based tap index → `"tap1_…"`), constants `kDryWetId = "mix_drywet"`, `kDuckId = "mix_duck"`, `kSyncChoices`; final `OrbitAudioProcessor` with public `juce::AudioProcessorValueTreeState apvts`. State blobs carry `product="orbit"`, `stateVersion=1` (spec §4 family rules). Phase 3's preset system and the future UI bind to these exact IDs.

- [ ] **Step 1: Write `plugin/Parameters.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace orbit::params {

inline juce::String tapEnabledId(int tapIndex)  { return "tap" + juce::String(tapIndex + 1) + "_enabled"; }
inline juce::String tapTimeId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_time"; }
inline juce::String tapSyncId(int tapIndex)     { return "tap" + juce::String(tapIndex + 1) + "_sync"; }
inline juce::String tapFeedbackId(int tapIndex) { return "tap" + juce::String(tapIndex + 1) + "_feedback"; }

inline constexpr auto kDryWetId = "mix_drywet";
inline constexpr auto kDuckId   = "mix_duck";

// Index order MUST match orbit::dsp::SyncDivision (core/dsp/TempoSync.h).
inline const juce::StringArray kSyncChoices {
    "Free", "1/1", "1/2", "1/4", "1/8", "1/16",
    "1/4 D", "1/8 D", "1/4 T", "1/8 T"
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace orbit::params
```

- [ ] **Step 2: Write `plugin/Parameters.cpp`**

```cpp
#include "Parameters.h"

namespace orbit::params {

juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    for (int i = 0; i < 4; ++i) {
        const auto n = juce::String(i + 1);
        parameters.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(tapEnabledId(i), 1), "Tap " + n + " On", i == 0));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(tapTimeId(i), 1), "Tap " + n + " Time",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 0.0f, 0.35f), 350.0f));
        parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(tapSyncId(i), 1), "Tap " + n + " Sync", kSyncChoices, 0));
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(tapFeedbackId(i), 1), "Tap " + n + " Feedback",
            juce::NormalisableRange<float>(0.0f, 0.98f), 0.35f));
    }

    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDryWetId, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kDuckId, 1), "Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    return { parameters.begin(), parameters.end() };
}

} // namespace orbit::params
```

- [ ] **Step 3: Replace `plugin/PluginProcessor.h` with the real processor**

```cpp
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "OrbitEngine.h"
#include "Parameters.h"

class OrbitAudioProcessor : public juce::AudioProcessor {
public:
    OrbitAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return orbit::OrbitEngine::kMaxDelaySeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateEngineFromParameters();

    orbit::OrbitEngine engine_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitAudioProcessor)
};
```

- [ ] **Step 4: Replace `plugin/PluginProcessor.cpp`**

```cpp
#include "PluginProcessor.h"

namespace params = orbit::params;

OrbitAudioProcessor::OrbitAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", params::createLayout()) {}

void OrbitAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine_.reset();
}

bool OrbitAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void OrbitAudioProcessor::updateEngineFromParameters() {
    for (int i = 0; i < orbit::OrbitEngine::kNumTaps; ++i) {
        orbit::TapSettings tap;
        tap.enabled = apvts.getRawParameterValue(params::tapEnabledId(i))->load() > 0.5f;
        tap.sync = static_cast<orbit::dsp::SyncDivision>(
            static_cast<int>(apvts.getRawParameterValue(params::tapSyncId(i))->load()));
        tap.timeSeconds = apvts.getRawParameterValue(params::tapTimeId(i))->load() / 1000.0f;
        tap.feedback = apvts.getRawParameterValue(params::tapFeedbackId(i))->load();
        engine_.setTap(i, tap);
    }
    engine_.setDryWet(apvts.getRawParameterValue(params::kDryWetId)->load());
    engine_.setDuckAmount(apvts.getRawParameterValue(params::kDuckId)->load());
}

void OrbitAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    if (auto* playhead = getPlayHead())
        if (const auto position = playhead->getPosition())
            if (const auto bpm = position->getBpm())
                engine_.setBpm(*bpm);

    updateEngineFromParameters();
    engine_.process(buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(), buffer.getNumSamples());
}

void OrbitAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Family-wide versioned state envelope (spec §4 product-family rules).
    auto state = apvts.copyState();
    state.setProperty("product", "orbit", nullptr);
    state.setProperty("stateVersion", 1, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void OrbitAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!tree.isValid())
        return;
    if (tree.hasProperty("product") && tree.getProperty("product").toString() != "orbit")
        return;   // refuse state from a different family product
    apvts.replaceState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OrbitAudioProcessor();
}
```

- [ ] **Step 5: Add `Parameters.cpp` to the plugin target**

In `plugin/CMakeLists.txt`:

```cmake
target_sources(OrbitDelay PRIVATE
    PluginProcessor.cpp
    OrbitEngine.cpp
    Parameters.cpp)
```

- [ ] **Step 6: Build all formats and re-run the unit tests**

Run:
```bash
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone orbit_tests -j8
./build/tests/orbit_tests
```
Expected: clean build, all unit tests still pass.

- [ ] **Step 7: Validate the AU with auval**

Run: `auval -v aufx Orb1 Orba`
Expected: ends with `AU VALIDATION SUCCEEDED`.

- [ ] **Step 8: Smoke-test the Standalone app**

Run: `open "build/plugin/OrbitDelay_artefacts/Debug/Standalone/Orbit.app"`
Expected: app launches, generic editor shows all 18 parameters (4 taps × 4 + Mix + Ducking), no crash. Quit it afterward.

- [ ] **Step 9: Commit**

```bash
git add plugin/
git commit -m "feat(plugin): APVTS parameter layer, engine wiring, versioned state"
```

---

### Task 8: pluginval gate + developer docs

**Files:**
- Create: `scripts/run_pluginval.sh`
- Modify: `README.md` (add Development section)

**Interfaces:**
- Consumes: built VST3 from Task 7.
- Produces: the validation gate every later phase re-runs before merging.

- [ ] **Step 1: Write `scripts/run_pluginval.sh`**

```bash
#!/usr/bin/env bash
# Validates the built plugin at max strictness. Install pluginval first:
#   brew install --cask pluginval
set -euo pipefail
cd "$(dirname "$0")/.."

PLUGIN="${1:-build/plugin/OrbitDelay_artefacts/Debug/VST3/Orbit.vst3}"

if ! command -v pluginval >/dev/null 2>&1; then
    echo "pluginval not found — install with: brew install --cask pluginval" >&2
    exit 1
fi

pluginval --strictness-level 10 --validate "$PLUGIN"
```

Run: `chmod +x scripts/run_pluginval.sh`

- [ ] **Step 2: Run it**

Run: `./scripts/run_pluginval.sh`
Expected: output ends with `ALL TESTS PASSED`. If pluginval isn't installed, install it first (`brew install --cask pluginval`), then re-run.

- [ ] **Step 3: Add a Development section to README.md**

Append to `README.md`:

```markdown
## Development

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # first configure downloads JUCE (~minutes)
cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests   # unit tests
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone -j8
./scripts/run_pluginval.sh                # plugin validation (max strictness)
auval -v aufx Orb1 Orba                   # Apple AU validation
```

Built plugins are auto-copied to `~/Library/Audio/Plug-Ins/` — rescan in your DAW to test.
```

- [ ] **Step 4: Commit**

```bash
git add scripts/ README.md
git commit -m "chore: pluginval gate script and dev docs"
```

---

## Self-Review Notes

- **Spec coverage (this phase):** build-order stages 1–3 fully covered (scaffolding + branding ✓ Task 1, multi-tap delay + sync + ducking ✓ Tasks 3–6, parameter layer + versioned state ✓ Task 7, validation ✓ Task 8). Stages 4–10 are explicitly Phase 2–4 plans.
- **Type consistency verified:** `DelayLine::processSample(float)` (T3) ← `OrbitEngine` (T6); `divisionToSeconds(SyncDivision, double)` (T4) ← T6; `Ducker::processGain(float)` (T5) ← T6; `SyncDivision` enum order (T4) ↔ `kSyncChoices` (T7); parameter ID helpers (T7) match Global Constraints IDs.
- **UI contract:** parameters listed in spec §6 that don't exist yet (pitch, reverse, character, mod, width, ping-pong, filters, freeze) are Phase 2 — the IDs defined here are final and won't change.
