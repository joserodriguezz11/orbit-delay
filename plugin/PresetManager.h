#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace orbit {

struct PresetInfo {
    juce::String name, tags, author, description;
    bool isFactory = false;
    juce::File file;            // invalid for factory presets
    int binaryDataIndex = -1;   // valid for factory presets
};

// Owns preset enumeration and application (spec §2). Message-thread only;
// the audio thread is touched solely via the same apvts.replaceState
// mechanism the DAW already uses.
//
// PHASE 3 NOTES (UI contract):
// - No change-notification surface: poll isModified() / isSlotB() /
//   currentPresetName() from a juce::Timer — there are no callbacks to hook.
// - Any future second juce_add_binary_data target MUST use a custom
//   NAMESPACE, otherwise the duplicate BinaryData symbols collide at link.
// - presetXmlFor doubles as the preview/export hook (raw .orbitpreset text).
class PresetManager : private juce::AudioProcessorValueTreeState::Listener {
public:
    // Test seam: a valid userDirOverride points user presets at that folder;
    // an invalid (default) File means the production default directory.
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts,
                           const juce::File& userDirOverride = {});
    ~PresetManager() override;

    // Enumeration
    const juce::Array<PresetInfo>& factoryPresets() const;  // embedded BinaryData, filename order
    juce::Array<PresetInfo> userPresets() const;             // rescans folder
    juce::Array<PresetInfo> filterByTag(const juce::String& tag) const; // both lists

    // Application
    bool loadPreset(const PresetInfo&);                 // false on invalid/foreign
    juce::String currentPresetName() const;             // "" if none loaded
    // True once any parameter changed since the last load/save. Note: reads
    // true after a host project restore even with no preset loaded (session
    // restore fires the parameter listeners) — the UI should gate its dirty
    // indicator on currentPresetName().isNotEmpty().
    bool isModified() const;

    // Raw XML text of a preset (factory: embedded BinaryData resource; user:
    // file contents). Empty string if the preset cannot be resolved.
    juce::String presetXmlFor(const PresetInfo&) const;

    // Fixed tag vocabulary (spec §1): { Ambient, Rhythm, Dub, Tape, Wide,
    // Utility }. Single source of truth for the Phase-3 filter buttons and
    // save-time tag enforcement.
    static const juce::StringArray& tagVocabulary();

    // User presets
    // The tagVocabulary() set is enforced by the Phase 3 UI, not here — tags
    // are stored verbatim and matching (filterByTag) is case-sensitive.
    bool saveUserPreset(const juce::String& name, const juce::String& tags,
                        const juce::String& description, bool overwrite);
        // false if exists && !overwrite, or unwritable; filename sanitized
    bool deleteUserPreset(const PresetInfo&);           // user presets only
    bool renameUserPreset(const PresetInfo&, const juce::String& newName);

    // A/B compare (spec §3): two session-local full-state slots, never
    // serialized. Fresh instance = both slots hold construction state, A live.
    void toggleAB();      // capture live -> current slot, apply the other; sets modified
    void copyAB();        // inactive := live; live state and modified flag untouched
    bool isSlotB() const; // false = slot A live

    static juce::File userPresetDirectory();  // created on demand

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Serialization helpers (shared by save/load paths).
    juce::ValueTree stateWithMeta(const juce::String& name, const juce::String& tags,
                                  const juce::String& description) const;
    bool applyPresetXml(const juce::String& xml, const juce::String& presetName);

    juce::File presetDir() const;
    static juce::String sanitizeName(const juce::String& name);

    // RAII suppress-dirty window for replaceState paths. Hand-rolled because
    // juce::ScopedValueSetter can't operate on std::atomic<bool>.
    struct ScopedSuppress {
        std::atomic<bool>& f;
        explicit ScopedSuppress(std::atomic<bool>& x) : f(x) { f.store(true, std::memory_order_relaxed); }
        ~ScopedSuppress() { f.store(false, std::memory_order_relaxed); }
    };

    juce::AudioProcessorValueTreeState& apvts_;
    juce::File userDirOverride_;
    juce::Array<PresetInfo> factoryPresets_;  // enumerated from BinaryData at construction
    juce::StringArray listenedParamIds_;
    juce::String currentPresetName_;
    // Atomic (relaxed): the listener callback parameterChanged can fire on the
    // AUDIO thread under host automation via the VST3/AU wrappers, racing the
    // message thread that reads/writes these flags.
    std::atomic<bool> modified_ { false };
    std::atomic<bool> suppressDirty_ { false };   // guards listener during load/save

    // A/B slots hold parameter-only trees (no PresetMeta ever, spec §3).
    // TRIPWIRE: slots are session-local and NEVER serialized (spec §3) — DAW
    // state save/restore is independent of their contents by design. If slots
    // ever become part of serialized state, setStateInformation must refresh
    // them after replaceState (see the matching note in PluginProcessor.cpp).
    juce::ValueTree slotA_, slotB_;
    bool slotBActive_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace orbit
