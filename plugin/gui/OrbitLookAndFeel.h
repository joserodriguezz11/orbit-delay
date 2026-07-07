#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Orbit's LookAndFeel (Phase 3 Wave 1, Task 4). Draws rotary sliders and
// toggle buttons in the design-token style from gui/OrbitTheme.h:
//   - rotary: thin arc track in theme::ink600, value arc in the active
//     accent, centred mono value readout
//   - toggle: pill switch (track + thumb) with a sans label
// Per-mode accent wiring arrives in a later task; until then the accent
// defaults to theme::ember and can be swapped via setAccentColour().
// Global namespace by convention (matches OrbitEditor / OrbitAudioProcessor).
class OrbitLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OrbitLookAndFeel();

    // Active accent for value arcs / on-state toggles (default theme::ember).
    void setAccentColour(juce::Colour accent) { accent_ = accent; }
    juce::Colour accentColour() const { return accent_; }

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

private:
    juce::Colour accent_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitLookAndFeel)
};
