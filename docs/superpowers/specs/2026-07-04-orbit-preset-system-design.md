# Orbit Preset System — Design (Phase 2 Wave 4)

Approved by Jose 2026-07-04. Scope: preset data layer + A/B compare + 40
factory presets (master spec §5). The preset browser UI is Phase 3; the
engine and parameters are untouched this wave.

## 1. Preset format

A `.orbitpreset` file is XML: the processor's existing versioned state tree
(`product="orbit"`, `stateVersion=1`, all 34 parameters via APVTS) with one
additional child node:

```xml
<PresetMeta name="Dotted 8th Vocal" tags="Wide" author="Synthios"
            description="Classic dotted-eighth slap for lead vocals."/>
```

- `tags`: comma-separated, from the fixed vocabulary {Ambient, Rhythm,
  Dub, Tape, Wide, Utility}. One or more.
- Loading runs the same validation as `setStateInformation`: invalid XML →
  reject; `product != "orbit"` → reject; `stateVersion` migration switch
  applies. Presets ride the session-state migration path forever.
- `PresetMeta` is stripped before `replaceState` (the APVTS tree must stay
  parameter-only); metadata lives beside it, not inside it.

## 2. PresetManager (plugin layer; JUCE allowed)

`plugin/PresetManager.h/.cpp`. Owns preset enumeration and application.
Message-thread only; the audio thread is touched solely via the same
`apvts.replaceState` mechanism the DAW already uses.

API (binding):

```cpp
struct PresetInfo {
    juce::String name, tags, author, description;
    bool isFactory = false;
    juce::File file;            // invalid for factory presets
    int binaryDataIndex = -1;   // valid for factory presets
};

class PresetManager {
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts);
    // Enumeration
    const juce::Array<PresetInfo>& factoryPresets() const;
    juce::Array<PresetInfo> userPresets() const;        // rescans folder
    juce::Array<PresetInfo> filterByTag(const juce::String& tag) const; // both lists
    // Application
    bool loadPreset(const PresetInfo&);                 // false on invalid/foreign
    juce::String currentPresetName() const;             // "" if none loaded
    bool isModified() const;                            // param changed since load
    // User presets
    bool saveUserPreset(const juce::String& name, const juce::String& tags,
                        const juce::String& description, bool overwrite);
        // false if exists && !overwrite, or unwritable; filename sanitized
    bool deleteUserPreset(const PresetInfo&);           // user presets only
    bool renameUserPreset(const PresetInfo&, const juce::String& newName);
    // A/B
    void toggleAB();      // capture live state into outgoing slot, swap
    void copyAB();        // copy live slot over the inactive one
    bool isSlotB() const; // false = A live
    static juce::File userPresetDirectory();  // created on demand
};
```

- User preset directory: `File::getSpecialLocation(userApplicationDataDirectory)`
  → `Synthios/Orbit/Presets` — resolves to `~/Library/Application Support/…`
  on macOS and the equivalent on Windows. (Deliberately NOT the shared
  `~/Library/Audio/Presets` root: app-data is sandbox-safer and
  installer-free.) Created on first save.
- Dirty flag: an APVTS listener on all parameters sets `modified_` after a
  load; `loadPreset`/`saveUserPreset` clear it.
- Filename sanitation: strip path separators and characters illegal on
  either OS; collision policy is the `overwrite` flag.
- The processor owns one PresetManager instance and exposes it
  (`presetManager()`) for the Phase 3 editor. Host program API stays at 1
  program — the Phase 3 browser supersedes DAW program menus (decision).

## 3. A/B compare

Two full-state slots, session-local (never serialized into presets or DAW
state this wave). `toggleAB()` first captures the current live state into
the slot being left, then applies the other slot — edits are never lost.
`copyAB()` clones live over inactive. Fresh instance: both slots hold the
default state, A live. A/B operations leave `currentPresetName()` unchanged
and set the modified flag (the applied slot's state counts as an edit);
`copyAB()` alone changes nothing live and leaves the flag as-is.

## 4. Factory content — 40 presets

Recipe-authored XML source files in `assets/presets/` (repo), compiled into
the binary via `juce_add_binary_data` target `OrbitPresetData`; the plugin
target links it. Spread: Ambient 8, Rhythm 8, Dub 6, Tape 6, Wide 8,
Utility 4 = 40. Each preset: musically-justified values across the full
engine (sync divisions, character modes, ping-pong/width, reverse/pitch,
ducking where the style calls for it) + a one-line description. Naming:
`NN_Category_Name.orbitpreset` (NN = 01..40, stable sort order).
Jose auditions/tweaks the set during release QA; files are trivially
editable text.

## 5. Testing

Headless Catch2 tests (new `tests/preset/PresetManagerTests.cpp`, own CMake
target — matches what shipped; JUCE
initialised via `juce::ScopedJuceInitialiser_GUI` fixture — same pattern
pluginval uses; user-preset tests point the manager at a temp directory).
Cover:

- Save → load round-trip preserves all 34 parameter values + metadata.
- Foreign-product and corrupt files rejected; loadPreset returns false and
  leaves state untouched.
- Tag filtering returns exactly the tagged subset across factory + user.
- Dirty flag: clear after load, set after a parameter change, clear after
  save.
- A/B: edits on A survive toggle to B and back; copyAB duplicates.
- Factory validation: all 40 embedded presets parse, pass the product
  check, reference only existing parameter IDs, and every value is inside
  its parameter's range; tag vocabulary respected; names unique.
- Regression: existing 61 cases unchanged; auval still 34 params.

## 6. Constraints (binding)

- Zero changes to `core/`, `plugin/OrbitEngine.*`, `plugin/Parameters.*`,
  or any DSP behavior. `stateVersion` stays 1. No new plugin parameters.
- PresetManager is message-thread-only; no allocation/locks added to the
  audio thread.
- Conventional commits. Branch `dev/phase-2-wave-4`. Do not push until
  delivery.
