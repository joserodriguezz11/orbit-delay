# Orbit — Full v2 UI design (authoritative)

`orbit-delay-full-v2.html` is the approved Claude Design mockup the Phase-3
GUI is built from (imported 2026-07-11 from the "Delay Plugin UI Design"
project, file `Orbit Delay - Full v2.dc.html`). It supersedes the earlier
`docs/superpowers/specs/phase3-ui-design` mockup wherever they disagree —
notably: 1180x720 window, Archivo + IBM Plex Mono type, a single red accent
with **position-derived OKLCH tap colours** (no per-mode accent colours),
and per-character field themes (clean/tape/grit).

`tokens/` holds the Synthios Records design-system CSS the mockup links
against (colour/type/spacing/effects custom properties).

## Contract

`plugin/gui/OrbitTheme.h` ports the mockup's constants verbatim — layout
metrics, TH colour table, CHRS character themes, posLCH position→colour,
and the 40–900ms log time curve (`ms = 40·22.5^x`). Change the mockup first,
then the tokens, never the other way around.

Deliberate deviations from the mockup (all UI-side, engine-faithful):

- **Sync is a division choice**, not a boolean: the SYNC pill maps on/off to
  `tapN_sync` (nearest real TempoSync division at host tempo / Free), and the
  pad grid + stepped TIME knob use `divisionToSeconds` at the live BPM
  instead of the mockup's hard-coded 120bpm DIVS table.
- **Width** displays the engine's 0–2 range as 0–100% (engine default 1.0
  shows 50%, not the mockup's 70 default).
- **Real echo fires** (VizFeed TapFireEvents) pulse the matching orb's halo;
  the mockup only simulated meter motion.
- Tap time/feedback parameters accept host automation outside the pad's
  visual range (1–2000ms / 0–0.98); the pad clamps display at its edges.
- **Window re-based to 900x550** (mockup remains 1180x720): 52px header,
  730x402 pad, 170px rail, 96px strip. True re-layout, not a uniform scale —
  type sizes kept, spacing tightened, rail sections distribute evenly.
- **Removed** the pad's SYN·FX·001 — STEREO MULTI-TAP ECHO sub-line and the
  rail's SYN·FX·001 / MK I footer stamp.
- **Added** (not in mockup): header settings gear opening an About / How to
  Use overlay (preset-browser idiom), and a PRESETS caption under the preset
  capsule.
- **Type families are Space Grotesk / Space Mono** (mockup: Archivo /
  IBM Plex Mono; Syne watermark unchanged). Embedded weights: Grotesk
  Medium/Bold, Mono Regular/Bold — SemiBold/ExtraBold roles map to Bold.
- **Knob indicator is a dot on the track** at the value angle (mockup:
  value arc + pointer line).
- **A/B and preset arrows are glyph-only** — no rings or active disc; the
  active slot reads in ember.
- **Pad watermark auto-fits** the pad width (mockup: fixed 190px); the
  header drops the 4-TAP ECHO tagline and the settings ABOUT drops the
  "4-tap echo" / "Synthios Records" byline.
- **Preset browser panel is 860x420 with a four-column grid** (mockup:
  566px, two columns, which only fit 20 of the 40 factory presets). Rows
  are drawn and clickable only while they fit the panel; overflow (user
  presets past capacity) is labelled "+N MORE" instead of hidden.
- **Feedback axis reads in true percent (0–98)** — pad y, FEEDBACK knob
  and strip readout all span the parameter's full 0–0.98 range (mockup
  used a 95 full-scale). Riding-knob double-click resets to the parameter
  defaults: 35% feedback, 350ms time.
- **Wave-4 viz animation** (not in mockup — the mockup only simulated
  motion): output RMS breathes the orbit rings and tap halos; ducking
  strains the drawn orbs a few px away from pad centre and dims their
  glow (paint-only — crosshair, halos and hit-testing stay on parameter
  truth); freeze crossfades the whole field to a cold desaturated tint
  (hue → 250°, chroma × 0.25) with the frost rings fading in, and held
  fire pulses stop decaying while frozen.

## Fonts

`assets/fonts/` embeds Space Grotesk (500/700), Space Mono (400/700) and
Syne (800) — all OFL-licensed — via the `OrbitFontData` binary target
(`OrbitFontBinary` namespace; the preset data owns plain `BinaryData`).
The `fonts::` SemiBold/ExtraBold accessors intentionally resolve to the
Bold faces (no static SemiBold exists for either family).
