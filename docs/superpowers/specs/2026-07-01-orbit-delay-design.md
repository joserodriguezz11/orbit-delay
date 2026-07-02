å# Orbit — Creative Delay Plugin — Design Spec

**Date:** 2026-07-01
**Status:** Approved by Jose (2026-07-01)
**Working name:** Orbit (rename anytime)

---

## 1. Product Summary

A creative delay plugin where echoes are **glowing orbs the user drags on a 2D pad** (horizontal = delay time, vertical = feedback/decay). Around that core: character modes, reverse and pitch-shifted repeats, freeze, modulation, and always-on smart ducking — so it sounds huge but never buries the dry signal. Genre-agnostic, one-screen UI, dark cosmic aesthetic.

**Goal:** commercial release (sellable product).
**Formats:** VST3 + AU + AAX, macOS + Windows.
**AAX caveat:** gated on Avid developer agreement + PACE/iLok signing — VST3/AU ship first; AAX slots in when Avid approves. Windows builds require a real machine or VM for release testing.

---

## 2. Scope Division (important)

- **Jose owns the UI visual design**, produced with Claude Design. This project does **not** design or style the UI.
- **This project builds everything else:** DSP engine, parameter/state layer, plugin shell (all formats), preset system, UI integration contract, tests, and commercial packaging.
- UI *implementation* in JUCE happens after Jose delivers the design, binding his visuals to the contract defined in §6.

---

## 3. Technology

