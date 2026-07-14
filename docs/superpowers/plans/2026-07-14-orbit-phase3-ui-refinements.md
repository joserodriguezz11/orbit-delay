# Orbit Phase 3 — UI Refinements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-base the editor to 900×550, add a settings (About / How to Use) overlay behind a header gear, fix the A/B double-draw, add a PRESETS caption, remove the SYN-FX-001 texts, and eliminate label/knob overlaps and the rail dead zone.

**Architecture:** All changes are GUI-only, on branch `dev/phase-3-wave-1`, spec `docs/superpowers/specs/2026-07-14-orbit-phase3-ui-refinements-design.md`. Components lay out in design pixels scaled by one editor transform; the re-base therefore happens in `OrbitTheme.h` constants plus targeted per-component re-spacing. The settings panel clones the `OrbitPresetBrowser` overlay idiom.

**Tech Stack:** JUCE 8 (C++20), Catch2 (`orbit_gui_tests` headless rig), CMake tree `build-release`.

## Global Constraints

- GUI-only: no parameter, DSP, preset-format, or state changes.
- Design tokens are a contract with `docs/design/orbit-delay-full-v2.html`; deviations must be recorded in `docs/design/README.md` (Task 7).
- The four layout constants must partition the window exactly (asserted in tests).
- Conventional commits on `dev/phase-3-wave-1`; do not push.
- Test command: `cmake --build ~/orbit-delay/build-release --target orbit_gui_tests -j8 && ~/orbit-delay/build-release/tests/gui/orbit_gui_tests` (run from `~/orbit-delay`).
- Every commit ends with trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

---

### Task 1: Theme re-base to 900×550

**Files:**
- Modify: `plugin/gui/OrbitTheme.h:19-25`
- Test: `tests/gui/OrbitThemeTests.cpp:12-23`

**Interfaces:**
- Consumes: nothing.
- Produces: `theme::kWindowW == 900`, `kWindowH == 550`, `kHeaderH == 52`, `kPadW == 730`, `kPadH == 402`, `kRailW == 170`, `kTapStripH == 96`. Every later task's geometry derives from these. `OrbitEditor::kBaseW/kBaseH` and the clamp test pick these up automatically.

- [ ] **Step 1: Update the failing test first**

In `tests/gui/OrbitThemeTests.cpp`, replace the body of the partition test (lines 12–23) with:

```cpp
TEST_CASE("layout metrics partition the 900x550 window exactly") {
    CHECK(theme::kWindowW == 900);
    CHECK(theme::kWindowH == 550);

    CHECK(theme::kHeaderH + theme::kPadH + theme::kTapStripH == theme::kWindowH);

    CHECK(theme::kPadW + theme::kRailW == theme::kWindowW);

    CHECK(theme::kPadW == 730);
    CHECK(theme::kPadH == 402);
    CHECK(theme::kTapStripH == 96);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests "[theme]"` — if the test file has no tags, run the full binary and filter by name: `./build-release/tests/gui/orbit_gui_tests "layout metrics*"`.
Expected: FAIL (`kWindowW == 900` is false — still 1180).

- [ ] **Step 3: Change the constants**

In `plugin/gui/OrbitTheme.h`, replace lines 15–25 with:

```cpp
// ------------------------------------------------------------ layout metrics
// Re-based 2026-07-14 (spec: phase3-ui-refinements): 900x550 window — a 52px
// header, the 730x402 orb pad beside a 170px control rail, and a 96px tap
// strip. Deliberate deviation from the 1180x720 mockup (README deviations).
// The four constants partition the window exactly (asserted in OrbitThemeTests).
inline constexpr int kWindowW   = 900;
inline constexpr int kWindowH   = 550;
inline constexpr int kHeaderH   = 52;
inline constexpr int kPadW      = 730;
inline constexpr int kPadH      = 402;
inline constexpr int kRailW     = kWindowW - kPadW;   // 170
inline constexpr int kTapStripH = kWindowH - kHeaderH - kPadH;  // 96
```

- [ ] **Step 4: Build and run the full GUI suite**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: theme + editor clamp tests PASS (clamp derives from `OrbitEditor::kBaseW/kBaseH`). If any other test hard-codes 1180/720/990/572, update it to derive from `theme::` constants instead of literals — same assertion intent.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitTheme.h tests/gui/OrbitThemeTests.cpp
git commit -m "feat(gui): re-base window to 900x550

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: Header re-space at 900w — capsule, PRESETS caption, meters clearance, non-overlap test

