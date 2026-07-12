#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

// The preset browser overlay: full-window scrim + a 566x420 panel (top-left
// under the preset capsule) with tag chips and a two-column preset grid.
// Picking a preset loads it and leaves the panel open (mockup behaviour);
// clicking the scrim or × closes. Rebuilds its list on every show so user
// presets saved mid-session appear without a restart.
class OrbitPresetBrowser : public juce::Component {
public:
    explicit OrbitPresetBrowser(orbit::PresetManager& presets);

    std::function<void()> onClose;

    void refresh();                              // re-enumerate + repaint
    void setTagFilter(const juce::String& tag);  // empty = ALL
    int tagCount() const;                        // chips incl. ALL
    int visibleCount() const;
    juce::String visibleName(int row) const;
    void pickVisible(int row);

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    struct Entry { orbit::PresetInfo info; int number; };

    juce::Rectangle<float> panelBounds() const;
    juce::Rectangle<float> chipBounds(int chip) const;
    juce::Rectangle<float> rowBounds(int visibleRow) const;
    void rebuildVisible();

    orbit::PresetManager& presets_;
    juce::Array<Entry> all_, visible_;
    juce::String tagFilter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitPresetBrowser)
};
