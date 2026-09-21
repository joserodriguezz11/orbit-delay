# Orbit Phase 3 — UI Polish 2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Swap the UI fonts to Space Grotesk + Space Mono, replace the knob arc+pointer with a dot indicator, strip A/B and preset arrows to glyphs, and auto-fit the ORBIT watermark.

**Architecture:** GUI-only, branch `dev/phase-3-wave-1`, spec `docs/superpowers/specs/2026-07-14-orbit-phase3-ui-polish2-design.md`. The `fonts::` accessor API is stable — the swap happens inside `OrbitFonts.cpp` + binary assets. New font TTFs are already in `assets/fonts/`.

**Tech Stack:** JUCE 8 (C++20), Catch2 (`orbit_gui_tests`), CMake tree `build-release`.

## Global Constraints

- GUI-only: no parameter, DSP, preset-format, state, or layout-metric changes.
- `fonts::` accessor signatures unchanged; `watermark()` stays Syne.
- Design deviations recorded in `docs/design/README.md` (Task 4).
- Conventional commits on `dev/phase-3-wave-1`; do not push.
- Test command: `cmake --build ~/orbit-delay/build-release --target orbit_gui_tests -j8 && ~/orbit-delay/build-release/tests/gui/orbit_gui_tests` (run from `~/orbit-delay`).
- Every commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

---

### Task 1: Font swap

**Files:**
- Delete: `assets/fonts/Archivo-Medium.ttf`, `assets/fonts/Archivo-SemiBold.ttf`, `assets/fonts/Archivo-ExtraBold.ttf`, `assets/fonts/IBMPlexMono-Regular.ttf`, `assets/fonts/IBMPlexMono-Medium.ttf`, `assets/fonts/IBMPlexMono-SemiBold.ttf`, `assets/fonts/IBMPlexMono-Bold.ttf`
- Keep (already present, untracked — `git add` them): `assets/fonts/SpaceGrotesk-Medium.ttf`, `assets/fonts/SpaceGrotesk-Bold.ttf`, `assets/fonts/SpaceMono-Regular.ttf`, `assets/fonts/SpaceMono-Bold.ttf`
- Modify: `plugin/gui/OrbitFonts.cpp`, `plugin/gui/OrbitFonts.h` (comment), `plugin/gui/OrbitTheme.h:145-147`, `plugin/gui/OrbitLookAndFeel.cpp:6-18`
- Test: `tests/gui/OrbitFontsTests.cpp`, `tests/gui/OrbitThemeTests.cpp`

**Interfaces:**
- Consumes: `OrbitFontBinary::` symbols generated from the TTF filenames by the CMake glob (`SpaceGroteskMedium_ttf`, `SpaceGroteskBold_ttf`, `SpaceMonoRegular_ttf`, `SpaceMonoBold_ttf` + `_ttfSize` counterparts).
- Produces: unchanged `fonts::` API resolving to the new families; `theme::fontSans == "Space Grotesk"`, `theme::fontMono == "Space Mono"`.

- [ ] **Step 1: Update the failing tests first**

`tests/gui/OrbitFontsTests.cpp` — replace the family test body:

```cpp
TEST_CASE("embedded fonts resolve to the design families") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    using namespace orbit::gui;

    CHECK(fonts::mono(12.0f).getTypefaceName().contains("Space Mono"));
    CHECK(fonts::monoSemiBold(12.0f).getTypefaceName().contains("Space Mono"));
    CHECK(fonts::sans(12.0f).getTypefaceName().contains("Space Grotesk"));
    CHECK(fonts::sansSemiBold(12.0f).getTypefaceName().contains("Space Grotesk"));
    CHECK(fonts::watermark(190.0f).getTypefaceName().contains("Syne"));
}
```

Update the file's header comment (line 2-3) to cite Space Mono / Space Grotesk.

`tests/gui/OrbitThemeTests.cpp` — replace the type-family test:

```cpp
TEST_CASE("type families are Space Grotesk / Space Mono / Syne watermark") {
    CHECK(juce::String(theme::fontSans) == "Space Grotesk");
    CHECK(juce::String(theme::fontMono) == "Space Mono");
}
```

- [ ] **Step 2: Run to verify failure**

Run the test command. Expected: FAIL (families still Archivo / IBM Plex Mono).

