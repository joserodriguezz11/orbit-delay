#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

// The preset browser overlay: full-window scrim + a 860x420 panel with tag
// chips and a four-column preset grid (10 rows — the full 40-preset factory
// set fits; overflow rows are neither drawn nor clickable, see rowFits).
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

    // Grid geometry seams: paint and mouseUp both gate rows on rowFits, so
    // what is clickable is exactly what is drawn.
    juce::Rectangle<float> rowBounds(int visibleRow) const;
    bool rowFits(int visibleRow) const;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    struct Entry { orbit::PresetInfo info; int number; };

    juce::Rectangle<float> panelBounds() const;
    juce::Rectangle<float> chipBounds(int chip) const;
    void rebuildVisible();

    orbit::PresetManager& presets_;
    juce::Array<Entry> all_, visible_;
    juce::String tagFilter_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitPresetBrowser)
};
