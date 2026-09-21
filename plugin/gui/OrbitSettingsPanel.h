#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OrbitTheme.h"

// Settings overlay (gear in the header): ABOUT + HOW TO USE, static text,
// same idiom as OrbitPresetBrowser — scrim, rounded panel, x close button,
// click-outside closes. Owns no state and touches no parameters.
class OrbitSettingsPanel : public juce::Component {
public:
    OrbitSettingsPanel() = default;

    std::function<void()> onClose;

    juce::Rectangle<float> panelBounds() const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitSettingsPanel)
};