**Files:**
- Modify: `plugin/gui/OrbitHeader.cpp` (namespace constants, `resized()`, `paint()`)
- Modify: `plugin/gui/OrbitHeader.h` (add `gearBounds()` accessor stub — full gear arrives in Task 6, reserve the space now)
- Test: `tests/gui/OrbitHeaderTests.cpp`

**Interfaces:**
- Consumes: `theme::kWindowW == 900` (Task 1).
- Produces: header child layout with no intersecting bounds; `juce::Rectangle<int> OrbitHeader::gearBounds() const` (the gear hit-area rect, used by Task 6). Capsule constant becomes `kCapsuleW = 220`.

- [ ] **Step 1: Write the failing non-overlap test**

Append to `tests/gui/OrbitHeaderTests.cpp`:

```cpp
TEST_CASE("header children and the gear slot do not overlap at base size") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    header.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kHeaderH);

    std::vector<juce::Rectangle<int>> rects;
    for (auto* c : header.getChildren())
        if (c->isVisible() && !c->getBounds().isEmpty())
            rects.push_back(c->getBounds());
    rects.push_back(header.gearBounds());

    for (size_t i = 0; i < rects.size(); ++i)
        for (size_t j = i + 1; j < rects.size(); ++j) {
            INFO("rect " << int(i) << " vs " << int(j));
            CHECK(!rects[i].intersects(rects[j]));
        }
    // Everything must fit inside the header.
    for (const auto& r : rects)
        CHECK(header.getLocalBounds().contains(r));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests "header children*"`
Expected: FAIL to compile — `gearBounds()` doesn't exist yet.

- [ ] **Step 3: Implement the re-space**

`plugin/gui/OrbitHeader.h` — after the `selectSlot` declaration add:

```cpp
    // Reserved hit area for the settings gear (wired in the settings task).
    juce::Rectangle<int> gearBounds() const { return gearBounds_; }
```

and in the private section, after `presetCapsule_`:

```cpp
    juce::Rectangle<int> gearBounds_;
```

`plugin/gui/OrbitHeader.cpp` — change the capsule constant (line 9) to:

```cpp
constexpr int kCapsuleX = 130, kCapsuleW = 220, kCapsuleH = 30;
```

Replace `resized()` (lines 98–112) with:

```cpp
void OrbitHeader::resized() {
    prev_.setBounds(kCapsuleX + 4, (52 - 24) / 2, 24, 24);
    next_.setBounds(kCapsuleX + kCapsuleW - 28, (52 - 24) / 2, 24, 24);
    presetCapsule_ = { kCapsuleX + 30, (52 - kCapsuleH) / 2,
                       kCapsuleW - 60, kCapsuleH };
    slotA_.setBounds(kCapsuleX + kCapsuleW + 10, (52 - 24) / 2, 24, 24);
    slotB_.setBounds(kCapsuleX + kCapsuleW + 38, (52 - 24) / 2, 24, 24);

    // Right side: seg, freeze, gear slot, divider, meters (design order).
    const int W = theme::kWindowW;
    seg_.setBounds(W - 290 - 156, (52 - 28) / 2, 156, 28);
    freeze_.setBounds(W - 290 + 10, (52 - 26) / 2, 66, 26);
    gearBounds_ = { W - 110, (52 - 26) / 2, 26, 26 };
    inMeter_.setBounds(W - 46, 8, 5, 24);
    outMeter_.setBounds(W - 33, 8, 5, 24);
}
```

In `paint()`, after the preset-capsule block (after line 180) add the caption:

```cpp
    // PRESETS caption under the capsule (same idiom as the logo tagline).
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.32f));
    g.setColour(theme::bone50.withAlpha(0.42f));
    g.drawText("PRESETS",
               juce::Rectangle<float>(float(kCapsuleX), 43.0f, float(kCapsuleW), 7.0f),
               juce::Justification::centred, false);
```

Still in `paint()`, replace the meter-labels block (lines 200–208) with (labels sit ≥2px under the shortened bars):

