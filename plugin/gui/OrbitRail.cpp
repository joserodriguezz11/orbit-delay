#include "OrbitRail.h"
#include "OrbitFonts.h"
#include "Parameters.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

namespace {

// Design-pixel layout (aside: padding 14/16, sections stacked, header 13px
// + 9 gap, knob row, computed section gap). Section content y-origins below.
constexpr float kPadX = 16.0f;
constexpr float kHeaderH = 13.0f, kHeaderGap = 9.0f;
constexpr float kLabelH = 12.0f, kKnobGap = 18.0f;

struct KnobSpec {
    const char* paramId;
    int size;
    juce::String (*fmt)(double);
};

juce::String fmtPercent(double v)  { return juce::String(juce::roundToInt(v * 100.0)) + "%"; }
juce::String fmtHalfPct(double v)  { return juce::String(juce::roundToInt(v * 50.0)) + "%"; }
juce::String fmtRate(double v)     { return juce::String(v, v < 2.0 ? 2 : 1); }
juce::String fmtHz(double v)       { return juce::String(juce::roundToInt(v)); }
juce::String fmtKilo(double v) {
    const double k = v / 1000.0;
    return k < 10.0 ? juce::String(k, 1) + "k"
                    : juce::String(juce::roundToInt(k)) + "k";
}

// Index-aligned with OrbitRail::Knob.
const KnobSpec kSpecs[] = {
    { orbit::params::kDryWetId,   56, fmtPercent },
    { orbit::params::kDuckId,     44, fmtPercent },
    { orbit::params::kWidthId,    44, fmtHalfPct },
    { orbit::params::kModDepthId, 44, fmtPercent },
    { orbit::params::kModRateId,  44, fmtRate },
    { orbit::params::kLowCutId,   44, fmtHz },
    { orbit::params::kHighCutId,  44, fmtKilo },
};

} // namespace

float OrbitRail::sectionGap(float railHeight) {
    constexpr float kFixedContent = 361.0f;   // pads + headers + rows + labels
    return std::max(6.0f, (railHeight - kFixedContent) / 3.0f);
}

OrbitRail::OrbitRail(juce::AudioProcessorValueTreeState& apvts,
                     std::function<float()> duckGainReduction)
    : duckMeter_(std::move(duckGainReduction), true,
                 juce::Colour { 0xffe8a33d }) {
    for (size_t i = 0; i < kNumKnobs; ++i) {
        auto& k = knobs_[i];
        const auto& spec = kSpecs[i];
        if (auto* param = apvts.getParameter(spec.paramId))
            k.setKnobDefault(double(param->convertFrom0to1(param->getDefaultValue())));
        addAndMakeVisible(k);
        knobAtts_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, spec.paramId, k);
        // After the attachment: SliderAttachment installs the parameter's own
        // text function on attach, which would clobber the mockup formatter.
        k.textFromValueFunction = [fmt = spec.fmt] (double v) { return fmt(v); };
    }
    addAndMakeVisible(pingPong_);
    pingAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, orbit::params::kPingPongId, pingPong_);
    addAndMakeVisible(duckMeter_);
}

void OrbitRail::resized() {
    const float gap = sectionGap(float(getHeight()));

    // Sections: BLEND, SPACE, MOTION, FILTER from y=14, each header + row.
    float y = 14.0f + kHeaderH + kHeaderGap;
    const auto place = [&] (Knob which, float x, float rowH) {
        auto& k = knobs_[size_t(which)];
        const int s = kSpecs[size_t(which)].size;
        // Rows align flex-end: knobs sit on the row's baseline.
        k.setBounds(int(x), int(y + rowH - float(s)), s, s);
    };

    // The second column right-anchors to the rail's right padding, so the
    // two columns spread across the full width instead of leaving a dead
    // right margin (left-flowed columns only filled ~134 of the 170px).
    const auto placeRight = [&] (Knob which, float rowH) {
        const float s = float(kSpecs[size_t(which)].size);
        place(which, float(getWidth()) - kPadX - s, rowH);
    };

    // BLEND: MIX 56 + DUCK 44 (+ meter under the duck label).
    place(Knob::Mix, kPadX, 56.0f);
    placeRight(Knob::Duck, 56.0f);
    duckMeter_.setBounds(int(float(getWidth()) - kPadX - 44.0f),
                         int(y + 56.0f + kLabelH + 3.0f), 44, 4);
    y += 56.0f + kLabelH + 9.0f + gap + kHeaderH + kHeaderGap;

    // SPACE: WIDTH 44 + P-PONG switch (centre-aligned with the knob).
    place(Knob::Width, kPadX, 44.0f);
    pingPong_.setBounds(int(float(getWidth()) - kPadX - float(OrbitSwitch::kWidth)),
                        int(y + (44.0f - OrbitSwitch::kHeight) / 2.0f),
                        OrbitSwitch::kWidth, OrbitSwitch::kHeight);
    y += 44.0f + kLabelH + gap + kHeaderH + kHeaderGap;

    // MOTION: DEPTH + RATE.
    place(Knob::ModDepth, kPadX, 44.0f);
    placeRight(Knob::ModRate, 44.0f);
    y += 44.0f + kLabelH + gap + kHeaderH + kHeaderGap;

    // FILTER: LO CUT + HI CUT.
    place(Knob::LowCut, kPadX, 44.0f);
    placeRight(Knob::HighCut, 44.0f);
}

