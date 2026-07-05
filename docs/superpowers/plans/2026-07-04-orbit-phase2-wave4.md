# Orbit Phase 2 Wave 4 — Preset System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the preset data layer — `.orbitpreset` format, PresetManager (load/save/tags/dirty/A-B), and 40 embedded factory presets — per spec `docs/superpowers/specs/2026-07-04-orbit-preset-system-design.md` (binding).

**Architecture:** `.orbitpreset` = the processor's existing versioned state XML + a `PresetMeta` sibling node. `plugin/PresetManager.{h,cpp}` (JUCE allowed) owns enumeration/application/A-B; the processor exposes one instance. Factory presets are repo XML compiled in via `juce_add_binary_data`. Tests run headlessly in a NEW test target (`orbit_preset_tests`) that links JUCE + the real parameter layout via a minimal test processor — the existing JUCE-free `orbit_tests` target is untouched.

**Tech Stack:** unchanged (C++17, JUCE 8.0.4, Catch2, CMake).

## Global Constraints

- Zero changes to `core/`, `plugin/OrbitEngine.*`, `plugin/Parameters.*` (read-only consumer), or DSP behavior. `stateVersion` stays 1. No new plugin parameters. auval must still report `34 Global Scope Parameters` + SUCCEEDED.
- PresetManager is message-thread-only; state application goes through `apvts.replaceState` only.
- Tag vocabulary (exact): `Vocals, Drums, Ambient, Dub, Lo-fi, Utility`.
- User preset dir: `userApplicationDataDirectory`/`Synthios/Orbit/Presets` (overridable in the constructor for tests).
- All 61 existing `orbit_tests` cases pass UNCHANGED. Existing target links no JUCE — keep it that way; preset tests live in the new `orbit_preset_tests` target.
- Conventional commits. Branch `dev/phase-2-wave-4` from main (created in Task 1). Do not push until delivery.

---

### Task 1: PresetManager core + headless JUCE test rig

**Files:**
- Create: `plugin/PresetManager.h`, `plugin/PresetManager.cpp`, `tests/preset/PresetManagerTests.cpp`, `tests/preset/TestProcessor.h`, `tests/preset/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt` (or root `CMakeLists.txt`) to `add_subdirectory` the new test dir — whichever mirrors existing structure most cleanly.

**Interfaces produced (binding for Tasks 2–3):** the spec §2 API verbatim, with two plan-level refinements:
1. Constructor gains a test seam: `PresetManager(juce::AudioProcessorValueTreeState& apvts, const juce::File& userDirOverride = {});` — invalid `File` ⇒ production default directory.
2. A/B methods (`toggleAB/copyAB/isSlotB`) are declared in Task 1's header but implemented in Task 2 (declare + `// Task 2` stub returning defaults so Task 1 compiles).

**Implementation notes (latitude on internals; behavior binding = spec §§1–2):**
- Serialization helpers (private): `juce::ValueTree stateWithMeta(name, tags, description)` — copies `apvts.copyState()`, sets `product`/`stateVersion` exactly like `getStateInformation`, appends a `PresetMeta` child; `bool applyPresetXml(const juce::String& xml)` — parse, validate product, strip `PresetMeta`, `replaceState`, clear dirty, set current name. Factory and user loading share `applyPresetXml`.
- Dirty flag: in the constructor, walk `apvts.processor.getParameters()`, `dynamic_cast` to `juce::AudioProcessorParameterWithID`, `apvts.addParameterListener(paramID, this)` for each; remove all in the destructor. `parameterChanged()` sets `modified_ = true` unless a load/save is in progress (guard flag). PresetManager implements `juce::AudioProcessorValueTreeState::Listener`.
- Filename sanitation: keep alphanumerics, space, `-`, `_`; replace everything else with `_`; trim; empty → `"Preset"`. File = sanitized name + `.orbitpreset`.
- `userPresets()` rescans the directory each call (cheap at UI rate, always fresh). `filterByTag` = factory + user where the comma-split tag list contains the tag (trimmed, case-sensitive).
- `renameUserPreset`: write the file under the new sanitized name with `PresetMeta name` updated, delete the old file; fail (return false) if target exists.
- Factory enumeration returns an empty array until Task 3 wires BinaryData (keep the member + getter now).

**Test rig (`tests/preset/TestProcessor.h`, complete):**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