- **Framework:** JUCE (C++17 or later). Chosen over iPlug2 (smaller ecosystem) and nih-plug/Rust (no AAX support). JUCE is the only mainstream framework with first-class AAX, and its license is free below $50k/yr revenue (with splash screen).
- **Build system:** CMake (JUCE's modern recommended path) — reproducible builds, CI-friendly.
- **Repo:** standalone repository (this one), independent of any other workspace.

---

## 4. Architecture

Orbit is plugin #1 of a planned **product family** (brand name TBD). Everything reusable lives in a `core/` library, built as its own CMake target from day one, so future plugins (reverb, saturator, …) consume it rather than copy it. When plugin #2 starts, `core/` graduates to its own repo; until then it stays here to avoid multi-repo overhead.

```
orbit-delay/
├── core/         # Reusable product-family library (own CMake target, no Orbit-specific code)
│   ├── dsp/               # Shared DSP building blocks — real-time safe, no UI deps
│   │   ├── DelayLine, Ducker, PitchShifter, ModEngine (LFOs),
│   │   ├── Saturation/CharacterStage (tape/BBD/clean — reusable coloring stage)
│   │   └── FreezeBuffer
│   ├── state/             # Versioned preset/state serialization (see below)
│   ├── licensing/         # Product-agnostic serial-key validation (productId field)
│   └── uibridge/          # Lock-free FIFO visualization feed, APVTS attachment helpers
├── plugin/       # Orbit-specific: MultiTapDelay topology, AudioProcessor shell, parameters
├── ui/           # UI scaffolding + integration contract (visuals from Jose's design)
├── presets/      # ~40 factory presets
├── tests/        # Unit tests (core + plugin) + pluginval harness
└── packaging/    # Installer scripts, signing, notarization
```

- **Core DSP** — real-time safe: no allocations, locks, or logging on the audio thread; denormal protection throughout. Orbit-specific wiring (the 4-tap topology) lives in `plugin/`, not `core/`.
- **Parameter/state layer** — JUCE `AudioProcessorValueTreeState` (APVTS). Every control, including each orb's X/Y, is a host-automatable parameter. Handles preset save/load and DAW session recall.
- **UI layer** — reads audio levels/tap activity via a lock-free FIFO for reactive visuals. Never touches the audio thread directly.

### Product-family readiness rules

- **Versioned, namespaced state:** every preset/session blob carries `{ product: "orbit", stateVersion: N }`; loaders migrate old versions forward. Future plugins share the same envelope format, enabling a family-wide preset browser later.
- **Brand/name in one place:** company name, product name, bundle IDs, and manufacturer codes are CMake variables in a single `Branding.cmake` — the TBD brand (and even the Orbit name) is a one-file change.
- **No cross-plugin runtime coupling in v1:** family integration means shared code and formats, not plugins talking to each other in-session. That keeps YAGNI honest; a message bus can be added to `core/` later without breaking anything.

---

## 5. Feature Spec (v1 — full creative suite)

### Echo Pad (core)
- Up to **4 draggable orbs** (delay taps). X-axis = time, Y-axis = feedback.
- Time: free (ms) or tempo-synced divisions including dotted and triplet.
- Per-orb: **Reverse** toggle, **Pitch** (±12 semitones applied to repeats — shimmer/octave effects), mute.

### Character modes (global)
- **Clean** — pristine digital.
- **Tape** — wow/flutter, saturation, high-end rolloff.
- **Grit** — BBD-style darkening + noise floor.

### Creative controls
- **Freeze** — big button; infinitely loops the current delay buffer.
- **Motion** — Modulation depth + rate knobs (subtle chorus → heavy wobble).

### Mix section
- Dry/Wet, stereo **Width**, **Ping-Pong** toggle.
- **Ducking** amount — sidechained from the dry signal, on by default at a musical setting.

### Filters on repeats
- Low-cut / high-cut, exposed as handles on the pad edges (per the UI contract).

### Presets
- ~40 factory presets tagged by use case (Vocals, Drums, Ambient, Dub, Lo-fi, …).
- A/B compare. Preset browser (UI per Jose's design; data layer here).

---

## 6. UI Integration Contract

What Jose's Claude Design UI can rely on from the engine:

**Parameters (bind via APVTS attachments):**
- `tap[1-4].time`, `tap[1-4].sync`, `tap[1-4].feedback`, `tap[1-4].reverse`, `tap[1-4].pitch`, `tap[1-4].enabled`
- `character.mode` (Clean/Tape/Grit)
- `mod.depth`, `mod.rate`
- `mix.dryWet`, `mix.width`, `mix.pingPong`, `mix.duckAmount`
- `filter.lowCut`, `filter.highCut`
- `freeze.active`

**Visualization feed (lock-free FIFO, polled at UI frame rate):**
- Per-tap repeat-fire events (for orb pulse/trail animations)
- Input/output RMS + peak levels
- Ducker gain-reduction amount

**UI constraints the design should honor:**
- Single window, resizable 100–200%.
- Every parameter above must be reachable (automation and accessibility).
- Suggested accent-hue-per-mode: cyan = Clean, amber = Tape, magenta = Grit (design may override).

---

## 7. Commercial Packaging

- **Licensing:** simple serial-key validation (offline-friendly, no iLok for VST3/AU). Keys embed a `productId` so the same scheme covers the whole future product family. Key generator kept private, outside this repo.
- **Installers:** signed + notarized `.pkg` (macOS, requires Apple Developer ID), signed `.exe` (Windows, requires code-signing cert).
- **AAX:** same codebase, additional build target once Avid agreement + PACE signing are in place.

---

## 8. Testing

- **Unit tests** on DSP: delay timing accuracy, feedback stability (no runaway), denormal safety, freeze correctness, pitch-shift quality bounds.
- **pluginval** at max strictness on VST3 + AU builds.
- **auval** pass for AU.
- **Manual DAW matrix** before release: Ableton Live, Logic, FL Studio, Reaper, Cubase, Pro Tools (post-AAX).

---

## 9. Build Order

1. Repo scaffolding: CMake + JUCE skeleton plugin that passes audio and loads in a DAW (`core/` as separate target, `Branding.cmake` from the start)
2. Core multi-tap delay + tempo sync + ducking
3. Parameter layer (APVTS) + UI integration contract surfaces
4. Character modes (Clean/Tape/Grit)
5. Reverse, pitch-shifted repeats, freeze
6. Modulation + repeat filters
7. Preset system + ~40 factory presets
8. UI implementation (after Jose delivers the Claude Design visuals)
9. Installers + code signing
10. AAX target (when Avid approves)

> Expectation: this is a multi-month project. Real-time C++ DSP is the deep end; stages ship incrementally and each is independently testable.