- [ ] **Step 3: Swap the assets and mapping**

```bash
git rm assets/fonts/Archivo-Medium.ttf assets/fonts/Archivo-SemiBold.ttf \
       assets/fonts/Archivo-ExtraBold.ttf assets/fonts/IBMPlexMono-Regular.ttf \
       assets/fonts/IBMPlexMono-Medium.ttf assets/fonts/IBMPlexMono-SemiBold.ttf \
       assets/fonts/IBMPlexMono-Bold.ttf
git add assets/fonts/SpaceGrotesk-Medium.ttf assets/fonts/SpaceGrotesk-Bold.ttf \
        assets/fonts/SpaceMono-Regular.ttf assets/fonts/SpaceMono-Bold.ttf
```

`plugin/gui/OrbitFonts.cpp` — replace the accessor bodies (mapping per spec; Space Mono ships only 400/700 and Space Grotesk statics only Medium/Bold, so the doubled mappings are deliberate):

```cpp
juce::Font mono(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoRegular_ttf,
                      OrbitFontBinary::SpaceMonoRegular_ttfSize, height);
}
juce::Font monoMedium(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoRegular_ttf,
                      OrbitFontBinary::SpaceMonoRegular_ttfSize, height);
}
juce::Font monoSemiBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoBold_ttf,
                      OrbitFontBinary::SpaceMonoBold_ttfSize, height);
}
juce::Font monoBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoBold_ttf,
                      OrbitFontBinary::SpaceMonoBold_ttfSize, height);
}
juce::Font sans(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskMedium_ttf,
                      OrbitFontBinary::SpaceGroteskMedium_ttfSize, height);
}
juce::Font sansSemiBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskBold_ttf,
                      OrbitFontBinary::SpaceGroteskBold_ttfSize, height);
}
juce::Font sansExtraBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskBold_ttf,
                      OrbitFontBinary::SpaceGroteskBold_ttfSize, height);
}
```

(`watermark()` and `tracked()` unchanged.) If the generated symbol names differ (check `build-release/juce_binarydata_OrbitFontData/JuceLibraryCode/OrbitFontBinary.h` after a configure), use the actual generated names — they derive from the filenames.

Update the header comment in `plugin/gui/OrbitFonts.h` (lines 5-8) and the per-accessor weight comments (lines 11-17) to the new faces/mapping.

`plugin/gui/OrbitTheme.h` typography block:

```cpp
inline constexpr auto fontSans      = "Space Grotesk";   // labels, preset names
inline constexpr auto fontMono      = "Space Mono";      // values, section heads
inline constexpr auto fontWatermark = "Syne";            // pad ORBIT watermark
```

`plugin/gui/OrbitLookAndFeel.cpp` — delete the anonymous-namespace `monoFont()`/`sansFont()` helpers; add `#include "OrbitFonts.h"` and `namespace fonts = orbit::gui::fonts;`; replace the two call sites: `monoFont(fontH)` → `fonts::mono(fontH)` (drawRotarySlider) and `sansFont(13.0f)` → `fonts::sans(13.0f)` (drawToggleButton).

- [ ] **Step 4: Reconfigure (glob is configure-time), build, run**

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-release --target orbit_gui_tests -j8 && ./build-release/tests/gui/orbit_gui_tests
```

Expected: PASS. If the build fails on missing `OrbitFontBinary::Archivo*` symbols elsewhere, grep for stragglers: `grep -rn "Archivo\|IBMPlexMono" plugin/ tests/ --include=*.cpp --include=*.h`.

- [ ] **Step 5: Commit**

```bash
git add -A assets/fonts plugin/gui/OrbitFonts.cpp plugin/gui/OrbitFonts.h \
        plugin/gui/OrbitTheme.h plugin/gui/OrbitLookAndFeel.cpp \
        tests/gui/OrbitFontsTests.cpp tests/gui/OrbitThemeTests.cpp
git commit -m "feat(gui): swap type to Space Grotesk + Space Mono (embedded, OFL)

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: Knob dot indicator

**Files:**
- Modify: `plugin/gui/OrbitControls.cpp:143-168` (OrbitKnob::paint), `plugin/gui/OrbitLookAndFeel.cpp:26-66` (drawRotarySlider)
- Test: `tests/gui/OrbitControlsTests.cpp` (and `tests/gui/OrbitLookAndFeelTests.cpp` if it asserts on the removed arc)

