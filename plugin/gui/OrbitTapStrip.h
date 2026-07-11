#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OrbitControls.h"
#include "dsp/TempoSync.h"

// The 96px tap strip: four cards, one per tap — enable dot, time/feedback
// readout, SYNC + REV pills, pitch knob. Dot/REV/pitch bind straight to
// their parameters; SYNC translates the pill's boolean gesture onto the
// tapN_sync division choice (on = nearest division to the current time at
// host tempo, off = back to Free keeping the effective time). Selection and
// per-tap accent colours flow in from the editor. Design-pixel layout.
class OrbitTapStrip : public juce::Component {
public:
    OrbitTapStrip(juce::AudioProcessorValueTreeState& apvts,
                  std::function<double()> bpmGetter);
    ~OrbitTapStrip() override;

    std::function<void(int)> onSelect;

    void setSelected(int i);
    int selected() const { return selected_; }
    // Editor pushes each tap's position colour (theme::tapLch → Colour).
    void setTapAccent(int i, juce::Colour accent);

    // Boolean sync gesture → division choice (public: the riding TIME knob
    // and tests drive the same path).
    void setSyncEnabled(int i, bool on);
    void selectRow(int i);

    // Seams for tests and the editor.
    juce::Button& dot(int i);
    OrbitPill& syncPill(int i);
    OrbitPill& revPill(int i);
    OrbitKnob& pitchKnob(int i);
    juce::String timeText(int i) const;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Row;

    juce::AudioProcessorValueTreeState& apvts_;
    std::function<double()> bpm_;
    std::array<std::unique_ptr<Row>, 4> rows_;
    int selected_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitTapStrip)
};