// Minimal host for the REAL Orbit parameter layout — no engine, no editor.
struct TestProcessor : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState apvts;
    TestProcessor()
        : apvts(*this, nullptr, "PARAMS", orbit::createParameterLayout()) {}
    const juce::String getName() const override { return "OrbitPresetTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
```

(If `orbit::createParameterLayout` has a different name/signature in `plugin/Parameters.h`, use the real one — check the header; do not modify it.)

**CMake (`tests/preset/CMakeLists.txt`) — model:** a plain `add_executable(orbit_preset_tests PresetManagerTests.cpp ../../plugin/PresetManager.cpp ../../plugin/Parameters.cpp)` linked against `Catch2::Catch2WithMain`, `orbit_core` (Parameters.h includes TempoSync via core), and JUCE: link the same JUCE module targets the plugin uses (`juce::juce_audio_processors`, `juce::juce_audio_utils` if needed) plus `juce::juce_recommended_config_flags`. Add compile definitions `JUCE_STANDALONE_APPLICATION=1 JUCE_MODULE_AVAILABLE_juce_audio_processors=1` as needed to compile without the plugin client. Include dirs: `plugin/`, `core/`. Each test provides JUCE runtime via a fixture member `juce::ScopedJuceInitialiser_GUI juceInit;`. **This is the riskiest plumbing in the wave — if JUCE refuses to build outside `juce_add_plugin` after a genuine attempt (e.g. missing plugin-client macros you cannot satisfy with compile definitions), STOP and report BLOCKED with the exact errors; do not hack plugin sources.**

- [ ] **Step 1: create branch** — `git checkout main && git checkout -b dev/phase-2-wave-4`
- [ ] **Step 2 (TDD): failing tests** — `tests/preset/PresetManagerTests.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestProcessor.h"
#include "PresetManager.h"

using Catch::Approx;

struct Fixture {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    juce::TemporaryFile tmpDirToken;       // unique path base
    juce::File dir;
    std::unique_ptr<orbit::PresetManager> pm;
    Fixture() {
        dir = tmpDirToken.getFile().getSiblingFile("orbit_preset_test_dir");
        dir.createDirectory();
        pm = std::make_unique<orbit::PresetManager>(proc.apvts, dir);
    }
    ~Fixture() { dir.deleteRecursively(); }
    float get(const char* id) { return proc.apvts.getRawParameterValue(id)->load(); }
    void set(const char* id, float v) {
        auto* p = proc.apvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(v));
    }
};

TEST_CASE_METHOD(Fixture, "save then load round-trips every parameter and metadata") {
    set("mix_width", 1.7f);
    set("tap1_feedback", 0.62f);
    set("tap3_pitch", 7.0f);
    REQUIRE(pm->saveUserPreset("My Wide Slap", "Vocals,Utility", "test preset", false));
    // scramble
    set("mix_width", 0.3f);
    set("tap1_feedback", 0.1f);
    set("tap3_pitch", -3.0f);
    const auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    CHECK(users[0].name == "My Wide Slap");
    CHECK(users[0].tags == "Vocals,Utility");
    CHECK(users[0].description == "test preset");
    CHECK_FALSE(users[0].isFactory);
    REQUIRE(pm->loadPreset(users[0]));
    CHECK(get("mix_width") == Approx(1.7f));
    CHECK(get("tap1_feedback") == Approx(0.62f));
    CHECK(get("tap3_pitch") == Approx(7.0f));
    CHECK(pm->currentPresetName() == "My Wide Slap");
}

TEST_CASE_METHOD(Fixture, "corrupt and foreign-product files are rejected without touching state") {
    set("mix_width", 1.5f);
    dir.getChildFile("bad.orbitpreset").replaceWithText("this is not xml <");
    dir.getChildFile("foreign.orbitpreset")
       .replaceWithText("<PARAMS product=\"nebula\" stateVersion=\"1\"/>");
    auto users = pm->userPresets();
    REQUIRE(users.size() == 2);
    for (auto& u : users)
        CHECK_FALSE(pm->loadPreset(u));
    CHECK(get("mix_width") == Approx(1.5f));       // untouched
    CHECK(pm->currentPresetName() == juce::String());
}

