#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OrbitControls.h"

// The 190px control rail beside the orb pad: BLEND (mix, duck + gain-
// reduction meter), SPACE (width, ping-pong), MOTION (depth, rate), FILTER
// (low/high cut), then the SYN·FX·001 footer. All controls bind to the APVTS
// through attachments; value text uses the mockup formatters. Laid out in
// design pixels — the editor scales the whole component with a transform.
class OrbitRail : public juce::Component {
public:
    enum class Knob { Mix, Duck, Width, ModDepth, ModRate, LowCut, HighCut };
    static constexpr int kNumKnobs = 7;

    OrbitRail(juce::AudioProcessorValueTreeState& apvts,
              std::function<float()> duckGainReduction);

    OrbitKnob& knob(Knob which) { return knobs_[size_t(which)]; }
    OrbitSwitch& pingPong() { return pingPong_; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Section { const char* title; float y; };

    std::array<OrbitKnob, kNumKnobs> knobs_;
    OrbitSwitch pingPong_;
    OrbitMeter duckMeter_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               kNumKnobs> knobAtts_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitRail)
};