**Interfaces:**
- Consumes: existing `linked_`, `accent_`, `theme::manual()`, `theme::track()`.
- Produces: no API change — paint-only.

- [ ] **Step 1: Check existing paint assertions**

`grep -n "arc\|pointer\|value" tests/gui/OrbitControlsTests.cpp tests/gui/OrbitLookAndFeelTests.cpp` — note any test asserting the value arc / pointer exists. Update those alongside Step 3 (paint smoke tests keep passing as-is; geometry-specific ones update to the dot).

- [ ] **Step 2: Implement — OrbitKnob::paint**

In `plugin/gui/OrbitControls.cpp`, DELETE the "Value arc" block (lines 152-160) and the "Pointer" block (lines 162-168). In their place:

```cpp
    // Value dot: rides the track arc at the value angle (halo underneath).
    {
        const auto dc = juce::Point<float>(cx, cy).getPointOnCircumference(r, rad(va));
        const float dotR = arcW * 0.9f;
        const auto col = linked_ ? accent_ : theme::manual();
        g.setColour(col.withAlpha(0.22f));
        g.fillEllipse(dc.x - dotR * 2.2f, dc.y - dotR * 2.2f, dotR * 4.4f, dotR * 4.4f);
        g.setColour(col);
        g.fillEllipse(dc.x - dotR, dc.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }
```

- [ ] **Step 3: Implement — OrbitLookAndFeel::drawRotarySlider**

In `plugin/gui/OrbitLookAndFeel.cpp`, DELETE the "Value arc" block (lines 49-58). In its place:

```cpp
    // Value dot on the track at the current angle (halo underneath).
    {
        const auto dc = centre.getPointOnCircumference(arcR, toAngle);
        const float dotR = lineW * 0.9f;
        g.setColour(accent_.withAlpha(0.22f));
        g.fillEllipse(dc.x - dotR * 2.2f, dc.y - dotR * 2.2f, dotR * 4.4f, dotR * 4.4f);
        g.setColour(accent_);
        g.fillEllipse(dc.x - dotR, dc.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }
```

