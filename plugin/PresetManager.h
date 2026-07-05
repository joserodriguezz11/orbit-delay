#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

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
class PresetManager : private juce::AudioProcessorValueTreeState::Listener {
public:
    // Test seam: a valid userDirOverride points user presets at that folder;
    // an invalid (default) File means the production default directory.
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts,
                           const juce::File& userDirOverride = {});
    ~PresetManager() override;

    // Enumeration
    const juce::Array<PresetInfo>& factoryPresets() const;  // empty until Task 3 wires BinaryData
    juce::Array<PresetInfo> userPresets() const;             // rescans folder
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

    // A/B compare — declared per spec §2; implemented in Task 2.
    void toggleAB();      // Task 2
    void copyAB();        // Task 2
    bool isSlotB() const; // Task 2

    static juce::File userPresetDirectory();  // created on demand

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Serialization helpers (shared by save/load paths).
    juce::ValueTree stateWithMeta(const juce::String& name, const juce::String& tags,
                                  const juce::String& description) const;
    bool applyPresetXml(const juce::String& xml, const juce::String& presetName);

    juce::File presetDir() const;
    static juce::String sanitizeName(const juce::String& name);

    juce::AudioProcessorValueTreeState& apvts_;
    juce::File userDirOverride_;
    juce::Array<PresetInfo> factoryPresets_;  // populated in Task 3 (BinaryData)
    juce::StringArray listenedParamIds_;
    juce::String currentPresetName_;
    bool modified_ = false;
    bool suppressDirty_ = false;   // guards listener during load/save

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace orbit