```cpp
    // Meter labels + right divider.
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(float(theme::kWindowW - 60), (H - 22.0f) / 2.0f, 1.0f, 22.0f);
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.2f));
    g.setColour(theme::bone50.withAlpha(0.4f));
    g.drawText("IN", juce::Rectangle<float>(float(theme::kWindowW - 50), 38.0f, 13.0f, 7.0f),
               juce::Justification::centred, false);
    g.drawText("OUT", juce::Rectangle<float>(float(theme::kWindowW - 38), 38.0f, 15.0f, 7.0f),
               juce::Justification::centred, false);
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: PASS, including the new non-overlap test. (Layout is design-pixel; the editor transform scales uniformly, so one base-size check covers all scales.)

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitHeader.h plugin/gui/OrbitHeader.cpp tests/gui/OrbitHeaderTests.cpp
git commit -m "feat(gui): header re-space at 900w + PRESETS caption + meter label clearance

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: A/B single-draw fix

**Files:**
- Modify: `plugin/gui/OrbitHeader.h:40`, `plugin/gui/OrbitHeader.cpp` (constructor, `paint()`)
- Test: `tests/gui/OrbitHeaderTests.cpp`

**Interfaces:**
- Consumes: header from Task 2.
- Produces: `juce::TextButton& OrbitHeader::slotAButton()`, `slotBButton()` accessors; both buttons carry empty text (LookAndFeel paints nothing), letters drawn once by `slotRing`.

- [ ] **Step 1: Write the failing test**

Append to `tests/gui/OrbitHeaderTests.cpp`:

```cpp
TEST_CASE("A/B slot buttons carry no LookAndFeel text (letters painted once)") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    CHECK(header.slotAButton().getButtonText().isEmpty());
    CHECK(header.slotBButton().getButtonText().isEmpty());
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8`
Expected: FAIL to compile — `slotAButton()` doesn't exist.

- [ ] **Step 3: Implement**

`OrbitHeader.h` line 40 — construct the buttons with component-names only, no button text (the second ctor arg stays empty by using `setName` semantics of the single-arg ctor: pass empty text explicitly):

```cpp
    juce::TextButton slotA_ { juce::String() }, slotB_ { juce::String() };
```

Add public accessors next to `characterSeg()`:

```cpp
    juce::TextButton& slotAButton() { return slotA_; }
    juce::TextButton& slotBButton() { return slotB_; }
```

`OrbitHeader.cpp` — the `slotRing` lambda in `paint()` (lines 183–198) currently reads `b.getButtonText()`; now the text is empty, so draw the literal letters. Replace the two call lines (197–198) and the lambda's `drawText` line (195):

```cpp
        g.drawText(letter, r, juce::Justification::centred, false);
