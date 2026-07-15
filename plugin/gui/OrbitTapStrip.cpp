#include "OrbitTapStrip.h"
#include "OrbitFonts.h"
#include "OrbitSync.h"
#include "Parameters.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;
using orbit::dsp::SyncDivision;

namespace {

// The enable dot: a 26px numbered circle button filled with the tap accent.
class TapDot : public juce::Button {
public:
    explicit TapDot(int number) : juce::Button("tap dot"), number_(number) {
        setClickingTogglesState(true);
    }
    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    void paintButton(juce::Graphics& g, bool, bool) override {
        const auto b = getLocalBounds().toFloat().reduced(1.0f);
        const bool on = getToggleState();
        if (on) {
            g.setColour(accent_);
            g.fillEllipse(b);
        }
        g.setColour(on ? accent_ : theme::bone50.withAlpha(0.3f));
        g.drawEllipse(b, 1.5f);
        g.setFont(fonts::monoSemiBold(10.0f));
        g.setColour(on ? theme::ink900 : theme::bone50.withAlpha(0.45f));
        g.drawText(juce::String(number_), getLocalBounds(),
                   juce::Justification::centred, false);
    }
private:
    int number_;
    juce::Colour accent_ { theme::ember };
};

int currentDivision(juce::AudioProcessorValueTreeState& apvts, int tap) {
    return int(apvts.getRawParameterValue(orbit::params::tapSyncId(tap))->load());
}

float currentTimeMs(juce::AudioProcessorValueTreeState& apvts, int tap) {
    return apvts.getRawParameterValue(orbit::params::tapTimeId(tap))->load();
}

} // namespace

// ----------------------------------------------------------------------- Row

