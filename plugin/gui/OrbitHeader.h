#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OrbitControls.h"
#include "PluginProcessor.h"

// The 52px header: logo lockup, preset capsule (prev / name+dirty dot /
// next, click opens the browser), A/B compare, CLEAN/TAPE/GRIT seg, FREEZE
// pill, IN/OUT meters. Preset state is polled from PresetManager on a timer
// (it exposes no callbacks by contract). Design-pixel layout.
class OrbitHeader : public juce::Component, private juce::Timer {
public:
    explicit OrbitHeader(OrbitAudioProcessor& proc);

    std::function<void()> onBrowserToggle;

    // Preset navigation over factory + user presets (wraps at the ends).
    void nextPreset() { stepPreset(1); }
    void prevPreset() { stepPreset(-1); }
    // Activates a slot: true = B. Clicking the live slot is a no-op.
    void selectSlot(bool slotB);

    // Reserved hit area for the settings gear (wired in the settings task).
    juce::Rectangle<int> gearBounds() const { return gearBounds_; }

    OrbitSeg& characterSeg() { return seg_; }
    OrbitPill& freezePill() { return freeze_; }
    juce::TextButton& slotAButton() { return slotA_; }
    juce::TextButton& slotBButton() { return slotB_; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void stepPreset(int delta);
    juce::Array<orbit::PresetInfo> allPresets() const;
    int currentIndex(const juce::Array<orbit::PresetInfo>&) const;

    OrbitAudioProcessor& proc_;

    juce::TextButton prev_ { juce::CharPointer_UTF8("\xe2\x80\xb9") },
                     next_ { juce::CharPointer_UTF8("\xe2\x80\xba") };
    juce::TextButton slotA_ { juce::String() }, slotB_ { juce::String() };
    OrbitSeg seg_;
    OrbitPill freeze_ { "FREEZE" };
    OrbitMeter inMeter_, outMeter_;
    std::unique_ptr<juce::ParameterAttachment> charAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt_;

    juce::Rectangle<int> presetCapsule_;   // click target for the browser
    juce::Rectangle<int> gearBounds_;
    juce::String shownPresetName_;
    bool shownDirty_ = false, shownSlotB_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitHeader)
};
