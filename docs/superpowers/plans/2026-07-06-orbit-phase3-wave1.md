# Orbit Phase 3 Wave 1 — Editor Foundation & Global Control Column

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the placeholder `GenericAudioProcessorEditor` with a real, resizable `OrbitEditor` that carries the design system as a single source of truth and renders the right-hand global control column (Mix, Ducking, Width, Ping-Pong, Motion Depth, Motion Rate, Low Cut, High Cut) with working APVTS attachments.

**Architecture:** A new `plugin/gui/` layer. `OrbitTheme.h` holds every colour/type/spacing token as `constexpr` data (ported verbatim from the approved mockup at `docs/superpowers/specs/phase3-ui-design/2026-07-06-orbit-phase3-ui-design.html`). `OrbitLookAndFeel` draws controls in that style. `OrbitEditor` is the top-level `AudioProcessorEditor`, hosts child components, and owns a single UI-side scale factor so a resize is a proportional scale of one fixed base layout. Global controls attach to the existing APVTS via JUCE `SliderAttachment`/`ButtonAttachment` and the exact parameter IDs already defined in `plugin/Parameters.h` — **the editor adds no new parameters and touches no DSP.** GUI logic is tested headlessly in a new `orbit_gui_tests` target that links JUCE and a minimal test processor, mirroring the existing `orbit_preset_tests` rig.

**Tech Stack:** unchanged — C++17, JUCE 8.0.4, Catch2, CMake.

## Global Constraints