```

with the lambda signature gaining the letter:

```cpp
    const auto slotRing = [&] (const juce::TextButton& b, const juce::String& letter,
                               bool active) {
```

and the calls becoming:

```cpp
    slotRing(slotA_, "A", !shownSlotB_);
    slotRing(slotB_, "B", shownSlotB_);
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: PASS (new test + existing A/B slot behavior tests).

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitHeader.h plugin/gui/OrbitHeader.cpp tests/gui/OrbitHeaderTests.cpp
git commit -m "fix(gui): A/B letters drawn once — clear TextButton text, slotRing is sole painter

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: OrbPad — remove SYN-FX-001 line, suppress scale labels under riding knobs

**Files:**
- Modify: `plugin/gui/OrbPad.h` (orbpad namespace), `plugin/gui/OrbPad.cpp` (predicate impl, paint rulers block, delete watermark sub-line)
- Test: `tests/gui/OrbPadTests.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `bool orbpad::scaleLabelVisible(juce::Rectangle<float> label, juce::Rectangle<float> timeKnob, juce::Rectangle<float> fbKnob)` — pure, returns false when the label rect intersects either knob rect expanded by 26px (the scrim pool + label halo of a 56px knob).

- [ ] **Step 1: Write the failing test**

Append to `tests/gui/OrbPadTests.cpp`:

```cpp
TEST_CASE("scale labels hide under a riding knob and return when it moves off") {
    const juce::Rectangle<float> timeKnob { 300.0f, 308.0f, 56.0f, 56.0f };
    const juce::Rectangle<float> fbKnob   {  28.0f, 120.0f, 56.0f, 56.0f };

    // Bottom ms label directly under the TIME knob — hidden (26px halo).
    CHECK(!orbpad::scaleLabelVisible({ 298.0f, 383.0f, 60.0f, 10.0f }, timeKnob, fbKnob));
    // Same label with TIME knob far away — visible.
    CHECK(orbpad::scaleLabelVisible({ 298.0f, 383.0f, 60.0f, 10.0f },
                                    { 600.0f, 308.0f, 56.0f, 56.0f }, fbKnob));
    // Left 50% label beside the FEEDBACK knob — hidden.
    CHECK(!orbpad::scaleLabelVisible({ 9.0f, 145.0f, 24.0f, 10.0f }, timeKnob, fbKnob));
    // Left label well below the FEEDBACK knob — visible.
    CHECK(orbpad::scaleLabelVisible({ 9.0f, 320.0f, 24.0f, 10.0f }, timeKnob, fbKnob));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8`
Expected: FAIL to compile — `scaleLabelVisible` not declared.

- [ ] **Step 3: Implement**

`OrbPad.h`, inside `namespace orbpad` after `stepHeat`:

```cpp
// Scale-label suppression: a ruler label hides while a riding knob (56px,
// with its ~26px scrim/label halo) overlaps it, and returns when clear.
bool scaleLabelVisible(juce::Rectangle<float> label,
                       juce::Rectangle<float> timeKnob,
                       juce::Rectangle<float> fbKnob);
```

`OrbPad.cpp`, next to the other `orbpad` free functions:

```cpp
bool orbpad::scaleLabelVisible(juce::Rectangle<float> label,
                               juce::Rectangle<float> timeKnob,
                               juce::Rectangle<float> fbKnob) {
    return !label.intersects(timeKnob.expanded(26.0f))
        && !label.intersects(fbKnob.expanded(26.0f));
}
```

In `paint()`'s etched-rulers block (lines 576–599), gate only the *text* draws (tick marks stay). Before the loops add:

```cpp
        const auto tkb = timeKnob_.getBounds().toFloat();
        const auto fkb = fbKnob_.getBounds().toFloat();
```

Bottom row: wrap the `drawText` (lines 586–588):

```cpp
            const juce::Rectangle<float> lr { gx - 30.0f, H - 19.0f, 60.0f, 10.0f };
            if (orbpad::scaleLabelVisible(lr, tkb, fkb)) {
                g.setColour(theme::bone50.withAlpha(0.28f));
                g.drawText(tk.label, lr, juce::Justification::centred, false);
            }
```

Left column: wrap the `drawText` (lines 594–597):

```cpp
            const juce::Rectangle<float> lr { 9.0f, py - 5.0f, 24.0f, 10.0f };
            if (orbpad::scaleLabelVisible(lr, tkb, fkb)) {
                g.setColour(theme::bone50.withAlpha(0.28f));
                g.drawText(juce::String(int(fb)), lr,
                           juce::Justification::centredLeft, false);
            }
```

Delete the SYN-FX-001 watermark sub-line (lines 464–468) — the `ORBIT` watermark above it stays:

```cpp
    // (removed) SYN·FX·001 — STEREO MULTI-TAP ECHO sub-line (spec: text removal)
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbPad.h plugin/gui/OrbPad.cpp tests/gui/OrbPadTests.cpp
git commit -m "feat(gui): hide ruler labels under riding knobs; drop SYN-FX-001 pad line

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: Rail — remove footer stamp, distribute sections over full height

**Files:**
- Modify: `plugin/gui/OrbitRail.h` (layout helper decl + comment), `plugin/gui/OrbitRail.cpp` (helper impl, `resized()`, `paint()`, delete footer)
- Test: `tests/gui/OrbitRailStripTests.cpp`

**Interfaces:**
- Consumes: `theme::kPadH == 402` (rail height).
- Produces: `static float OrbitRail::sectionGap(float railHeight)` — the single inter-section gap that makes BLEND/SPACE/MOTION/FILTER fill the column evenly. Fixed content model: top pad 14 + bottom pad 14 + section blocks (header 13+9 + row + label 12, rows 56/44/44/44, BLEND +9 meter clearance) = 361; `gap = (railHeight - 361) / 3`, floored at 6.

- [ ] **Step 1: Write the failing test**

Append to `tests/gui/OrbitRailStripTests.cpp`:

```cpp
TEST_CASE("rail sections distribute evenly over the full column height") {
    // Fixed content is 361 design px; three equal gaps absorb the rest.
    CHECK(OrbitRail::sectionGap(402.0f) == Approx((402.0f - 361.0f) / 3.0f));
    // Never collapses below the 6px floor, even in a too-short column.
    CHECK(OrbitRail::sectionGap(300.0f) == Approx(6.0f));
}
```

(If `Approx` is not already imported in this file, add `#include <catch2/catch_approx.hpp>` and `using Catch::Approx;` at the top.)

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8`
Expected: FAIL to compile — `sectionGap` not declared.

- [ ] **Step 3: Implement**

`OrbitRail.h` — update the class comment (drop "then the SYN·FX·001 footer"), and add in the public section:

```cpp
    // Even vertical distribution: the gap between sections that fills
    // railHeight exactly (fixed content = 361 design px), floored at 6.
    static float sectionGap(float railHeight);
```

`OrbitRail.cpp` — implement above the constructor:

```cpp
float OrbitRail::sectionGap(float railHeight) {
    constexpr float kFixedContent = 361.0f;   // pads + headers + rows + labels
    return std::max(6.0f, (railHeight - kFixedContent) / 3.0f);
}
```

In `resized()` and `paint()`, replace every use of `kSectionGap` with a local `const float gap = sectionGap(float(getHeight()));` computed once at the top of each function (delete the `kSectionGap` constant at line 13). The y-advance lines become e.g. in `resized()`:

```cpp
    y += 56.0f + kLabelH + 9.0f + gap + kHeaderH + kHeaderGap;   // after BLEND
    ...
    y += 44.0f + kLabelH + gap + kHeaderH + kHeaderGap;          // after SPACE, MOTION
```

and in `paint()`:

```cpp
    y += 56.0f + kLabelH + 9.0f + gap;   // after BLEND
    y += 44.0f + kLabelH + gap;          // after SPACE, MOTION
```

Delete the footer block in `paint()` (lines 164–178, the `// Footer: mark + catalogue number.` braces) entirely.

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitRail.h plugin/gui/OrbitRail.cpp tests/gui/OrbitRailStripTests.cpp
git commit -m "feat(gui): rail fills its column evenly; drop SYN-FX-001 footer stamp

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: Settings — gear button, OrbitSettingsPanel overlay, editor wiring

**Files:**
- Create: `plugin/gui/OrbitSettingsPanel.h`, `plugin/gui/OrbitSettingsPanel.cpp`
- Modify: `plugin/gui/OrbitHeader.h` (callback + gear paint/hit), `plugin/gui/OrbitHeader.cpp`, `plugin/gui/OrbitEditor.h`, `plugin/gui/OrbitEditor.cpp`, `plugin/CMakeLists.txt:53` (source list), `tests/gui/CMakeLists.txt` (source list + new test file)
- Test: Create `tests/gui/OrbitSettingsTests.cpp`

**Interfaces:**
- Consumes: `gearBounds_` (Task 2), overlay idiom from `OrbitPresetBrowser`.
- Produces: `class OrbitSettingsPanel : public juce::Component` with `std::function<void()> onClose` and `juce::Rectangle<float> panelBounds() const`; `std::function<void()> OrbitHeader::onSettingsToggle`; `OrbitSettingsPanel& OrbitEditor::settingsPanel()`.

- [ ] **Step 1: Write the failing tests**

Create `tests/gui/OrbitSettingsTests.cpp`:

```cpp
// Settings overlay: gear toggle, close paths, mutual exclusion with the
// preset browser. Mirrors the browser-overlay test idiom.
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitEditor.h"
#include "gui/OrbitSettingsPanel.h"
#include "PluginProcessor.h"

TEST_CASE("gear toggle shows and hides the settings panel") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> raw { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(raw.get());
    REQUIRE(ed != nullptr);

    CHECK(!ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(!ed->settingsPanel().isVisible());
}

TEST_CASE("settings and preset browser are mutually exclusive") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> raw { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(raw.get());
    REQUIRE(ed != nullptr);

    ed->header().onSettingsToggle();
    REQUIRE(ed->settingsPanel().isVisible());
    ed->header().onBrowserToggle();
    CHECK(ed->presetBrowser().isVisible());
    CHECK(!ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(ed->settingsPanel().isVisible());
    CHECK(!ed->presetBrowser().isVisible());
}

TEST_CASE("clicking outside the panel closes settings") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitSettingsPanel panel;
    bool closed = false;
    panel.onClose = [&closed] { closed = true; };
    panel.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kWindowH);

    const auto outside = panel.panelBounds().getBottomRight() + juce::Point<float>(20.0f, 20.0f);
    juce::MouseEvent up { juce::Desktop::getInstance().getMainMouseSource(),
                          outside, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          &panel, &panel, juce::Time::getCurrentTime(), outside,
                          juce::Time::getCurrentTime(), 1, false };
    panel.mouseUp(up);
    CHECK(closed);
}
```

Add `OrbitSettingsTests.cpp` to the `add_executable(orbit_gui_tests ...)` list in `tests/gui/CMakeLists.txt` (next to `OrbitHeaderTests.cpp`) and `${CMAKE_SOURCE_DIR}/plugin/gui/OrbitSettingsPanel.cpp` next to the `OrbitPresetBrowser.cpp` entry.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build-release --target orbit_gui_tests -j8`
Expected: FAIL to compile — `OrbitSettingsPanel.h` not found.

- [ ] **Step 3: Create the panel**

`plugin/gui/OrbitSettingsPanel.h`:

```cpp
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OrbitTheme.h"

// Settings overlay (gear in the header): ABOUT + HOW TO USE, static text,
// same idiom as OrbitPresetBrowser — scrim, rounded panel, x close button,
// click-outside closes. Owns no state and touches no parameters.
class OrbitSettingsPanel : public juce::Component {
public:
    OrbitSettingsPanel() = default;

    std::function<void()> onClose;

    juce::Rectangle<float> panelBounds() const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitSettingsPanel)
};
```

`plugin/gui/OrbitSettingsPanel.cpp`:

```cpp
#include "OrbitSettingsPanel.h"
#include "OrbitFonts.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

#ifndef JucePlugin_VersionString
#define JucePlugin_VersionString "dev"   // console test rig has no plugin defines
#endif

namespace {
constexpr float kPanelW = 560.0f, kPanelH = 390.0f;

const char* const kHowTo[] = {
    "Drag an orb - x sets tap time, y sets feedback. Flick to throw.",
    "TIME and FEEDBACK knobs ride the selected orb's axes.",
    "SYNC locks a tap to tempo divisions at the host BPM.",
    "CLEAN / TAPE / GRIT re-voice and recolour the field.",
    "FREEZE holds the delay buffer; REV plays a tap backwards.",
    "Click the preset capsule to open the browser; A/B compares edits.",
};
} // namespace

juce::Rectangle<float> OrbitSettingsPanel::panelBounds() const {
    return { (float(theme::kWindowW) - kPanelW) / 2.0f, 70.0f, kPanelW, kPanelH };
}

void OrbitSettingsPanel::mouseUp(const juce::MouseEvent& e) {
    const auto pos = e.position;
    const auto panel = panelBounds();
    const bool onClose_ = !panel.contains(pos)
        || juce::Rectangle<float>(panel.getRight() - 38.0f, panel.getY() + 10.0f,
                                  22.0f, 22.0f).expanded(4.0f).contains(pos);
    if (onClose_ && onClose != nullptr)
        onClose();
}

void OrbitSettingsPanel::paint(juce::Graphics& g) {
    // Scrim + panel chrome — mirrors OrbitPresetBrowser.
    g.fillAll(juce::Colour { 0xff040408 }.withAlpha(0.45f));
    const auto panel = panelBounds();
    g.setColour(juce::Colour { 0xff0c0c13 });
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(theme::bone50.withAlpha(0.16f));
    g.drawRoundedRectangle(panel, 14.0f, 1.0f);

    const auto sectionHead = [&] (const char* title, float y) {
        g.setColour(theme::ember);
        g.fillRect(panel.getX() + 16.0f, y + 5.0f, 4.0f, 4.0f);
        g.setFont(fonts::tracked(fonts::monoSemiBold(9.0f), 0.26f));
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawText(title, juce::Rectangle<float>(panel.getX() + 27.0f, y, 200.0f, 14.0f),
                   juce::Justification::centredLeft, false);
    };

    // Close x.
    g.setColour(theme::bone50.withAlpha(0.14f));
    g.drawEllipse(panel.getRight() - 38.0f, panel.getY() + 10.0f, 22.0f, 22.0f, 1.0f);
    g.setColour(theme::bone50.withAlpha(0.6f));
    g.setFont(fonts::mono(11.0f));
    g.drawText(juce::String::fromUTF8("\xc3\x97"),
               juce::Rectangle<float>(panel.getRight() - 38.0f, panel.getY() + 10.0f,
                                      22.0f, 22.0f),
               juce::Justification::centred, false);

    // ABOUT.
    float y = panel.getY() + 12.0f;
    sectionHead("ABOUT", y);
    y += 26.0f;
    g.setFont(fonts::sansSemiBold(14.0f));
    g.setColour(theme::text());
    g.drawText("ORBIT - 4-tap echo",
               juce::Rectangle<float>(panel.getX() + 27.0f, y, 300.0f, 16.0f),
               juce::Justification::centredLeft, false);
    y += 20.0f;
    g.setFont(fonts::mono(9.5f));
    g.setColour(theme::bone50.withAlpha(0.55f));
    g.drawText(juce::String("Version ") + JucePlugin_VersionString
                   + "  -  Synthios Records",
               juce::Rectangle<float>(panel.getX() + 27.0f, y, 400.0f, 12.0f),
               juce::Justification::centredLeft, false);
    y += 30.0f;
    g.setColour(theme::bone50.withAlpha(0.07f));
    g.fillRect(panel.getX(), y, panel.getWidth(), 1.0f);
    y += 14.0f;

    // HOW TO USE.
    sectionHead("HOW TO USE", y);
    y += 28.0f;
    g.setFont(fonts::sans(12.0f));
    for (const auto* line : kHowTo) {
        g.setColour(theme::ember.withAlpha(0.8f));
        g.fillEllipse(panel.getX() + 29.0f, y + 5.0f, 3.0f, 3.0f);
        g.setColour(theme::bone50.withAlpha(0.78f));
        g.drawText(line, juce::Rectangle<float>(panel.getX() + 42.0f, y,
                                                panel.getWidth() - 70.0f, 14.0f),
                   juce::Justification::centredLeft, false);
        y += 26.0f;
    }
}
```

- [ ] **Step 4: Gear in the header**

`OrbitHeader.h` — next to `onBrowserToggle`:

```cpp
    std::function<void()> onSettingsToggle;
```

`OrbitHeader.cpp` — in `mouseUp` (line 114), extend:

```cpp
void OrbitHeader::mouseUp(const juce::MouseEvent& e) {
    if (presetCapsule_.contains(e.getPosition()) && onBrowserToggle != nullptr)
        onBrowserToggle();
    else if (gearBounds_.contains(e.getPosition()) && onSettingsToggle != nullptr)
        onSettingsToggle();
}
```

In `paint()`, after the A/B `slotRing` calls, draw the gear (paint-only, same idiom):

```cpp
    // Settings gear.
    {
        const auto c = gearBounds_.toFloat().getCentre();
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawEllipse(c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f, 1.4f);
        g.fillEllipse(c.x - 1.7f, c.y - 1.7f, 3.4f, 3.4f);
        for (int i = 0; i < 8; ++i) {
            const float a = juce::MathConstants<float>::pi * float(i) / 4.0f;
            juce::Path tooth;
            tooth.addRectangle(c.x - 1.1f, c.y - 9.0f, 2.2f, 3.2f);
            g.fillPath(tooth, juce::AffineTransform::rotation(a, c.x, c.y));
        }
    }
```

- [ ] **Step 5: Wire the editor**

`OrbitEditor.h` — add `#include "gui/OrbitSettingsPanel.h"`, a member `OrbitSettingsPanel settings_;` after `browser_`, and an accessor next to `presetBrowser()`:

```cpp
    OrbitSettingsPanel& settingsPanel() { return settings_; }
```

`OrbitEditor.cpp` constructor — after `addChildComponent(browser_);`:

```cpp
    addChildComponent(settings_);  // hidden until the gear opens it
```

Replace the `onBrowserToggle` lambda body and add the settings wiring (mutual exclusion both ways):

```cpp
    header_.onBrowserToggle = [this] {
        if (!browser_.isVisible())
            browser_.refresh();    // pick up user presets saved mid-session
        settings_.setVisible(false);
        browser_.setVisible(!browser_.isVisible());
        browser_.toFront(false);
    };
    browser_.onClose = [this] { browser_.setVisible(false); };
    header_.onSettingsToggle = [this] {
        browser_.setVisible(false);
        settings_.setVisible(!settings_.isVisible());
        settings_.toFront(false);
    };
    settings_.onClose = [this] { settings_.setVisible(false); };
```

In `resized()`, after the browser line:

```cpp
    placeScaled(settings_, 0, 0, theme::kWindowW, theme::kWindowH);
```

Wait — `onBrowserToggle` currently toggles: if settings was open and browser hidden, `setVisible(!isVisible())` opens the browser; correct. But when the browser is open and the user clicks the capsule again, `settings_.setVisible(false)` is a no-op and the browser closes; also correct.

`plugin/CMakeLists.txt` — in `target_sources(OrbitDelay PRIVATE ...)` add, next to `gui/OrbitPresetBrowser.cpp` (line 53):

```cmake
    gui/OrbitSettingsPanel.cpp
```

- [ ] **Step 6: Run to verify it passes**

Run: `cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests`
Expected: PASS — 3 new settings tests plus the whole suite.

- [ ] **Step 7: Commit**

```bash
git add plugin/gui/OrbitSettingsPanel.h plugin/gui/OrbitSettingsPanel.cpp \
        plugin/gui/OrbitHeader.h plugin/gui/OrbitHeader.cpp \
        plugin/gui/OrbitEditor.h plugin/gui/OrbitEditor.cpp \
        plugin/CMakeLists.txt tests/gui/CMakeLists.txt tests/gui/OrbitSettingsTests.cpp
git commit -m "feat(gui): settings gear + About / How to Use overlay

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: Design-contract update + full validation + reinstall

**Files:**
- Modify: `docs/design/README.md` (deviations list)
- No code changes.

**Interfaces:** none — documentation and verification.

- [ ] **Step 1: Record the deviations**

In `docs/design/README.md`, append to the "Deliberate deviations" bullet list:

```markdown
- **Window re-based to 900x550** (mockup remains 1180x720): 52px header,
  730x402 pad, 170px rail, 96px strip. True re-layout, not a uniform scale —
  type sizes kept, spacing tightened, rail sections distribute evenly.
- **Removed** the pad's SYN·FX·001 — STEREO MULTI-TAP ECHO sub-line and the
  rail's SYN·FX·001 / MK I footer stamp.
- **Added** (not in mockup): header settings gear opening an About / How to
  Use overlay (preset-browser idiom), and a PRESETS caption under the preset
  capsule.
```

- [ ] **Step 2: Full test sweep**

```bash
cmake --build build-release -j8 --target orbit_gui_tests orbit_tests orbit_preset_tests
./build-release/tests/gui/orbit_gui_tests
./build-release/tests/orbit_tests
./build-release/tests/preset/orbit_preset_tests
```

Expected: all PASS. (Test-tree paths: confirm binary locations with `find build-release -name "orbit_*tests" -type f` if they differ.)

- [ ] **Step 3: Rebuild + reinstall the plugin, validate**

```bash
cmake --build build-release --target OrbitDelay_VST3 OrbitDelay_AU -j8
lipo -archs ~/Library/Audio/Plug-Ins/VST3/Orbit.vst3/Contents/MacOS/Orbit   # arm64 + x86_64
killall -9 AudioComponentRegistrar 2>/dev/null
auval -v aufx Orb1 Orba | tail -5
```

Expected: `AU VALIDATION SUCCEEDED`, 34 Global Scope Parameters, universal binary.

- [ ] **Step 4: Commit docs + manual screenshot check**

```bash
git add docs/design/README.md
git commit -m "docs(design): record 900x550 re-base, text removals, settings overlay deviations

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Then ask Jose to reopen Orbit in FL Studio and confirm: single 900×550 window, gear opens settings, PRESETS caption visible, upright A/B, no label collisions, rail fills its column.

---

## Self-Review Notes

- **Spec coverage:** item 1 → Task 6; item 2 → Tasks 4+5; item 3 → Tasks 1+2 (header) with pad/strip deriving from constants (strip card width is `getWidth()`-driven at `OrbitTapStrip.cpp:266`); item 4 → Task 2; item 5 → Task 3; item 6a → Task 4, 6b → Task 2, 6c → Task 5; contract note → Task 7.
- **Types:** `sectionGap(float) -> float`; `scaleLabelVisible(Rect, Rect, Rect) -> bool`; `gearBounds() -> Rectangle<int>`; accessors `slotAButton()/slotBButton() -> TextButton&`; `settingsPanel() -> OrbitSettingsPanel&` — used consistently across tasks.
- **Known judgment call:** the spec's "header non-overlap at 900w and 1800w" collapses to one base-size test because children are laid out in design pixels and scaled by a single transform — scale cannot introduce overlap.
