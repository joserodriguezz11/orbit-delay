# Orbit Phase 3 — UI Polish 2 (fonts, knob indicator, glyph-only controls, watermark fit)

**Date:** 2026-07-14
**Status:** Approved by Jose (design conversation, 2026-07-14; "just work on it")
**Branch target:** continues `dev/phase-3-wave-1`

## Context

After the first refinement wave, a hands-on pass surfaced five more changes.
Decisions locked with Jose: Space Grotesk + Space Mono fonts; knob indicator
becomes a dot at the value position; A/B and preset arrows lose ALL chrome
(no rings, no red disc — active A/B is a red letter); the pad's ORBIT
watermark must never clip.

## Work Items

### 1. Font swap — Space Grotesk (sans) + Space Mono (mono)

- New TTFs are ALREADY in `assets/fonts/` (controller downloaded, verified
  TrueType): `SpaceGrotesk-Medium.ttf`, `SpaceGrotesk-Bold.ttf`,
  `SpaceMono-Regular.ttf`, `SpaceMono-Bold.ttf`. All OFL.
- Delete the retired faces: `Archivo-*.ttf` (3 files), `IBMPlexMono-*.ttf`
  (4 files). `Syne-ExtraBold.ttf` STAYS (watermark).
- CMake globs `assets/fonts/*.ttf` into `OrbitFontData` (NAMESPACE
  `OrbitFontBinary`) — symbol names derive from filenames, no list edit
  needed, but the glob is configure-time: fresh-configure or touch the
  CMakeLists so the target re-globs.
- `plugin/gui/OrbitFonts.cpp` accessor mapping (API UNCHANGED — no call-site
  edits anywhere):
  - `mono`, `monoMedium` → `SpaceMonoRegular_ttf`
  - `monoSemiBold`, `monoBold` → `SpaceMonoBold_ttf`
  - `sans` → `SpaceGroteskMedium_ttf`
  - `sansSemiBold`, `sansExtraBold` → `SpaceGroteskBold_ttf`
  (Space Grotesk has no static SemiBold/ExtraBold; Space Mono ships only
  400/700 — the doubled mappings are deliberate.)
  - `watermark` → unchanged (Syne).
- `plugin/gui/OrbitTheme.h`: `fontSans = "Space Grotesk"`,
  `fontMono = "Space Mono"` (fontWatermark unchanged).
- `plugin/gui/OrbitLookAndFeel.cpp`: replace the name-based `monoFont()`/
  `sansFont()` helpers with the embedded accessors `fonts::mono()`/
  `fonts::sans()` (fixes latent system-fallback bug on machines without the
  fonts installed). Add the `OrbitFonts.h` include.
- Tests: `OrbitFontsTests.cpp` family assertions → "Space Mono" /
  "Space Grotesk" (watermark stays "Syne"); `OrbitThemeTests.cpp` type-family
  test → new names. Update both files' header comments too.

### 2. Knob indicator — dot at arc end (replaces white arc + pointer)

Applies to BOTH knob painters so they don't diverge:
- `plugin/gui/OrbitControls.cpp` `OrbitKnob::paint`: DELETE the value-arc
  block and the pointer block. ADD a dot riding the track arc at angle `va`:
  centre = point on circumference at radius `r`, angle `rad(va)`; glow halo
  first (filled ellipse, diameter `~4.4x` dot radius, colour at alpha 0.22),
  then the dot (radius `arcW * 0.9` — ~3.2px on a 56px knob). Dot colour
  keeps the existing semantic: `linked_ ? accent_ : theme::manual()`.
  Track arc, ticks, cap, value text all stay.
- `plugin/gui/OrbitLookAndFeel.cpp` `drawRotarySlider`: same treatment —
  delete the value arc, add the same dot+halo at `toAngle` on radius `arcR`,
  colour `accent_`.
- Tests: `OrbitLookAndFeelTests.cpp` / `OrbitControlsTests.cpp` — update any
  assertion that references the removed arc/pointer; add/keep a smoke
  assertion that painting at 0%, 50%, 100% doesn't crash and the dot angle
  maths stays within the -135..135 sweep (pure-function test if one exists,
  else paint-smoke).

### 3. A/B + preset arrows — glyph-only

- `plugin/gui/OrbitHeader.cpp` `slotRing` lambda: DELETE the filled ellipse
  (active) and the outline ellipse (inactive). Letters only:
  active → `theme::ember`, inactive → `theme::bone50.withAlpha(0.5f)`.
  Font stays `monoSemiBold(9.5f)`.
- Prev/next ‹ › buttons: no LookAndFeel chrome. Clear their button text
  (empty `juce::String()` ctor args, same pattern as A/B), keep them as hit
  areas, and draw the ‹ › glyphs in `paint()` at the buttons' bounds
  (`fonts::mono(11.0f)`, `bone50 @ 0.6`, centred). This kills the default
  LookAndFeel outline for good.
- Existing empty-text A/B test still passes; extend it (or add a sibling)
  asserting prev/next button text is also empty.

### 4. Watermark auto-fit — "ORBIT" never clips

- `plugin/gui/OrbPad.cpp` watermark block: measure the string width at the
  190px face (`juce::GlyphArrangement`/`Font::getStringWidthFloat`); if it
  exceeds `W - 40.0f`, scale the font height by `(W - 40) / measuredWidth`.
  Draw centred at the scaled height (vertical rect recomputed from the
  actual height). Full word always visible at every pad size ≥ base.
- Test: pure helper `orbpad::watermarkHeight(float baseHeight, float
  stringWidthAtBase, float availableWidth)` returning the fitted height —
  clamped so it never exceeds baseHeight; unit-test fit and no-fit cases.

## Non-Goals

- No mockup HTML edits; no parameter/DSP/preset/state changes; no layout
  metric changes (window stays 900x550).
- Syne watermark face unchanged.

## Design-Contract Update

Append to `docs/design/README.md` deviations: type families are Space
Grotesk / Space Mono (mockup: Archivo / IBM Plex Mono); knob indicator is a
dot-on-track (mockup: value arc + pointer); A/B and preset arrows are
glyph-only (mockup: ringed circles); watermark auto-fits the pad width.
Also update the Fonts section (embedded faces list).

## Testing

- Full `orbit_gui_tests` green; `orbit_tests` + `orbit_preset_tests`
  untouched but re-run in final validation; auval SUCCEEDED, universal
  binaries reinstalled.
- Manual: Jose eyeballs in FL Studio (fonts, dots, glyph-only header,
  full ORBIT watermark).
