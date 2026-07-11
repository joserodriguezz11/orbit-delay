#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/OrbPad.h"
#include "gui/OrbitRail.h"
#include "gui/OrbitTapStrip.h"
#include "gui/OrbitTheme.h"

// Top-level editor: fixed-aspect window (100%-200% of the 1180x720 Full-v2
// base) hosting the orb pad, control rail and tap strip, laid out in design
// pixels and scaled with a single transform. The editor is the binder: it
// owns the per-tap ParameterAttachments, distributes selection, translates
// orb drags into parameter writes (snapping synced taps onto real divisions),
// and pushes position colours to the strip. Header controls arrive next.
class OrbitEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    static constexpr int kBaseW = orbit::gui::theme::kWindowW;
    static constexpr int kBaseH = orbit::gui::theme::kWindowH;

    explicit OrbitEditor(OrbitAudioProcessor& proc);
    ~OrbitEditor() override;

    // The single layout scale factor children derive from (1.0 at base).
    double scale() const { return getWidth() / double(kBaseW); }

    // Child seams — the riding knobs and tests reach across components.
    OrbPad& orbPad() { return pad_; }
    OrbitTapStrip& tapStrip() { return strip_; }
    OrbitRail& rail() { return rail_; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setSelectedTap(int i);
    void refreshTap(int i);
    void refreshSyncGrid();
    void writeOrb(int i, float x, float y);

    OrbitAudioProcessor& proc_;
    OrbPad pad_;
    OrbitRail rail_;
    OrbitTapStrip strip_;

    // Per-tap view attachments (time/feedback/sync/enabled/reverse) plus
    // character + freeze; all funnel into refreshTap()/pad setters.
    std::vector<std::unique_ptr<juce::ParameterAttachment>> viewAtts_;

    juce::ComponentBoundsConstrainer constrainer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitEditor)
};