class OrbitTapStrip::Row : public juce::Component {
public:
    Row(OrbitTapStrip& strip, juce::AudioProcessorValueTreeState& apvts, int index)
        : strip_(strip), apvts_(apvts), index_(index),
          dot_(index + 1), sync_("SYNC"), rev_("REV") {
        addAndMakeVisible(dot_);
        addAndMakeVisible(sync_);
        addAndMakeVisible(rev_);
        addAndMakeVisible(pitch_);

        dotAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, orbit::params::tapEnabledId(index), dot_);
        revAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, orbit::params::tapReverseId(index), rev_);
        pitchAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, orbit::params::tapPitchId(index), pitch_);
        // After the attachment — it installs the parameter's text function.
        pitch_.textFromValueFunction = [] (double v) {
            const int r = juce::roundToInt(v);
            return (r > 0 ? "+" : "") + juce::String(r);
        };
        pitch_.setKnobDefault(0.0);
        pitch_.onValueChange = [this] {
            pitch_.setLinked(juce::roundToInt(pitch_.getValue()) != 0);
        };

        // Enabling from the dot also selects the tap (mockup dot onClick).
        dot_.onClick = [this] { strip_.selectRow(index_); };

        // SYNC is a boolean gesture over the division choice parameter.
        sync_.onClick = [this] { strip_.setSyncEnabled(index_, sync_.getToggleState()); };

        // Repaint the readout when time/feedback/sync move (host automation
        // included — ParameterAttachment marshals to the message thread).
        const auto repaintRow = [this] (float) { repaint(); };
        for (const auto& id : { orbit::params::tapTimeId(index),
                                orbit::params::tapFeedbackId(index) })
            valueAtts_.push_back(std::make_unique<juce::ParameterAttachment>(
                *apvts.getParameter(id), repaintRow, nullptr));
        syncAtt_ = std::make_unique<juce::ParameterAttachment>(
            *apvts.getParameter(orbit::params::tapSyncId(index)),
            [this] (float v) {
                sync_.setToggleState(int(v) != int(SyncDivision::Free),
                                     juce::dontSendNotification);
                repaint();
            },
            nullptr);
        syncAtt_->sendInitialUpdate();
    }

    void setAccent(juce::Colour accent) {
        accent_ = accent;
        dot_.setAccent(accent);
        sync_.setAccent(accent);
        rev_.setAccent(accent);
        pitch_.setAccent(accent);
        repaint();
    }

    void resized() override {
        // Design: padding 0 14, dot 26 centred, pills column, pitch right.
        const int h = getHeight();
        const int w = getWidth();
        dot_.setBounds(14, (h - 26) / 2, 26, 26);

        // Pitch knob stays right-anchored on the card's right margin.
        constexpr int pitchW = 34;
        const int pitchX = w - 14 - pitchW;
        pitch_.setBounds(pitchX, (h - pitchW) / 2 - 4, pitchW, pitchW);

        // Pills are right-anchored to the pitch knob with a fixed clearance,
        // rather than a left-anchored fixed offset — at the narrower ~211px
        // card (900px window rebase) a fixed pillX collided with the knob.
        // Clearance comfortably clears the >=6px requirement.
        constexpr int pillW = 40;
        constexpr int pillClearance = 7;
        const int pillX = pitchX - pillClearance - pillW;
        sync_.setBounds(pillX, h / 2 - 22, pillW, 20);
        rev_.setBounds(pillX, h / 2 + 2, pillW, 20);

        // The time/FB readout fills the space between the dot and the pill
        // column, capped at the original design width so unshrunk cards
        // keep their look.
        const int readoutLeft = 14 + 26 + 12;
        readoutW_ = juce::jlimit(24, 64, pillX - 12 - readoutLeft);
    }

    void paint(juce::Graphics& g) override {
        const bool sel = strip_.selected() == index_;
        const bool on = dot_.getToggleState();
        const auto b = getLocalBounds().toFloat().reduced(0.5f);

        // Card: selection tints border/fill with the tap accent.
        g.setColour(sel ? accent_.withAlpha(on ? 0.08f : 0.04f)
                        : theme::bone50.withAlpha(0.02f));
        g.fillRoundedRectangle(b, 10.0f);
        g.setColour(sel ? accent_.withAlpha(on ? 0.8f : 0.5f)
                        : theme::bone50.withAlpha(0.09f));
        g.drawRoundedRectangle(b, 10.0f, 1.0f);
        if (on) {
            // Mockup inset bottom glow.
            g.setColour(accent_.withAlpha(sel ? 0.5f : 0.22f));
            g.fillRect(b.getX() + 6.0f, b.getBottom() - 2.0f, b.getWidth() - 12.0f, 2.0f);
        }

        // Readout: big time, small FB%.
        const float tx = 14.0f + 26.0f + 12.0f;
        const float readoutW = float(readoutW_);
        g.setFont(fonts::monoSemiBold(14.0f));
        g.setColour(theme::text());
        g.drawText(strip_.timeText(index_),
                   juce::Rectangle<float>(tx, b.getCentreY() - 13.0f, readoutW, 14.0f),
                   juce::Justification::centredLeft, false);
        const int fbPct = juce::roundToInt(
            apvts_.getRawParameterValue(orbit::params::tapFeedbackId(index_))->load() * 100.0f);
        g.setFont(fonts::tracked(fonts::mono(8.5f), 0.08f));
        g.setColour(theme::bone50.withAlpha(0.45f));
        g.drawText("FB " + juce::String(fbPct) + "%",
                   juce::Rectangle<float>(tx, b.getCentreY() + 2.0f, readoutW, 9.0f),
                   juce::Justification::centredLeft, false);

        // PITCH caption under the knob.
        g.setFont(fonts::tracked(fonts::monoSemiBold(6.5f), 0.2f));
        g.setColour(theme::bone50.withAlpha(0.4f));
        g.drawText("PITCH",
                   juce::Rectangle<float>(float(pitch_.getX()) - 8.0f,
                                          float(pitch_.getBottom()) + 1.0f,
                                          float(pitch_.getWidth()) + 16.0f, 8.0f),
                   juce::Justification::centred, false);
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (getLocalBounds().contains(e.getPosition()))
            strip_.selectRow(index_);
    }

    TapDot& dot() { return dot_; }
    OrbitPill& syncPill() { return sync_; }
    OrbitPill& revPill() { return rev_; }
    OrbitKnob& pitchKnob() { return pitch_; }

