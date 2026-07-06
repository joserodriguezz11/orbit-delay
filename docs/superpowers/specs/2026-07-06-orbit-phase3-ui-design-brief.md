# Orbit — Phase 3 UI Design Brief (for Claude Design)

> Paste this whole document into Claude Design as the grounding brief.
> Everything here is extracted from the live engine code on `main`
> (commit `d80aa4e`) — every control listed maps 1:1 to a real, automatable
> plugin parameter. Design anything you want visually; don't invent or
> drop controls.

## 1. What Orbit is

A creative delay plugin. The signature interaction: **echoes are glowing
orbs you drag on a 2D pad** — horizontal = delay time, vertical = feedback.
Up to 4 taps (orbs). Character modes recolor the sound (and may recolor the
UI). One screen, no menu-diving, genre-agnostic.

Suggested accent hue per character mode (design may override):
**cyan = Clean · amber = Tape · magenta = Grit**.

## 2. Hard constraints

- **Single window, resizable 100–200%.** Design at a base size; everything
  scales proportionally.
- **Every parameter in §3 must be reachable** in the UI (automation +
  accessibility requirement). Grouping/prioritizing visually is fine;
  hiding is not.
- Target implementation is JUCE (C++). Favor: solid shapes, gradients,
  glows, simple particle/trail effects, vector-style controls. Avoid:
  photorealistic skeuomorphism, video textures, complex 3D.

## 3. Controls the UI must expose

### Per-tap (× 4 taps — the orbs)

| Control | Type | Range / choices | Default |
|---|---|---|---|
| Enabled | toggle | on/off | tap 1 on, others off |
| Time | continuous | 1–2000 ms, log-ish skew (350 ms center-weighted) | 350 ms |
| Sync | choice | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/4 D, 1/8 D, 1/4 T, 1/8 T | Free |
| Feedback | continuous | 0–98% | 35% |
| Reverse | toggle | on/off | off |
| Pitch | stepped | −12…+12 semitones (snapped) | 0 |

Pad mapping: **Time = X axis, Feedback = Y axis** (drag the orb).
Reverse/Pitch/Sync per tap need a compact per-orb affordance (e.g. halo
segments, orbit ring, small satellite controls, or a tap inspector strip).
When Sync ≠ Free, the tap's time is tempo-locked — the orb's X position
should reflect/snap to the synced value.

### Global

| Control | Type | Range / choices | Default |
|---|---|---|---|
| Mix (dry/wet) | continuous | 0–100% | 30% |
| Ducking | continuous | 0–100% | 50% |
| Width | continuous | 0–200% | 100% |
| Ping-Pong | toggle | on/off | off |
| Character | choice | Clean / Tape / Grit | Clean |
| Motion Depth | continuous | 0–100% | 0% |
| Motion Rate | continuous | 0.1–8 Hz (log-ish skew) | 0.5 Hz |
| Low Cut | continuous | 0–2000 Hz (log-ish skew) | 0 (off) |
| High Cut | continuous | 200–20000 Hz (log-ish skew) | 20000 (off) |
| Freeze | toggle (momentary-feel) | on/off | off |

Freeze is a performance gesture — make it big, satisfying, and visually
transformative (the whole pad can respond).

## 4. Live animation data (what the engine feeds the UI)

The engine publishes a real-time visualization feed the UI polls every
frame. Available signals:

- **Per-tap repeat-fire events** — each audible echo emits an event with
  the tap index, a sample-accurate timestamp, and an intensity value
  (0–1, loudness of that echo). Use for orb pulses, ripples, trails.
  The feed also publishes the engine's current sample clock, so the UI can
  compute each event's age for decay animations.
- **Input and output levels** — RMS + peak, both channels. Use for
  ambient meters or background energy.
- **Ducker gain reduction** — how hard the ducker is pressing the echoes
  down right now. Great for a "dry signal pushes the orbs back" visual.

Design freely against these — they exist and are tested.

## 5. Preset browser

- Factory set: **40 presets**, each tagged from a fixed vocabulary:
  **Vocals · Drums · Ambient · Dub · Lo-fi · Utility** — these six are the
  filter buttons.
- User presets: save / rename / delete, name field, tag picker (same six
  tags).
- **A/B compare** with copy A→B / B→A.
- Dirty-state indicator (preset modified since load).
- Browser must not dominate the one-screen layout — collapsed strip or
  overlay panel recommended.

## 6. Deliverables from the design side

1. Full-window layout at base size (and how it breathes at 200%).
2. Orb/pad interaction states: idle, hover, drag, disabled tap, synced tap,
   reverse on, pitched, freeze active.
3. The three character-mode looks (Clean/Tape/Grit).
4. Preset browser open/closed states.
5. Color/typography/spacing tokens the JUCE build can consume as constants.

Static mockups (HTML/PNG) are enough — animation described in words per
state is fine; the fire-event/level/duck signals in §4 tell you what can
drive motion.