TEST_CASE_METHOD(Fixture, "dirty flag lifecycle") {
    REQUIRE(pm->saveUserPreset("Base", "Utility", "", false));
    CHECK_FALSE(pm->isModified());                 // save clears
    REQUIRE(pm->loadPreset(pm->userPresets()[0]));
    CHECK_FALSE(pm->isModified());                 // load clears
    set("tap2_feedback", 0.4f);
    CHECK(pm->isModified());                       // edit sets
}

TEST_CASE_METHOD(Fixture, "tag filtering, overwrite protection, delete and rename") {
    REQUIRE(pm->saveUserPreset("VoxOne", "Vocals", "", false));
    REQUIRE(pm->saveUserPreset("DrumOne", "Drums", "", false));
    CHECK_FALSE(pm->saveUserPreset("VoxOne", "Vocals", "", false));   // exists
    REQUIRE(pm->saveUserPreset("VoxOne", "Vocals", "v2", true));      // overwrite ok
    CHECK(pm->filterByTag("Vocals").size() == 1);
    CHECK(pm->filterByTag("Drums").size() == 1);
    CHECK(pm->filterByTag("Ambient").size() == 0);
    auto vox = pm->filterByTag("Vocals")[0];
    REQUIRE(pm->renameUserPreset(vox, "VoxTwo"));
    CHECK(pm->filterByTag("Vocals")[0].name == "VoxTwo");
    REQUIRE(pm->deleteUserPreset(pm->filterByTag("Vocals")[0]));
    CHECK(pm->userPresets().size() == 1);
}

TEST_CASE_METHOD(Fixture, "filenames are sanitized") {
    REQUIRE(pm->saveUserPreset("A/B: <Test>?", "Utility", "", false));
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.orbitpreset");
    REQUIRE(files.size() == 1);
    CHECK_FALSE(files[0].getFileName().containsAnyOf("/\\:<>?*|\""));
}
```

- [ ] **Step 3: verify RED** (PresetManager.h not found / target absent), **Step 4: implement + CMake**, **Step 5: GREEN** — `cmake --build build --target orbit_preset_tests -j8 && ./build/tests/preset/orbit_preset_tests` all pass AND `./build/tests/orbit_tests` still 61/61.
- [ ] **Step 6: commit** — `feat(plugin): PresetManager — orbitpreset format, user presets, tags, dirty flag`

---

### Task 2: A/B compare + processor integration

**Files:**
- Modify: `plugin/PresetManager.h/.cpp` (implement the A/B stubs), `plugin/PluginProcessor.h/.cpp` (member + accessor), `tests/preset/PresetManagerTests.cpp` (add cases)

**Interfaces:**
- Consumes: Task 1 manager.
- Produces: working `toggleAB()/copyAB()/isSlotB()` per spec §3; `orbit::PresetManager& OrbitAudioProcessor::presetManager()`.

**Behavior (binding, spec §3):** two full-state `juce::ValueTree` slots; fresh instance = both hold the state captured at construction, A live. `toggleAB()`: capture live state into the current slot, swap the live-slot marker, apply the other slot via the same strip-and-replace path (`PresetMeta` never present in slots), set `modified_`, keep `currentPresetName`. `copyAB()`: overwrite the INACTIVE slot with the live state; live state untouched; flag untouched.

**Processor wiring:** `OrbitAudioProcessor` gains `std::unique_ptr<orbit::PresetManager> presetManager_;` constructed AFTER `apvts` in the constructor init order (member declaration order in the header controls this — declare after apvts) and `orbit::PresetManager& presetManager() { return *presetManager_; }`. Host program API stays exactly as is (1 program) — add a one-line comment referencing the spec decision.

- [ ] **Step 1 (TDD): add failing tests**

```cpp
TEST_CASE_METHOD(Fixture, "A/B: edits survive a round trip") {
    set("mix_width", 1.9f);                        // edit on A
    pm->toggleAB();                                // -> B (default state)
    CHECK(pm->isSlotB());
    CHECK(get("mix_width") != Approx(1.9f).epsilon(0.001));
    set("tap1_feedback", 0.33f);                   // edit on B
    pm->toggleAB();                                // -> back to A
    CHECK_FALSE(pm->isSlotB());
    CHECK(get("mix_width") == Approx(1.9f));       // A's edit preserved
    pm->toggleAB();                                // -> B again
    CHECK(get("tap1_feedback") == Approx(0.33f));  // B's edit preserved
}