void OrbitRail::paint(juce::Graphics& g) {
    const float gap = sectionGap(float(getHeight()));
    const float W = float(getWidth()), H = float(getHeight());

    // Panel: vertical gradient + left hairline.
    g.setGradientFill({ juce::Colour { 0xff0b0b12 }, 0.0f, 0.0f,
                        juce::Colour { 0xff08080e }, 0.0f, H, false });
    g.fillRect(0.0f, 0.0f, W, H);
    g.setColour(theme::line());
    g.fillRect(0.0f, 0.0f, 1.0f, H);

    const auto sectionHead = [&] (const char* title, float y) {
        g.setColour(theme::ember);
        g.fillRect(kPadX, y + 4.0f, 4.0f, 4.0f);
        g.setFont(fonts::tracked(fonts::monoSemiBold(8.0f), 0.26f));
        g.setColour(theme::bone50.withAlpha(0.45f));
        const float textX = kPadX + 11.0f;
        g.drawText(title, juce::Rectangle<float>(textX, y, 120.0f, kHeaderH),
                   juce::Justification::centredLeft, false);
        const float lineX = textX + fonts::monoSemiBold(8.0f).getStringWidthFloat(title) * 1.26f + 7.0f;
        g.setColour(theme::bone50.withAlpha(0.07f));
        g.fillRect(lineX, y + kHeaderH / 2.0f, W - kPadX - lineX, 1.0f);
    };

    const auto knobLabel = [&] (const char* label, Knob which) {
        const auto& k = knobs_[size_t(which)];
        g.setFont(fonts::tracked(fonts::monoSemiBold(7.5f), 0.18f));
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawText(label,
                   juce::Rectangle<float>(float(k.getX()) - 8.0f, float(k.getBottom()) + 2.0f,
                                          float(k.getWidth()) + 16.0f, 10.0f),
                   juce::Justification::centred, false);
    };

    float y = 14.0f;
    sectionHead("BLEND", y);
    y += kHeaderH + kHeaderGap;
    knobLabel("MIX", Knob::Mix);
    knobLabel("DUCK", Knob::Duck);
    y += 56.0f + kLabelH + 9.0f + gap;

    sectionHead("SPACE", y);
    y += kHeaderH + kHeaderGap;
    knobLabel("WIDTH", Knob::Width);
    {
        // P-PONG label under the switch.
        g.setFont(fonts::tracked(fonts::monoSemiBold(7.5f), 0.18f));
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawText("P-PONG",
                   juce::Rectangle<float>(float(pingPong_.getX()) - 8.0f,
                                          float(pingPong_.getBottom()) + 6.0f,
                                          float(pingPong_.getWidth()) + 16.0f, 10.0f),
                   juce::Justification::centred, false);
    }
    y += 44.0f + kLabelH + gap;

    sectionHead("MOTION", y);
    y += kHeaderH + kHeaderGap;
    knobLabel("DEPTH", Knob::ModDepth);
    knobLabel("RATE", Knob::ModRate);
    y += 44.0f + kLabelH + gap;

    sectionHead("FILTER", y);
    knobLabel("LO CUT", Knob::LowCut);
    knobLabel("HI CUT", Knob::HighCut);
}
