# Orbit Phase 3 — UI Refinements (settings menu, 900×550 re-layout, cleanup)

**Date:** 2026-07-14
**Status:** Approved by Jose (design conversation, 2026-07-14)
**Branch target:** continues `dev/phase-3-wave-1`

## Context

First hands-on DAW test of the Phase-3 GUI (FL Studio 2025, after the
Release/universal rebuild) surfaced seven change requests. The approved Claude
Design mockup (`docs/design/orbit-delay-full-v2.html`) remains authoritative
for look and idiom; this spec adds documented deviations (base size, removed
text) to the existing deviation list in `docs/design/README.md`.

A screenshot-driven review mapped the requests to six work items below.

## Work Items

### 1. Settings menu — header gear + overlay

- New gear button in the header, placed in the gap between FREEZE and the
  meters block. Paint-only (see item 5's idiom), `bone50 @ 0.6` idle, `text()`
  on hover.
- Clicking opens `OrbitSettingsPanel`, an overlay child of the editor in the
  exact idiom of `OrbitPresetBrowser`: full-window scrim (`0xff040408 @ 0.45`),
  rounded panel (`0xff0c0c13`, 14px radius, `bone50 @ 0.16` border), ember
  dot + tracked mono section heads, × close button, click-outside closes.
- Panel content, static text, no scrolling, two stacked sections:
  - **ABOUT** — ORBIT wordmark row, version string from
    `JucePlugin_VersionString`, one line: "A 4-tap echo by Synthios Records."
  - **HOW TO USE** — short lines (≤8): drag an orb to set its time (x) and
    feedback (y); flick to throw; riding TIME/FEEDBACK knobs edit the selected
    tap; SYNC pill locks a tap to tempo divisions; CLEAN/TAPE/GRIT recolour and
    re-voice the field; FREEZE holds the buffer; preset capsule opens the
    browser; A/B compares two edit states.
- Editor wiring mirrors the browser: `addChildComponent`, `toFront`, mutual
  exclusion with the preset browser (opening one closes the other).

### 2. Text removal

- Delete the pad's centered `SYN-FX-001 — STEREO MULTI-TAP ECHO` line.
- Delete the rail's bottom `SYN-FX-001` / `MK I` stamp (and its ring glyph).
- Keep: `ORBIT` pad watermark, `DRAG ORB · FLICK TO THROW` hint, `4-TAP ECHO`
  header tagline.

### 3. Window base 900×550 (true re-layout)

- Theme constants: `kWindowW 900`, `kWindowH 550`, `kHeaderH 52` (unchanged),
  `kTapStripH 96` (unchanged), `kPadH = 550-52-96 = 402`. Rail width: 170
  (`kPadW = 730`). The four constants must still partition the window exactly
  (existing `OrbitThemeTests` assertion updated).
- Resize limits: 900×550 → 1800×1100, fixed aspect (still 100–200%).
- Header re-spacing at 900w, left→right: logo block, preset capsule (width may
  shrink to ~220), A/B pair, CLEAN/TAPE/GRIT segment, FREEZE, gear, divider,
  IN/OUT meters. No element may overlap; positions derive from `kWindowW`.
- Rail internal layout compresses to 402px pad height (see item 6c).
- Tap strip: 4 cards at ~215px each; trim card padding so number + ms + FB,
  SYNC/REV pills, and PITCH knob fit without collision. If PITCH must move
  closer to the pills, keep ≥6px clearance.
- OrbPad metrics (scale rows, watermark size, riding-knob rails) derive from
  pad size already; verify at 402 height that the 75/50/25 row spacing and
  bottom ms row remain legible.
- Update `tests/gui` resize-clamp expectations to the new limits (derive from
  theme constants, not literals, if not already).

### 4. "PRESETS" caption

- Tracked mono caption (`6.5px`, `0.2–0.32` tracking, `bone50 @ 0.42`) reading
  `PRESETS`, centered under the preset capsule — same idiom as the `4-TAP
  ECHO` tagline under the wordmark.

### 5. A/B double-draw fix

- Root cause: `slotA_`/`slotB_` are `juce::TextButton`s whose LookAndFeel
  paints their text, and `OrbitHeader::paint`'s `slotRing` lambda draws the
  letter again — two offset renders.
- Fix: draw each letter exactly once, upright, centered — clear the buttons'
  text (empty `setButtonText`, letter kept in a member) and keep `slotRing`
  as the single painter. ‹ › prev/next are single-drawn by the button today
  and stay as they are.
- Applies the same "paint-only control" idiom the new gear button uses.

### 6. Overlap cleanup

- (a) **Scale labels vs riding knobs:** OrbPad skips drawing any axis scale
  label (left 75/50/25 column, bottom ms row) whose bounds intersect the
  current bounds of a riding knob; the label returns when the knob moves off.
- (b) **Meters block:** re-space so the IN/OUT labels sit fully below the
  meter bars with ≥2px clearance at 100% scale, inside the header at 900w.
- (c) **Rail dead zone:** rail computes section layout from its actual height
  — BLEND/SPACE/MOTION/FILTER distribute evenly over the full column (equal
  inter-section gaps), no fixed trailing gap. Removing the MK I stamp (item 2)
  frees the bottom of the column.

## Non-Goals

- No mockup HTML edits (it stays 1180×720; deviations documented instead).
- No parameter, DSP, preset-format, or state changes. GUI-only.
- No new fonts or binary assets.

## Design-Contract Update

Append to `docs/design/README.md` deviations: base window 900×550 (re-layout,
mockup remains 1180×720), removed SYN-FX-001 pad line and rail stamp, added
gear/settings overlay and PRESETS caption (not present in mockup).

## Testing

- Existing: theme partition assert, resize-clamp bounds — updated to new
  constants; full `orbit_gui_tests`, `orbit_tests`, `orbit_preset_tests` stay
  green; auval + pluginval SUCCESS.
- New: settings panel opens/closes (gear click, ×, outside click, mutual
  exclusion with browser); A/B letters drawn once (paint coverage or state
  test on button text being empty); scale-label suppression predicate (pure
  function: label rect × knob rect → visible flag); rail even-distribution
  layout function; header layout non-overlap assertion at 900w and 1800w
  (pairwise `getBounds().intersects` over header children == false).
- Manual: screenshot pass in FL Studio at 100% and ~150% scale.