TEST_CASE_METHOD(Fixture, "copyAB clones live over inactive and keeps live untouched") {
    set("mix_width", 1.8f);
    pm->copyAB();                                  // B := A
    CHECK(get("mix_width") == Approx(1.8f));       // live unchanged
    pm->toggleAB();                                // -> B
    CHECK(get("mix_width") == Approx(1.8f));       // clone applied
}

TEST_CASE_METHOD(Fixture, "toggleAB sets modified and keeps preset name") {
    REQUIRE(pm->saveUserPreset("Named", "Utility", "", false));
    REQUIRE(pm->loadPreset(pm->userPresets()[0]));
    pm->toggleAB();
    CHECK(pm->isModified());
    CHECK(pm->currentPresetName() == "Named");
}
```

- [ ] **Step 2: RED**, **Step 3: implement (manager + processor member/accessor)**, **Step 4: GREEN — both test targets** (`orbit_preset_tests` all; `orbit_tests` 61/61) **and the plugin still builds** (`cmake --build build --target OrbitDelay_VST3 -j8`), **Step 5: commit** — `feat(plugin): A/B compare + processor preset-manager integration`

---

### Task 3: 40 factory presets + BinaryData + validation + gates

**Files:**
- Create: `assets/presets/01_Utility_Init_Quarter.orbitpreset` … `40_...orbitpreset` (40 files), and a generator is permitted but the committed artifacts are the 40 XML files.
- Modify: root `CMakeLists.txt` or `plugin/CMakeLists.txt` (add `juce_add_binary_data(OrbitPresetData SOURCES ...)`, link into the plugin target AND `orbit_preset_tests`), `plugin/PresetManager.cpp` (factory enumeration from BinaryData), `tests/preset/PresetManagerTests.cpp` (validation suite).

**Factory enumeration:** iterate `BinaryData::namedResourceList[0..numResources)`, take resources whose `BinaryData::getNamedResourceOriginalFilename` ends in `.orbitpreset`, parse name/tags/author/description from `PresetMeta`, sort by original filename (the `NN_` prefix gives stable order). `loadPreset` for factory entries parses from the embedded char array.

**Preset authoring (binding recipe table).** Derive exact parameter IDs from `plugin/Parameters.h` (do not guess; the validation test enforces existence). All presets: `product="orbit"`, `stateVersion="1"`, `author="Synthios"`, values inside ranges, unspecified params at default, `freeze_active` false everywhere. Sync divisions by name from `kSyncDivisionNames`. The table binds musical intent + the listed values; translate to the real ID spellings:

| # | Name | Tags | Key settings (unlisted = default) |
|---|------|------|-----------------------------------|
| 01 | Init Quarter | Utility | tap1 on 1/4 fb 0.35, mix 0.25 |
| 02 | Init Eighth | Utility | tap1 on 1/8 fb 0.30, mix 0.22 |
| 03 | Mono Slap | Utility | tap1 on Free 90ms fb 0.05, width 0, mix 0.28 |
| 04 | Stereo Quarter Wide | Utility | tap1 on 1/4 fb 0.4, width 1.6, mix 0.3 |
| 05 | Dotted 8th Vocal | Vocals | tap1 on 1/8 D fb 0.42, duck 0.55, highcut 9k, mix 0.32 |
| 06 | Vocal Slap 120 | Vocals | tap1 Free 120ms fb 0.08, duck 0.3, mix 0.3 |
| 07 | Whisper Trail | Vocals | tap1 1/4 fb 0.55, character Tape, highcut 6.5k, duck 0.7, mix 0.35 |
| 08 | Call and Answer | Vocals | tap1 1/4 fb 0.3, tap2 1/2 fb 0.25, pingpong on, duck 0.5, mix 0.3 |
| 09 | Thickener | Vocals | tap1 Free 28ms fb 0.0, width 1.8, mix 0.45 |
| 10 | Vocal Shimmer Lift | Vocals | tap1 1/2 fb 0.45 pitch +12, character Clean, duck 0.6, mix 0.28 |
| 11 | Ad-Lib Ping | Vocals | tap1 1/8 fb 0.5, pingpong on, highcut 8k, duck 0.65, mix 0.35 |
| 12 | Radio Vox Echo | Vocals | tap1 1/4 fb 0.5, character Grit, lowcut 400, highcut 4.5k, mix 0.3 |
| 13 | Ping-Pong 16ths | Drums | tap1 1/16 fb 0.45, pingpong on, width 1.4, mix 0.35 |
| 14 | Snare Splash | Drums | tap1 1/8 fb 0.25, tap2 1/4 fb 0.15, highcut 10k, mix 0.3 |
| 15 | Perc Scatter | Drums | taps1-4 on 1/16,1/8,1/8 D,1/4 fb .2/.25/.3/.2, width 1.5, mix 0.32 |
| 16 | Room Builder | Drums | tap1 Free 60ms, tap2 Free 95ms, tap3 Free 130ms fb .15 each, width 1.7, mix 0.4 |
| 17 | Half-Time Throw | Drums | tap1 1/2 fb 0.5, character Tape, duck 0.4, mix 0.3 |
| 18 | Trap Hat Bounce | Drums | tap1 1/8 T fb 0.4, pingpong on, mix 0.3 |
| 19 | Kick Safe Echo | Drums | tap1 1/4 fb 0.35, lowcut 250, duck 0.5, mix 0.3 |
| 20 | Reverse Snare Tail | Drums | tap1 1/2 fb 0.3 reverse on, mix 0.35 |
| 21 | Big Sky | Ambient | tap1 1/2 fb 0.6, tap2 1/1 fb 0.5, width 2.0, highcut 12k, mix 0.5, mod 0.3/0.4Hz |
| 22 | Shimmer Up | Ambient | tap1 1/2 fb 0.55 pitch +12, width 1.8, mix 0.45 |
| 23 | Shimmer Down | Ambient | tap1 1/2 fb 0.5 pitch -12, character Tape, mix 0.4 |
| 24 | Slow Tide | Ambient | tap1 1/1 fb 0.65, mod 0.5/0.25Hz, highcut 8k, mix 0.5 |
| 25 | Glass Orbit | Ambient | tap1 1/4 fb 0.5, tap2 1/8 D fb 0.4 pitch +7, pingpong on, width 1.9, mix 0.45 |
| 26 | Frozen Bloom Bed | Ambient | tap1 1/2 fb 0.68, tap2 1/1 fb 0.55, character Tape, width 2.0, mix 0.55 |
| 27 | Fifth Cascade | Ambient | tap1 1/4 fb 0.55 pitch +7, tap2 1/2 fb 0.45 pitch +7, mix 0.42 |
| 28 | Night Swells | Ambient | tap1 1/1 fb 0.6, mod 0.4/0.3Hz, duck 0.35, highcut 7k, mix 0.48 |
| 29 | King Tubby Throw | Dub | tap1 1/4 fb 0.62, character Tape, highcut 5.5k, lowcut 300, mix 0.4 |
| 30 | Steppers Bounce | Dub | tap1 1/8 D fb 0.55, pingpong on, character Tape, mix 0.38 |
| 31 | Siren Regen | Dub | tap1 1/4 fb 0.72, character Grit, highcut 4k, mod 0.25/0.6Hz, mix 0.4 |
| 32 | Melodica Space | Dub | tap1 1/4 fb 0.5, tap2 1/2 fb 0.4, width 1.6, highcut 6k, mix 0.42 |
| 33 | Rewind Feel | Dub | tap1 1/2 fb 0.55 reverse on, character Tape, mix 0.38 |
| 34 | Deep Echo Chamber | Dub | tap1 1/4 fb 0.6, tap2 1/1 fb 0.45, lowcut 200, highcut 5k, duck 0.3, mix 0.45 |
| 35 | Cassette Slap | Lo-fi | tap1 Free 110ms fb 0.2, character Tape, highcut 5k, mod 0.35/0.5Hz, mix 0.32 |
| 36 | Warped Memory | Lo-fi | tap1 1/4 fb 0.5, character Grit, mod 0.6/0.4Hz, highcut 4.5k, mix 0.4 |
| 37 | Dusty Ping | Lo-fi | tap1 1/8 fb 0.45, pingpong on, character Grit, highcut 3.8k, mix 0.35 |
| 38 | Broken Speaker Echo | Lo-fi | tap1 1/4 fb 0.55, character Grit, lowcut 500, highcut 3k, mix 0.38 |
| 39 | Tape Stop Dream | Lo-fi | tap1 1/2 fb 0.5 pitch -5, character Tape, mod 0.5/0.3Hz, mix 0.4 |
| 40 | VHS Choir | Lo-fi | tap1 1/2 fb 0.55 pitch +12, character Tape, highcut 6k, width 1.7, mix 0.42 |

(mix = dry/wet; mod a/b = depth/rateHz; cuts in Hz. Tap times: sync name or Free+seconds.)

- [ ] **Step 1 (TDD): validation tests first**

```cpp
TEST_CASE_METHOD(Fixture, "factory set: 40 presets, unique names, valid tags, ordered") {
    const auto& f = pm->factoryPresets();
    REQUIRE(f.size() == 40);
    juce::StringArray names;
    const juce::StringArray vocab { "Vocals","Drums","Ambient","Dub","Lo-fi","Utility" };
    for (auto& p : f) {
        CHECK(p.isFactory);
        CHECK(p.name.isNotEmpty());
        names.addIfNotAlreadyThere(p.name);
        for (auto& t : juce::StringArray::fromTokens(p.tags, ",", ""))
            CHECK(vocab.contains(t.trim()));
        CHECK(p.description.isNotEmpty());
    }
    CHECK(names.size() == 40);                     // unique
}

