# Orbit Visualization Feed — Design (Phase 2 Wave 3)

Approved by Jose 2026-07-04. Scope: the engine-side visualization feed from the
master design spec §6 (UI Integration Contract), and nothing else. Presets and
the Wave 2 quality backlog are explicitly deferred to later waves.

## 1. Purpose

Give the Phase 3 UI a real-time, lock-free data feed for the Echo Pad
animations: per-tap repeat-fire events (orb pulse/trails), input/output
RMS + peak (meters), and ducker gain reduction. Prove it works this wave via
unit tests plus a JUCE-free terminal demo — no plugin UI code.

## 2. Architecture

New JUCE-free module `core/viz/`. `OrbitEngine` owns one `viz::VizFeed` and
fills it during `process()`; consumers (Phase 3 UI, the demo, tests) read it
from another thread. Two transport primitives matched to consumption:

- **Events (discrete):** single-producer/single-consumer lock-free ring
  buffer, capacity 256 events, storage allocated in `prepare()`. Event =
  `{ uint32 tapIndex; uint64 timeSamples; float intensity01; }`.
  `timeSamples` is an absolute engine sample counter (starts at 0 in
  `prepare()`, advances every processed sample). When full, the newest event
  is dropped silently (producer never waits; animation data is disposable).
- **Levels (continuous):** seqlock snapshot updated once per block by the
  producer. Fields: `inRms, inPeak, outRms, outPeak, duckGain` (floats;
  gains linear, not dB). Writer bumps a version counter (odd = writing);
  reader retries on odd/changed version. Reader never blocks the writer.

RMS values are one-pole smoothed mean-square (~50 ms release) computed per
sample and published per block; peaks are the per-block absolute max
(consumer applies its own ballistics/decay). `duckGain` is the engine's
current duck gain (1.0 = no ducking). Stereo is folded max-of-channels for
peaks and mean-of-channels for RMS.

## 3. Tap-fire detection

Per tap, on the tap's summed wet contribution (enabled taps only, max |.|
across channels), a `viz::TapFireDetector`:

- Envelope follower: attack 1 ms, release 50 ms (Ducker coefficient formula).
- Fire on upward crossing of the threshold, −40 dB (0.01 linear).
- Re-trigger hold after each fire: half the tap's delay time, clamped to
  [20 ms, 250 ms] — one echo, one pulse. Hold is recomputed when the tap
  time changes.
- Intensity: measured as the peak |tap output| from the crossing until 5 ms
  after it (or the end of the hold, whichever comes first), clamped to
  [0, 1]. The event is pushed when that measurement window closes but
  carries the timestamp of the original crossing — sample-accurate timing,
  ~5 ms reporting latency, imperceptible for animation. (Envelope value at
  the crossing itself would always be ≈ the threshold — useless as a
  loudness signal.) Disabled taps emit nothing and their detectors reset.

## 4. Engine integration

- `OrbitEngine::prepare()` prepares the feed (allocates ring, resets
  counters, prepares detectors); `reset()` clears events and snapshot.
- In the per-sample loop the engine already has each tap's per-channel
  outputs (`outs[]`) and the duck gain: it feeds detectors and level
  accumulators there. Event pushes and the snapshot publish happen once per
  block (fires are queued sample-accurately with the absolute timestamp at
  which they crossed). Clarification: fire events are pushed from the
  per-sample loop at the moment each intensity window closes
  (sample-accurate; bounded wait-free work), not batched at block end — the
  block-end publish applies to the level snapshot only. No allocation,
  locks, or logging in the audio path.
- Public accessor: `viz::VizFeed& OrbitEngine::vizFeed()` (and const
  overload). The processor exposes it to the future editor; nothing else
  changes in the plugin layer this wave.
- The feed is always on. Per-sample cost: 4 envelope followers + 2 level
  accumulators (a handful of multiply-adds).

## 5. Terminal demo

CMake target `viz-demo` (`tools/viz_demo.cpp`), JUCE-free, links the same
engine + core the plugin uses. Runs a deterministic rhythmic impulse pattern
through a ping-pong + ducking configuration at 48 kHz, renders ~30 fps to the
terminal: input/output meter bars, duck-gain readout, and per-tap fire
markers with intensity. Runs for a fixed duration (~10 s) then exits 0, so it
is scriptable. Real-time pacing via std::this_thread::sleep (host side only —
never in engine code).

## 6. Testing

- Ring buffer: fill/drain semantics, wraparound, drop-when-full, and a
  two-thread producer/consumer stress test (no losses below capacity, no
  duplicates, no torn events).
- Seqlock: reader never observes a torn snapshot while a writer spins
  (two-thread test with a recognizable field pattern).
- TapFireDetector: impulse train through one tap → exactly one event per
  audible repeat; silence → zero events; louder echo → higher intensity;
  hold prevents double-fires; timestamps within one block of the true echo
  positions.
- Engine integration: process a buffer with known taps → events carry
  correct tap indices; duckGain in the snapshot drops during ducking; RMS/
  peak fields track a known signal.
- Regression: all 49 existing cases pass unchanged; default audio output is
  bit-identical (the feed observes, never modifies the signal path).

## 7. Constraints (binding)

- No JUCE anywhere in `core/`, `plugin/OrbitEngine.*`, or `tools/`.
- No allocation/locks/logging in per-sample or per-block audio paths after
  `prepare()`.
- No new plugin parameters; `stateVersion` stays 1; auval must still report
  34 Global Scope Parameters and pass.
- Namespace `orbit::viz`. Conventional commits. Branch `dev/phase-2-wave-3`.