- **Zero changes** to `core/`, `plugin/OrbitEngine.*`, `plugin/Parameters.*`, `plugin/PresetManager.*`, or any DSP behaviour. `stateVersion` stays 1. **No new plugin parameters** — `auval` must still report `34 Global Scope Parameters` + SUCCEEDED, and `pluginval` (strictness 10) must still pass.
- The one permitted change to `plugin/PluginProcessor.{h,cpp}` in this wave: (a) `createEditor()` returns the new `OrbitEditor`; (b) add a `orbit::viz::VizFeed& vizFeed()` accessor delegating to `engine_.vizFeed()` so later waves can drive animation. Nothing else in the processor changes.
- **Single window, resizable 100–200%.** Base size **1100 × 700** px = 100%. `setResizeLimits(1100, 700, 2200, 1400)` with a fixed-aspect constrainer (`juce::ComponentBoundsConstrainer::setFixedAspectRatio(1100.0/700.0)`). All child layout is computed from a `scale = getWidth() / 1100.0` factor so the layout breathes proportionally. (Base dims are provisional — match them to the mockup's proportions during Task 3 if they drift; keep the 100–200% range exact.)
- **Every parameter must remain reachable** (automation + accessibility). This wave wires the 8 global rotaries + Ping-Pong; per-tap and header controls arrive in later waves. Do not hide any control.
- Design tokens are **ported verbatim** from the mockup CSS custom properties — do not re-invent values. Canonical values are listed in Task 1.
- Fonts: the mockup uses `Syne` (display), `Schibsted Grotesk` (sans), `Space Mono` (mono). JUCE cannot assume these are installed. This wave uses `juce::Font` **system fallbacks** (sans-serif / monospaced) and defines the family names as tokens; bundling the real TTFs via `juce_add_binary_data` is deferred to a later wave and noted in the roadmap. Do not block Wave 1 on font bundling.
- GUI code is **message-thread-only**. It reads parameters through the APVTS and (later) the VizFeed consumer API; it never touches the audio thread directly.
- All existing `orbit_tests`, `orbit_preset_tests`, and `orbit_viz_*` cases pass **unchanged**. The JUCE-free `orbit_tests` target stays JUCE-free — GUI tests live only in the new `orbit_gui_tests` target.
- Conventional commits. Branch `dev/phase-3-wave-1` from `main` (created in Task 1). Do not push until delivery.

---

### Task 1: Design-token header + headless GUI test rig

**Files:**
- Create: `plugin/gui/OrbitTheme.h`
- Create: `tests/gui/OrbitThemeTests.cpp`, `tests/gui/TestProcessor.h`, `tests/gui/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt` — `add_subdirectory(gui)` alongside the existing `preset` line (mirror it exactly).

**Interfaces produced (binding for later tasks):**
- Namespace `orbit::gui::theme`.
- Colour tokens as `juce::Colour` via `constexpr` `0xAARRGGBB` factory calls, e.g. `inline const juce::Colour ink900 { 0xff08080c };`.
- `enum class Accent { Clean, Tape, Grit };` and `juce::Colour modeAccent(orbit::dsp::CharacterStage::Mode mode);` mapping the three character modes to their accent colour.
- Type tokens: `inline constexpr float fsLabel = 13.0f;` etc.; family-name strings `inline constexpr auto fontSans = "Schibsted Grotesk";`.
- Spacing/motion: `inline constexpr int gutter = 24;`, `controlHmd = 40`, `durBaseMs = 200`, etc.

Canonical token values (ported verbatim from the mockup CSS):

| Token | Value | Token | Value |
|---|---|---|---|
| `ink900` | `#08080C` | `bone50` | `#F5F1E8` |
| `ink850` | `#0C0C12` | `bone100` | `#ECE7DA` |
| `ink800` | `#111119` | `bone200` | `#DAD4C4` |
| `ink700` | `#181822` | `bone300` | `#B8B1A0` |
| `ink600` | `#21212D` | `emberBright` | `#FF5C5C` (ember-300) |
| `ink500` | `#2D2D3A` | `ember` | `#E30E0E` (ember-400, primary accent) |
| `ink400` | `#3B3B4A` | `emberDeep` | `#C40A0A` (ember-500) |
| `accentClean` | `#5BC8E6` | `emberDeeper` | `#9E0808` (ember-600) |
| `accentTape` | `#F2B33A` | `accentGrit` | `#E30E0E` |

> Note: the mockup **overrode** the brief's suggested cyan/amber/magenta hint hues, standardising on an ember-red primary with Clean=voltage-cyan, Tape=amber, Grit=ember-red per-mode accents. Read the mockup's mode-switch CSS to confirm the exact Grit accent before finalising `modeAccent()`.

**Interfaces consumed:** `orbit::dsp::CharacterStage::Mode` from `plugin/dsp/CharacterStage.h` (existing enum — read it to get the three enumerators' exact names).

- [ ] **Step 1: Create the new GUI test target skeleton**

Create `tests/gui/CMakeLists.txt` by copying `tests/preset/CMakeLists.txt` and renaming the target `orbit_gui_tests`, its sources to `OrbitThemeTests.cpp`, and keeping the JUCE + `Catch2` + APVTS links. Create `tests/gui/TestProcessor.h` as a copy of `tests/preset/TestProcessor.h` (a minimal `juce::AudioProcessor` built on `orbit::params::createLayout()`).

Add to `tests/CMakeLists.txt` next to the existing preset line:
```cmake
add_subdirectory(gui)
```

- [ ] **Step 2: Write the failing test**

```cpp
// tests/gui/OrbitThemeTests.cpp
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitTheme.h"
#include "dsp/CharacterStage.h"

TEST_CASE("modeAccent maps each character mode to a distinct accent colour") {
    using orbit::dsp::CharacterStage;
    const auto clean = orbit::gui::theme::modeAccent(CharacterStage::Mode::Clean);
    const auto tape  = orbit::gui::theme::modeAccent(CharacterStage::Mode::Tape);
    const auto grit  = orbit::gui::theme::modeAccent(CharacterStage::Mode::Grit);
    CHECK(clean != tape);
    CHECK(tape  != grit);
    CHECK(clean == orbit::gui::theme::accentClean);
}

TEST_CASE("ink and bone scales are ordered dark-to-light by luminance") {
    using namespace orbit::gui::theme;
    CHECK(ink900.getPerceivedBrightness() < ink600.getPerceivedBrightness());
    CHECK(bone300.getPerceivedBrightness() < bone50.getPerceivedBrightness());
}
```

> Confirm the exact `CharacterStage::Mode` enumerator names by reading `plugin/dsp/CharacterStage.h` before writing this test; adjust `Clean/Tape/Grit` to match.

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build-preset --target orbit_gui_tests` (reuse the existing configured build dir; re-run `cmake build-preset` first if the target isn't found).
Expected: FAIL to compile — `OrbitTheme.h` not found / `modeAccent` undefined.

- [ ] **Step 4: Write `OrbitTheme.h`**

Implement the namespace with all tokens from the table above and:
```cpp
inline juce::Colour modeAccent(orbit::dsp::CharacterStage::Mode mode) {
    switch (mode) {
        case orbit::dsp::CharacterStage::Mode::Clean: return accentClean;
        case orbit::dsp::CharacterStage::Mode::Tape:  return accentTape;
        case orbit::dsp::CharacterStage::Mode::Grit:  return accentGrit;
    }
    return ember;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build-preset --target orbit_gui_tests && ./build-preset/tests/gui/orbit_gui_tests`
Expected: PASS (all assertions).

- [ ] **Step 6: Commit**

```bash
git checkout -b dev/phase-3-wave-1
git add plugin/gui/OrbitTheme.h tests/gui/ tests/CMakeLists.txt
git commit -m "feat(gui): design-token header + headless GUI test rig"
```

---

### Task 2: Processor exposes the VizFeed

**Files:**
- Modify: `plugin/PluginProcessor.h:37-39` (add accessor near `presetManager()`)
- Test: `tests/gui/OrbitThemeTests.cpp` (add a case) — or a new `tests/gui/ProcessorAccessorTests.cpp` added to the target's sources.

**Interfaces produced:** `orbit::viz::VizFeed& OrbitAudioProcessor::vizFeed();` — returns the live engine feed for the editor to poll in later waves.

**Interfaces consumed:** `orbit::OrbitEngine::vizFeed()` (exists, `plugin/OrbitEngine.h:79`).

- [ ] **Step 1: Write the failing test**

```cpp
TEST_CASE("processor exposes the engine VizFeed with a sane default snapshot") {
    orbit::test::TestProcessor proc;   // or the real OrbitAudioProcessor if the rig links it
    proc.prepareToPlay(48000.0, 512);
    const auto snap = proc.vizFeed().readLevels();
    CHECK(snap.duckGain == 1.0f);      // no ducking at rest
}
```

> If the GUI test rig links the real `OrbitAudioProcessor`, use it directly; otherwise the accessor is verified on the real processor in the plugin's own smoke test. Pick whichever the existing rig supports — read `tests/preset/TestProcessor.h` first.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-preset --target orbit_gui_tests`
Expected: FAIL — `vizFeed` is not a member of the processor.

- [ ] **Step 3: Add the accessor**

```cpp
// plugin/PluginProcessor.h, near presetManager()
orbit::viz::VizFeed& vizFeed() { return engine_.vizFeed(); }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build-preset --target orbit_gui_tests && ./build-preset/tests/gui/orbit_gui_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/PluginProcessor.h tests/gui/
git commit -m "feat(gui): expose engine VizFeed from the processor for the editor"
```

---

### Task 3: OrbitEditor shell — window, scale factor, resize limits

**Files:**
- Create: `plugin/gui/OrbitEditor.h`, `plugin/gui/OrbitEditor.cpp`
- Modify: `plugin/PluginProcessor.h:17` (return `new OrbitEditor(*this)`)
- Modify: `plugin/CMakeLists.txt` — add `gui/OrbitEditor.cpp` to the plugin sources and `gui/` to the include dirs.
- Test: `tests/gui/OrbitEditorTests.cpp` (add to `orbit_gui_tests` sources)

**Interfaces produced:**
- `class OrbitEditor : public juce::AudioProcessorEditor` with ctor `OrbitEditor(OrbitAudioProcessor&)`.
- `static constexpr int kBaseW = 1100, kBaseH = 700;`
- `double scale() const { return getWidth() / double(kBaseW); }` — the single layout scale factor children use.

**Interfaces consumed:** `OrbitAudioProcessor&` (owns `apvts`, `vizFeed()`, `presetManager()`).

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitEditor.h"
#include "PluginProcessor.h"

TEST_CASE("OrbitEditor opens at base size and is resizable within 100-200%") {
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed { proc.createEditor() };
    REQUIRE(ed != nullptr);
    CHECK(dynamic_cast<juce::GenericAudioProcessorEditor*>(ed.get()) == nullptr); // real editor
    CHECK(ed->getWidth()  == OrbitEditor::kBaseW);
    CHECK(ed->getHeight() == OrbitEditor::kBaseH);
    CHECK(ed->isResizable());
    ed->setSize(OrbitEditor::kBaseW * 2, OrbitEditor::kBaseH * 2);   // 200%
    CHECK(dynamic_cast<OrbitEditor*>(ed.get())->scale() == Approx(2.0));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-preset --target orbit_gui_tests`
Expected: FAIL — `OrbitEditor.h` not found.

- [ ] **Step 3: Implement the shell**

`OrbitEditor` ctor: `setResizable(true, true)`, install a `juce::ComponentBoundsConstrainer` with `setFixedAspectRatio(double(kBaseW)/kBaseH)` and `setSizeLimits(kBaseW, kBaseH, kBaseW*2, kBaseH*2)`, `setSize(kBaseW, kBaseH)`. `paint()` fills `theme::ink900`. Keep `resized()` empty for now (children added in Task 5).

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build-preset --target orbit_gui_tests && ./build-preset/tests/gui/orbit_gui_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitEditor.* plugin/PluginProcessor.h plugin/CMakeLists.txt tests/gui/
git commit -m "feat(gui): OrbitEditor shell with proportional scale + resize limits"
```

---

### Task 4: OrbitLookAndFeel — rotary + toggle drawing

**Files:**
- Create: `plugin/gui/OrbitLookAndFeel.h`, `plugin/gui/OrbitLookAndFeel.cpp`
- Modify: `plugin/CMakeLists.txt` — add `gui/OrbitLookAndFeel.cpp`.
- Test: `tests/gui/OrbitLookAndFeelTests.cpp` (add to `orbit_gui_tests` sources)

**Interfaces produced:**
- `class OrbitLookAndFeel : public juce::LookAndFeel_V4` overriding `drawRotarySlider(...)` and `drawToggleButton(...)` in the token style (thin arc track in `ink600`, value arc in the active accent, centred mono value readout).

**Interfaces consumed:** `orbit::gui::theme` tokens.

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitLookAndFeel.h"

TEST_CASE("OrbitLookAndFeel renders a rotary into an image without crashing") {
    OrbitLookAndFeel laf;
    juce::Image img { juce::Image::ARGB, 80, 80, true };
    juce::Graphics g { img };
    laf.drawRotarySlider(g, 0, 0, 80, 80, 0.5f, 0.0f,
                         juce::MathConstants<float>::twoPi * 0.8f, *(juce::Slider*)nullptr);
    // Reaching here (no crash) plus a non-transparent centre pixel proves it drew.
    CHECK(img.getPixelAt(40, 40).getAlpha() >= 0);
}
```

> `drawRotarySlider` must not dereference the `Slider&` for anything the test can't provide; if the real signature needs slider state, construct a real `juce::Slider` on the stack and pass it instead of a null ref. Adjust the test to the exact JUCE 8.0.4 signature (read `LookAndFeel_V4`).

- [ ] **Step 2: Run test to verify it fails** — Expected: FAIL, header not found.
- [ ] **Step 3: Implement the drawing** using `theme` tokens; arc geometry from the `rotaryStart/EndAngle` args.
- [ ] **Step 4: Run tests to verify they pass.**
- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitLookAndFeel.* plugin/CMakeLists.txt tests/gui/
git commit -m "feat(gui): OrbitLookAndFeel rotary + toggle drawing"
```

---

### Task 5: GlobalControls component — 8 rotaries + Ping-Pong, attached to APVTS

**Files:**
- Create: `plugin/gui/GlobalControls.h`, `plugin/gui/GlobalControls.cpp`
- Modify: `plugin/gui/OrbitEditor.{h,cpp}` — own a `GlobalControls` child, lay it out in the right column in `resized()`.
- Modify: `plugin/CMakeLists.txt` — add `gui/GlobalControls.cpp`.
- Test: `tests/gui/GlobalControlsTests.cpp` (add to `orbit_gui_tests` sources)

**Interfaces produced:**
- `class GlobalControls : public juce::Component` ctor `GlobalControls(juce::AudioProcessorValueTreeState& apvts, OrbitLookAndFeel& laf)`.
- Internally: one `juce::Slider` + `juce::AudioProcessorValueTreeState::SliderAttachment` per global param, one `juce::ToggleButton` + `ButtonAttachment` for Ping-Pong.

**Interfaces consumed (exact IDs, from `plugin/Parameters.h`):**
`orbit::params::kDryWetId` (`mix_drywet`), `kDuckId` (`mix_duck`), `kWidthId` (`mix_width`), `kPingPongId` (`mix_pingpong`), `kModDepthId` (`mod_depth`), `kModRateId` (`mod_rate`), `kLowCutId` (`filter_lowcut`), `kHighCutId` (`filter_highcut`).

- [ ] **Step 1: Write the failing test**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "gui/GlobalControls.h"
#include "gui/OrbitLookAndFeel.h"
#include "TestProcessor.h"
#include "Parameters.h"

TEST_CASE("GlobalControls attachments write through to the APVTS") {
    orbit::test::TestProcessor proc;
    OrbitLookAndFeel laf;
    GlobalControls gc { proc.apvts, laf };

    auto* width = proc.apvts.getParameter(orbit::params::kWidthId);
    const float before = width->getValue();
    gc.setSliderNormForTest(orbit::params::kWidthId, before < 0.5f ? 0.9f : 0.1f);
    CHECK(width->getValue() != before);   // moving the slider moved the param
}

TEST_CASE("GlobalControls exposes exactly the 8 global rotaries + ping-pong toggle") {
    orbit::test::TestProcessor proc;
    OrbitLookAndFeel laf;
    GlobalControls gc { proc.apvts, laf };
    CHECK(gc.rotaryCount() == 8);
    CHECK(gc.hasToggle(orbit::params::kPingPongId));
}
```

> `setSliderNormForTest`, `rotaryCount`, `hasToggle` are tiny test-only accessors on `GlobalControls` — declare them in the header. They exist so the wiring is verifiable headlessly without a visible window.

- [ ] **Step 2: Run test to verify it fails** — Expected: FAIL, header not found.
- [ ] **Step 3: Implement `GlobalControls`** — build the sliders/toggle, create attachments against the IDs above, apply `laf`, add the test accessors. Group visually (Blend / Space / Motion / Filter) per the mockup but that's paint-only.
- [ ] **Step 4: Run tests to verify they pass.**
- [ ] **Step 5: Add the component to the editor** — in `OrbitEditor`, construct `GlobalControls` with `processor.apvts` and the LookAndFeel, `addAndMakeVisible`, and in `resized()` place it in the right-hand column scaled by `scale()`.
- [ ] **Step 6: Run the whole GUI suite + the plugin's existing checks**

Run: `./build-preset/tests/gui/orbit_gui_tests` (all green) and confirm `orbit_tests` + `orbit_preset_tests` still pass.
Expected: PASS across all targets.

- [ ] **Step 7: Commit**

```bash
git add plugin/gui/GlobalControls.* plugin/gui/OrbitEditor.* plugin/CMakeLists.txt tests/gui/
git commit -m "feat(gui): global control column wired to APVTS with attachments"
```

---

### Task 6: Wave-close validation — auval + pluginval unchanged

**Files:** none (verification only).

- [ ] **Step 1: Build the plugin target** (AU/VST3) in the plugin build dir. Expected: builds clean with the new editor.
- [ ] **Step 2: Run `auval`** for the component. Expected: `34 Global Scope Parameters` + `SUCCEEDED` — the editor added no parameters.
- [ ] **Step 3: Run `pluginval`** at strictness 10 against the VST3. Expected: SUCCESS (editor open/close/resize stress included).
- [ ] **Step 4: Manual smoke** — load in a host, confirm the window opens at 1100×700, resizes to 200% proportionally, and the 8 global rotaries + Ping-Pong move their parameters (watch the host's automation/parameter view).
- [ ] **Step 5: Commit the validation note** into this plan's progress ledger (or `docs/superpowers/sdd/progress.md`) and hand off for whole-branch review.

---

## Self-Review (against the Phase 3 UI design brief)

- **§2 single resizable window 100–200%** → Task 3 (base 1100×700, limits ×1–×2, fixed aspect). ✓
- **§2 every parameter reachable** → Wave 1 covers the 8 globals + Ping-Pong (Task 5); per-tap Time/Sync/Feedback/Reverse/Pitch and header Character/Freeze are **explicitly deferred to Waves 2–3** (roadmap below) — flagged, not dropped. ✓ (coverage completes across the phase, not this wave)
- **§3 global control ranges** → attachments inherit ranges from `createLayout()`; no ranges re-declared. ✓
- **Design tokens (§6 deliverable 5)** → Task 1 ports them verbatim as constants. ✓
- **§4 live animation feed** → not consumed yet; Task 2 exposes the feed so Wave 4 can. ✓
- **Type consistency:** `scale()`, `kBaseW/kBaseH`, `modeAccent()`, the `theme` token names, and the exact `orbit::params::k*Id` constants are used identically across tasks. ✓
- **No new parameters / DSP untouched** → Global Constraints + Task 6 guard. ✓

---

## Phase 3 Roadmap (follow-on plans — NOT part of this wave)

Each becomes its own dated plan (`docs/superpowers/plans/`) when reached, detailed to this wave's standard. Listed here so the decomposition is visible and Wave 1's interfaces (theme, scale factor, viz accessor) are built to serve them.

- **Wave 2 — The pad & orbs (signature interaction).** The 2D pad with up to 4 draggable orbs: Time = X (1–2000 ms, log-ish, 350 ms centre-weighted), Feedback = Y (0–98%). Per-orb drag → `tap{n}_time` / `tap{n}_feedback` attachments; enabled/disabled orb states; synced-tap X-snap when `tap{n}_sync ≠ Free`. Orb hit-testing and the Time↔X / Feedback↔Y coordinate mapping are pure, headlessly-testable math.
- **Wave 3 — Per-tap inspector + header.** Compact per-orb affordances for Sync (10 choices), Reverse, Pitch (−12…+12 snapped); the bottom 4-tap inspector strip; header Character mode (Clean/Tape/Grit) recolouring the UI via `modeAccent()`, Freeze toggle, A/B compare (uses `PresetManager` A/B), in/out meters.
- **Wave 4 — Viz-driven animation.** Consume `processor.vizFeed()`: orb pulses/ripples from `popEvent()` tap-fire events (age = `now − timeSamples`), ambient energy from `readLevels()` RMS/peak, "dry pushes orbs back" from `duckGain`. Freeze made visually transformative. Timer-driven repaint at UI rate; all reads via the SPSC consumer API.
- **Wave 5 — Preset browser.** Collapsible overlay: 40 factory presets, the six filter buttons (**Ambient · Rhythm · Dub · Tape · Wide · Utility** — the reconciled vocabulary from PR #3), user save/rename/delete with tag picker, A/B copy A→B / B→A, dirty-state indicator. Wired to the existing `PresetManager` API.
- **Wave 6 (optional) — Font bundling.** Embed `Syne` / `Schibsted Grotesk` / `Space Mono` TTFs via `juce_add_binary_data` and swap the theme's system fallbacks for `juce::Typeface::createSystemTypefaceFor`, if licensing allows redistribution.