private:
    OrbitTapStrip& strip_;
    juce::AudioProcessorValueTreeState& apvts_;
    int index_;
    TapDot dot_;
    OrbitPill sync_, rev_;
    OrbitKnob pitch_;
    int readoutW_ = 64;
    juce::Colour accent_ { theme::ember };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dotAtt_, revAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAtt_;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> valueAtts_;
    std::unique_ptr<juce::ParameterAttachment> syncAtt_;
};

// --------------------------------------------------------------------- strip

OrbitTapStrip::OrbitTapStrip(juce::AudioProcessorValueTreeState& apvts,
                             std::function<double()> bpmGetter)
    : apvts_(apvts), bpm_(std::move(bpmGetter)) {
    for (int i = 0; i < 4; ++i) {
        rows_[size_t(i)] = std::make_unique<Row>(*this, apvts, i);
        addAndMakeVisible(*rows_[size_t(i)]);
    }
}

OrbitTapStrip::~OrbitTapStrip() = default;

void OrbitTapStrip::setSelected(int i) {
    if (i == selected_ || i < 0 || i >= 4)
        return;
    selected_ = i;
    repaint();
    for (auto& r : rows_)
        r->repaint();
}

void OrbitTapStrip::setTapAccent(int i, juce::Colour accent) {
    if (i >= 0 && i < 4)
        rows_[size_t(i)]->setAccent(accent);
}

void OrbitTapStrip::selectRow(int i) {
    setSelected(i);
    if (onSelect != nullptr)
        onSelect(i);
}

void OrbitTapStrip::setSyncEnabled(int i, bool on) {
    if (i < 0 || i >= 4)
        return;
    auto* syncParam = apvts_.getParameter(orbit::params::tapSyncId(i));
    auto* timeParam = apvts_.getParameter(orbit::params::tapTimeId(i));
    const double bpm = bpm_ != nullptr ? bpm_() : 120.0;

    if (on) {
        // Nearest real division to the current free time.
        const int best = orbit::gui::nearestSyncDivision(currentTimeMs(apvts_, i), bpm);
        syncParam->setValueNotifyingHost(syncParam->convertTo0to1(float(best)));
    } else {
        // Keep the effective time when dropping back to Free.
        const int div = currentDivision(apvts_, i);
        if (div != int(SyncDivision::Free)) {
            const float effMs = orbit::dsp::divisionToSeconds(SyncDivision(div), bpm) * 1000.0f;
            if (effMs > 0.0f)
                timeParam->setValueNotifyingHost(timeParam->convertTo0to1(effMs));
        }
        syncParam->setValueNotifyingHost(syncParam->convertTo0to1(float(SyncDivision::Free)));
    }
}

juce::String OrbitTapStrip::timeText(int i) const {
    const int div = currentDivision(apvts_, i);
    if (div != int(SyncDivision::Free))
        return orbit::dsp::kSyncDivisionNames[div];
    return juce::String(juce::roundToInt(currentTimeMs(apvts_, i))) + "ms";
}

juce::Button& OrbitTapStrip::dot(int i)        { return rows_[size_t(i)]->dot(); }
OrbitPill& OrbitTapStrip::syncPill(int i)      { return rows_[size_t(i)]->syncPill(); }
OrbitPill& OrbitTapStrip::revPill(int i)       { return rows_[size_t(i)]->revPill(); }
OrbitKnob& OrbitTapStrip::pitchKnob(int i)     { return rows_[size_t(i)]->pitchKnob(); }

void OrbitTapStrip::resized() {
    // Container: padding 12/16, four equal cards with 8px gaps.
    const float x0 = 16.0f, gap = 8.0f;
    const float w = (float(getWidth()) - x0 * 2.0f - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i)
        rows_[size_t(i)]->setBounds(juce::Rectangle<float>(
            x0 + float(i) * (w + gap), 12.0f, w, float(getHeight()) - 24.0f)
            .toNearestInt());
}

void OrbitTapStrip::paint(juce::Graphics& g) {
    g.setGradientFill({ juce::Colour { 0xff0b0b12 }, 0.0f, 0.0f,
                        juce::Colour { 0xff090910 }, 0.0f, float(getHeight()), false });
    g.fillRect(getLocalBounds());
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(0.0f, 0.0f, float(getWidth()), 1.0f);
}
