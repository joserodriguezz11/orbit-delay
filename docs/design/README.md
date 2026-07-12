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

## Fonts

`assets/fonts/` embeds Archivo (500/600/800), IBM Plex Mono (400/500/600/700)
and Syne (800) — all OFL-licensed — via the `OrbitFontData` binary target
(`OrbitFontBinary` namespace; the preset data owns plain `BinaryData`).