(Note: `drawRotarySlider`'s angles are already radians from JUCE — use `toAngle` directly. JUCE's `getPointOnCircumference(radius, angle)` measures angle clockwise from 12 o'clock, which matches how `rotaryStartAngle`/`toAngle` are supplied by the slider and how `OrbitKnob` builds `va` off -135..135 degrees.)

- [ ] **Step 4: Build + run the suite; add a paint smoke case if none covers 0/50/100%**

If no existing test paints an OrbitKnob at extreme values, append to `tests/gui/OrbitControlsTests.cpp`:

```cpp
TEST_CASE("knob paints at value extremes without the removed arc/pointer") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitKnob k;
    k.setRange(0.0, 1.0, 0.01);
    k.setBounds(0, 0, 56, 56);
    juce::Image img { juce::Image::ARGB, 56, 56, true };
    for (const double v : { 0.0, 0.5, 1.0 }) {
        k.setValue(v, juce::dontSendNotification);
        juce::Graphics g { img };
        k.paint(g);   // must not assert/crash at either sweep extreme
    }
    SUCCEED();
}
```

Run the test command. Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitControls.cpp plugin/gui/OrbitLookAndFeel.cpp \
        tests/gui/OrbitControlsTests.cpp tests/gui/OrbitLookAndFeelTests.cpp
git commit -m "feat(gui): knob value dot replaces arc + pointer

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

(Omit the LookAndFeel test file from `git add` if untouched.)

---

### Task 3: A/B + arrows glyph-only, header/settings text removals

**Files:**
- Modify: `plugin/gui/OrbitHeader.h` (prev_/next_ ctor args), `plugin/gui/OrbitHeader.cpp` (slotRing, paint arrows, tagline removal), `plugin/gui/OrbitSettingsPanel.cpp` (ABOUT text)
- Test: `tests/gui/OrbitHeaderTests.cpp`

**Interfaces:**
- Consumes: Task 1's `fonts::` (mono).
- Produces: no API change; prev/next buttons carry empty text like A/B.

- [ ] **Step 1: Extend the failing test**

In `tests/gui/OrbitHeaderTests.cpp`, extend the existing A/B empty-text test (or add a sibling):

```cpp
TEST_CASE("preset arrows carry no LookAndFeel text (glyphs painted once)") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    CHECK(header.prevButton().getButtonText().isEmpty());
    CHECK(header.nextButton().getButtonText().isEmpty());
}
```

Add the accessors next to `slotAButton()`/`slotBButton()` in `OrbitHeader.h`:

```cpp
    juce::TextButton& prevButton() { return prev_; }
    juce::TextButton& nextButton() { return next_; }
```

- [ ] **Step 2: Run to verify failure** — compile error (accessors), then after adding accessors alone the CHECKs fail (text is "‹"/"›").

- [ ] **Step 3: Implement**

`OrbitHeader.h` — the button members become textless:

```cpp
    juce::TextButton prev_ { juce::String() }, next_ { juce::String() };
```

`OrbitHeader.cpp` `paint()`:

(a) Replace the whole `slotRing` lambda + calls with letters-only (no ellipses):

```cpp
    // A/B: letters only — active slot reads in ember, inactive dim bone.
    const auto slotLetter = [&] (const juce::TextButton& b, const juce::String& letter,
                                 bool active) {
        g.setColour(active ? theme::ember : theme::bone50.withAlpha(0.5f));
        g.setFont(fonts::monoSemiBold(9.5f));
        g.drawText(letter, b.getBounds().toFloat(), juce::Justification::centred, false);
    };
    slotLetter(slotA_, "A", !shownSlotB_);
    slotLetter(slotB_, "B", shownSlotB_);
```

(b) After that block, draw the arrow glyphs (buttons no longer render them):

```cpp
    // Preset arrows: glyph-only (buttons are bare hit areas).
    g.setColour(theme::bone50.withAlpha(0.6f));
    g.setFont(fonts::mono(11.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), prev_.getBounds().toFloat(),
               juce::Justification::centred, false);
    g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), next_.getBounds().toFloat(),
               juce::Justification::centred, false);
```

(c) In the constructor, `styleRound`'s text-colour line is now inert for all four buttons — leave the helper (it still kills the button background colour) but drop its `textColourOffId` line if trivially safe, else leave as-is.

(d) Text removals (same commit):
- In `OrbitHeader.cpp` `paint()`, DELETE the `4-TAP ECHO` tagline draw (the `fonts::tracked(fonts::mono(6.5f), 0.32f)` + `drawText("4-TAP ECHO", ...)` lines under the wordmark block). The wordmark, logo rings, and the PRESETS caption stay.
- In `plugin/gui/OrbitSettingsPanel.cpp` `paint()`: change `drawText("ORBIT - 4-tap echo", ...)` to `drawText("ORBIT", ...)`, and the version line from `juce::String("Version ") + JucePlugin_VersionString + "  -  Synthios Records"` to `juce::String("Version ") + JucePlugin_VersionString`.

- [ ] **Step 4: Build + run** — expected PASS, including the existing "header children do not overlap" and A/B behavior tests.

- [ ] **Step 5: Commit**

```bash
git add plugin/gui/OrbitHeader.h plugin/gui/OrbitHeader.cpp \
        plugin/gui/OrbitSettingsPanel.cpp tests/gui/OrbitHeaderTests.cpp
git commit -m "feat(gui): glyph-only A/B + arrows; drop 4-TAP ECHO tagline and settings byline

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: Watermark auto-fit + README + validation

**Files:**
- Modify: `plugin/gui/OrbPad.h` (orbpad namespace), `plugin/gui/OrbPad.cpp` (watermark block), `docs/design/README.md`
- Test: `tests/gui/OrbPadTests.cpp`

**Interfaces:**
- Produces: `float orbpad::watermarkHeight(float baseHeight, float stringWidthAtBase, float availableWidth)`.

- [ ] **Step 1: Write the failing test**

Append to `tests/gui/OrbPadTests.cpp`:

```cpp
TEST_CASE("watermark height scales down to fit the pad, never up") {
    // Wider than available: scales proportionally.
    CHECK(orbpad::watermarkHeight(190.0f, 900.0f, 690.0f)
          == Approx(190.0f * 690.0f / 900.0f));
    // Fits already: stays at base height.
    CHECK(orbpad::watermarkHeight(190.0f, 600.0f, 690.0f) == Approx(190.0f));
    // Degenerate width: clamps to a positive floor, no divide-by-zero.
    CHECK(orbpad::watermarkHeight(190.0f, 0.0f, 690.0f) == Approx(190.0f));
}
```

(`Approx` — add `#include <catch2/catch_approx.hpp>` + `using Catch::Approx;` if the file lacks it.)

- [ ] **Step 2: Run to verify failure** — compile error, `watermarkHeight` not declared.

- [ ] **Step 3: Implement**

`OrbPad.h`, in `namespace orbpad` after `scaleLabelVisible`:

```cpp
// Watermark fit: the height that renders a string measured at baseHeight
// into availableWidth — scales down proportionally, never up.
float watermarkHeight(float baseHeight, float stringWidthAtBase, float availableWidth);
```

`OrbPad.cpp`, next to the other orbpad free functions:

```cpp
float orbpad::watermarkHeight(float baseHeight, float stringWidthAtBase,
                              float availableWidth) {
    if (stringWidthAtBase <= availableWidth || stringWidthAtBase <= 0.0f)
        return baseHeight;
    return baseHeight * availableWidth / stringWidthAtBase;
}
```

Watermark block in `paint()` (currently fixed 190px) becomes:

```cpp
    // Watermark — fitted so ORBIT never clips at any pad width.
    {
        const float wmBase = 190.0f;
        const float wmW = fonts::watermark(wmBase).getStringWidthFloat("ORBIT");
        const float wmH = orbpad::watermarkHeight(wmBase, wmW, W - 40.0f);
        g.setFont(fonts::watermark(wmH));
        g.setColour(theme::bone50.withAlpha(0.032f));
        g.drawText("ORBIT",
                   juce::Rectangle<float>(0.0f, H / 2.0f + 8.0f - wmH / 2.0f, W, wmH),
                   juce::Justification::centred, false);
    }
```

- [ ] **Step 4: README deviations**

Append to the `docs/design/README.md` deviations list (match existing style):

```markdown
- **Type families are Space Grotesk / Space Mono** (mockup: Archivo /
  IBM Plex Mono; Syne watermark unchanged). Embedded weights: Grotesk
  Medium/Bold, Mono Regular/Bold — SemiBold/ExtraBold roles map to Bold.
- **Knob indicator is a dot on the track** at the value angle (mockup:
  value arc + pointer line).
- **A/B and preset arrows are glyph-only** — no rings or active disc;
  the active slot reads in ember.
- **Pad watermark auto-fits** the pad width (mockup: fixed 190px).
```

Also update the Fonts section's embedded-faces list (Archivo/IBM Plex Mono → Space Grotesk Medium/Bold + Space Mono Regular/Bold; Syne stays).

- [ ] **Step 5: Full validation + reinstall**

```bash
cmake --build build-release --target orbit_gui_tests orbit_tests orbit_preset_tests -j8
./build-release/tests/gui/orbit_gui_tests && ./build-release/tests/orbit_tests \
  && ./build-release/tests/preset/orbit_preset_tests
cmake --build build-release --target OrbitDelay_VST3 OrbitDelay_AU -j8
lipo -archs ~/Library/Audio/Plug-Ins/VST3/Orbit.vst3/Contents/MacOS/Orbit
killall -9 AudioComponentRegistrar 2>/dev/null; auval -v aufx Orb1 Orba | tail -3
```

Expected: all suites green, "x86_64 arm64", AU VALIDATION SUCCEEDED.

- [ ] **Step 6: Commit**

```bash
git add plugin/gui/OrbPad.h plugin/gui/OrbPad.cpp tests/gui/OrbPadTests.cpp docs/design/README.md
git commit -m "feat(gui): ORBIT watermark auto-fits pad width; record polish-2 deviations

Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

## Self-Review Notes

- Spec coverage: item 1 → Task 1; item 2 → Task 2; item 3 → Task 3; item 4 → Task 4; README → Task 4.
- Interfaces: `watermarkHeight` used consistently; `prevButton()/nextButton()` match test usage; `fonts::` API untouched.
- Judgment call: `getStringWidthFloat` is deprecated in newer JUCE in favour of `GlyphArrangement` measurement — if the build warns/errs, measure via `juce::GlyphArrangement ga; ga.addLineOfText(font, "ORBIT", 0, 0); ga.getBoundingBox(0, -1, true).getWidth()` with identical semantics.
