# Orbit — Creative Delay Plugin

A delay where echoes are **glowing orbs you drag on a 2D pad** — time on one axis, feedback on the other. Character modes (Clean / Tape / Grit), reverse and pitch-shifted repeats, freeze, modulation, and always-on smart ducking. Genre-agnostic, one screen, no menu-diving.

**Formats (planned):** VST3 · AU · AAX — macOS + Windows
**Stack:** JUCE (C++) + CMake

## Status

🚧 Early development. The approved design spec lives at
[`docs/superpowers/specs/2026-07-01-orbit-delay-design.md`](docs/superpowers/specs/2026-07-01-orbit-delay-design.md).

## Layout

```
dsp/        Pure C++ DSP engine (no UI deps, unit-testable)
plugin/     JUCE plugin shell + parameter/state layer
ui/         UI scaffolding + integration contract
presets/    Factory presets
tests/      DSP unit tests + pluginval harness
packaging/  Installers, signing, notarization
```

## UI

The visual design is produced separately (Claude Design) and binds to the engine
through the UI integration contract in the spec (§6).

## Development

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # first configure downloads JUCE (~minutes)
cmake --build build --target orbit_tests -j8 && ./build/tests/orbit_tests   # unit tests
cmake --build build --target OrbitDelay_VST3 OrbitDelay_AU OrbitDelay_Standalone -j8
./scripts/run_pluginval.sh                # plugin validation (max strictness)
auval -v aufx Orb1 Orba                   # Apple AU validation
```

Built plugins are auto-copied to `~/Library/Audio/Plug-Ins/` — rescan in your DAW to test.