TEST_CASE_METHOD(Fixture, "every factory preset loads and every value is in range") {
    for (auto& p : pm->factoryPresets()) {
        REQUIRE(pm->loadPreset(p));
        for (auto* param : proc.getParameters()) {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param);
            REQUIRE(rp != nullptr);
            const float norm = rp->getValue();
            CHECK(norm >= 0.0f);
            CHECK(norm <= 1.0f);
        }
        CHECK(pm->currentPresetName() == p.name);
        CHECK_FALSE(pm->isModified());
    }
}

TEST_CASE_METHOD(Fixture, "factory presets reference only real parameter IDs") {
    // Loading via replaceState silently drops unknown children; guard by
    // checking each preset XML's parameter ids against the layout.
    juce::StringArray known;
    for (auto* param : proc.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            known.add(wid->paramID);
    for (auto& p : pm->factoryPresets()) {
        const auto xml = pm->presetXmlFor(p);      // add this accessor (returns raw XML text)
        auto tree = juce::ValueTree::fromXml(xml);
        REQUIRE(tree.isValid());
        for (int i = 0; i < tree.getNumChildren(); ++i) {
            auto child = tree.getChild(i);
            if (child.hasType("PARAM"))
                CHECK(known.contains(child.getProperty("id").toString()));
        }
    }
}
```

(APVTS serializes parameters as `PARAM` children with `id` properties — verify the actual tree shape from a round-trip in Task 1 and adjust the type/property names in this test to the real ones before locking RED. `presetXmlFor(const PresetInfo&)` is a small public accessor added this task.)

- [ ] **Step 2: RED** (factoryPresets() empty / accessor missing), **Step 3: author the 40 XML files + BinaryData target + enumeration + accessor**, **Step 4: GREEN both test targets + orbit_tests 61/61**
- [ ] **Step 5: full gates**

```bash
cmake --build build --target orbit_tests orbit_preset_tests OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone -j8
./build/tests/orbit_tests                      # 61/61
./build/tests/preset/orbit_preset_tests       # all preset cases
auval -v aufx Orb1 Orba                        # 34 Global Scope Parameters, SUCCEEDED
./scripts/run_pluginval.sh                     # SUCCESS
```

- [ ] **Step 6: commit** — `feat(presets): 40 factory presets embedded via BinaryData + validation`

---

## Self-Review Notes

- Spec coverage: §1 format (T1 helpers + T3 validation), §2 manager API incl. dir override + accessor (T1/T2), §3 A/B incl. name/flag semantics (T2), §4 forty presets + naming + BinaryData (T3), §5 tests (T1–T3), §6 constraints (global block). Program-API decision documented in T2.
- Type consistency: `PresetInfo` fields used identically in T1 tests and T3 validation; `presetXmlFor` introduced in T3 and used only there; A/B names match spec (`toggleAB/copyAB/isSlotB`).
- Known risk, called out in T1: JUCE headless test-target plumbing. Escalation path defined (BLOCKED with errors, no hacking plugin sources).
- Task 3's tree-shape caveat is deliberate: the test author must confirm `PARAM`/`id` naming against a real APVTS dump before trusting RED.
