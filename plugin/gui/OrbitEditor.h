#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Top-level editor shell (Phase 3 Wave 1, Task 3). Owns the window: fixed
// 11:7 aspect, resizable 100%-200% of the 1100x700 base, and exposes the
// single UI-side proportional scale factor every child layout derives from.
// No controls yet — children arrive in Task 5.
class OrbitEditor : public juce::AudioProcessorEditor {
public:
    static constexpr int kBaseW = 1100;
    static constexpr int kBaseH = 700;

    explicit OrbitEditor(OrbitAudioProcessor& proc);

    // The single layout scale factor children use (1.0 at base, 2.0 at max).
    double scale() const { return getWidth() / double(kBaseW); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Member so it outlives every host-driven resize (setConstrainer keeps a
    // raw pointer to it).
    juce::ComponentBoundsConstrainer constrainer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitEditor)
};
