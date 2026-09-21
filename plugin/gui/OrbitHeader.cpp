#include "OrbitHeader.h"
#include "OrbitFonts.h"
#include "Parameters.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

namespace {
// Compact 680w header: icon+wordmark to 126, capsule 164, A/B, then the
// right cluster (seg 126, freeze, gear, meters) walking back from the edge.
constexpr int kCapsuleX = 126, kCapsuleW = 164, kCapsuleH = 30;

// Vector redraw of the Orbitum SVG mark (132x104 viewBox): a bone ring tilted
// -16° passing behind then in front of an ink-rimmed bone planet, with an
// ember comet streak and four-point spark. The SVG's grain filter is
// intentionally dropped — clean vector at header size.
void drawOrbitumMark(juce::Graphics& g, juce::Rectangle<float> area) {
    const float s = juce::jmin(area.getWidth() / 132.0f, area.getHeight() / 104.0f);
    juce::Graphics::ScopedSaveState save { g };
    g.addTransform(juce::AffineTransform::scale(s).translated(
        area.getX() + (area.getWidth() - 132.0f * s) / 2.0f,
        area.getY() + (area.getHeight() - 104.0f * s) / 2.0f));

    juce::Path ring;                              // annulus via even-odd fill
    ring.setUsingNonZeroWinding(false);
    ring.addEllipse(6.0f, 38.0f, 116.0f, 32.0f);  // rx 58, ry 16 about (64,54)
    ring.addEllipse(17.5f, 42.5f, 93.0f, 23.0f);  // rx 46.5, ry 11.5
    ring.applyTransform(juce::AffineTransform::rotation(
        juce::degreesToRadians(-16.0f), 64.0f, 54.0f));

    const auto ringHalf = [&] (juce::Rectangle<int> clip) {
        juce::Graphics::ScopedSaveState clipSave { g };
        g.reduceClipRegion(clip);
        g.setColour(orbit::gui::theme::bone50);
        g.fillPath(ring);
        g.setColour(orbit::gui::theme::ink900);
        g.strokePath(ring, juce::PathStrokeType { 1.3f });
    };

    ringHalf({ -60, -60, 260, 116 });             // behind the planet
    g.setColour(orbit::gui::theme::ink900);       // rim gap around the disc
    g.fillEllipse(37.5f, 27.5f, 53.0f, 53.0f);
    g.setColour(orbit::gui::theme::bone50);
    g.fillEllipse(40.0f, 30.0f, 48.0f, 48.0f);
    ringHalf({ -60, 54, 260, 120 });              // in front of the planet

    juce::Path comet;
    comet.startNewSubPath(9.0f, 7.0f);
    comet.cubicTo(17.0f, 12.0f, 23.0f, 17.0f, 29.0f, 22.5f);
    comet.lineTo(26.5f, 25.5f);
    comet.cubicTo(20.5f, 20.0f, 14.5f, 14.0f, 9.0f, 7.0f);
    comet.closeSubPath();
    juce::Path spark;
    spark.startNewSubPath(33.0f, 25.5f);
    spark.cubicTo(34.5f, 28.3f, 35.7f, 29.5f, 38.5f, 31.0f);
    spark.cubicTo(35.7f, 32.5f, 34.5f, 33.7f, 33.0f, 36.5f);
    spark.cubicTo(31.5f, 33.7f, 30.3f, 32.5f, 27.5f, 31.0f);
    spark.cubicTo(30.3f, 29.5f, 31.5f, 28.3f, 33.0f, 25.5f);
    spark.closeSubPath();
    g.setColour(orbit::gui::theme::ember);
    g.fillPath(comet);
    g.fillPath(spark);
}

// Round nav/slot buttons drawn flat in the header idiom. The default
// LookAndFeel unconditionally strokes a button outline with
// ComboBox::outlineColourId — transparent kills the ring so only the
// glyphs painted by OrbitHeader::paint show.
void styleRound(juce::TextButton& b) {
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    b.setColour(juce::TextButton::textColourOffId,
                orbit::gui::theme::bone50.withAlpha(0.6f));
}
} // namespace

OrbitHeader::OrbitHeader(OrbitAudioProcessor& proc)
    : proc_(proc),
      seg_({ "CLEAN", "TAPE", "GRIT" }),
      inMeter_([&proc] { return proc.vizFeed().readLevels().inPeak; }, false,
               theme::bone50.withAlpha(0.75f)),
      outMeter_([&proc] { return proc.vizFeed().readLevels().outPeak; }, false,
                theme::ember) {
    for (auto* b : { &prev_, &next_, &slotA_, &slotB_ }) {
        styleRound(*b);
        addAndMakeVisible(*b);
    }
    prev_.onClick = [this] { prevPreset(); };
    next_.onClick = [this] { nextPreset(); };
    slotA_.onClick = [this] { selectSlot(false); };
    slotB_.onClick = [this] { selectSlot(true); };

    addAndMakeVisible(seg_);
    charAtt_ = std::make_unique<juce::ParameterAttachment>(
        *proc.apvts.getParameter(orbit::params::kCharacterModeId),
        [this] (float v) { seg_.setSelectedIndex(int(v), juce::dontSendNotification); },
        nullptr);
    charAtt_->sendInitialUpdate();
    seg_.onChange = [this] (int i) {
        charAtt_->setValueAsCompleteGesture(float(i));
    };

    addAndMakeVisible(freeze_);
    freezeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, orbit::params::kFreezeId, freeze_);

    addAndMakeVisible(inMeter_);
    addAndMakeVisible(outMeter_);

    cachedPresets_ = allPresets();
    startTimerHz(4);   // PresetManager is poll-only by contract
}

juce::Array<orbit::PresetInfo> OrbitHeader::allPresets() const {
    auto list = proc_.presetManager().factoryPresets();
    list.addArray(proc_.presetManager().userPresets());
    return list;
}

int OrbitHeader::currentIndex(const juce::Array<orbit::PresetInfo>& list) const {
    const auto name = proc_.presetManager().currentPresetName();
    for (int i = 0; i < list.size(); ++i)
        if (list.getReference(i).name == name)
            return i;
    return -1;
}

void OrbitHeader::stepPreset(int delta) {
    cachedPresets_ = allPresets();   // pick up user presets saved mid-session
    const auto& list = cachedPresets_;
    if (list.isEmpty())
        return;
    const int cur = currentIndex(list);
    const int next = cur < 0 ? (delta > 0 ? 0 : list.size() - 1)
                             : ((cur + delta) % list.size() + list.size()) % list.size();
    proc_.presetManager().loadPreset(list.getReference(next));
    repaint();
}

void OrbitHeader::selectSlot(bool slotB) {
    if (proc_.presetManager().isSlotB() != slotB)
        proc_.presetManager().toggleAB();
    repaint();
}

void OrbitHeader::timerCallback() {
    const auto name = proc_.presetManager().currentPresetName();
    const bool dirty = name.isNotEmpty() && proc_.presetManager().isModified();
    const bool slotB = proc_.presetManager().isSlotB();
    if (name != shownPresetName_ || dirty != shownDirty_ || slotB != shownSlotB_) {
        shownPresetName_ = name;
        shownDirty_ = dirty;
        shownSlotB_ = slotB;
        cachedPresets_ = allPresets();   // state changed: re-enumerate once
        repaint();
    }
}

void OrbitHeader::resized() {
    prev_.setBounds(kCapsuleX + 2, (52 - 24) / 2, 24, 24);
    next_.setBounds(kCapsuleX + kCapsuleW - 26, (52 - 24) / 2, 24, 24);
    presetCapsule_ = { kCapsuleX + 30, (52 - kCapsuleH) / 2,
                       kCapsuleW - 60, kCapsuleH };
    slotA_.setBounds(kCapsuleX + kCapsuleW + 8, (52 - 24) / 2, 24, 24);
    slotB_.setBounds(kCapsuleX + kCapsuleW + 34, (52 - 24) / 2, 24, 24);

    // Right side: seg, freeze, gear slot, divider, meters (design order).
    const int W = theme::kWindowW;
    seg_.setBounds(W - 324, (52 - 28) / 2, 126, 28);
    freeze_.setBounds(W - 190, (52 - 26) / 2, 56, 26);
    gearBounds_ = { W - 104, (52 - 26) / 2, 26, 26 };
    inMeter_.setBounds(W - 46, 8, 5, 24);
    outMeter_.setBounds(W - 33, 8, 5, 24);
}

void OrbitHeader::mouseUp(const juce::MouseEvent& e) {
    if (presetCapsule_.contains(e.getPosition()) && onBrowserToggle != nullptr)
        onBrowserToggle();
    else if (gearBounds_.contains(e.getPosition()) && onSettingsToggle != nullptr)
        onSettingsToggle();
}

void OrbitHeader::paint(juce::Graphics& g) {
    const float W = float(getWidth()), H = float(getHeight());
    g.setGradientFill({ juce::Colour { 0xff0c0c14 }, 0.0f, 0.0f,
                        juce::Colour { 0xff090910 }, 0.0f, H, false });
    g.fillRect(0.0f, 0.0f, W, H);
    g.setColour(theme::bone50.withAlpha(0.05f));
    g.fillRect(0.0f, 0.0f, W, 1.0f);
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(0.0f, H - 1.0f, W, 1.0f);

    // Logo: the Orbitum planet mark, then a compact wordmark.
    drawOrbitumMark(g, { 12.0f, 9.0f, 44.0f, 34.0f });
    g.setFont(fonts::tracked(fonts::sansExtraBold(12.5f), 0.06f));
    g.setColour(theme::text());
    g.drawText("ORBITUM", juce::Rectangle<float>(60.0f, 18.0f, 64.0f, 16.0f),
               juce::Justification::centredLeft, false);

    // Preset capsule.
    {
        const auto cap = juce::Rectangle<float>(float(kCapsuleX), (H - kCapsuleH) / 2.0f,
                                                float(kCapsuleW), float(kCapsuleH));
        g.setColour(theme::bone50.withAlpha(0.04f));
        g.fillRoundedRectangle(cap, cap.getHeight() / 2.0f);
        g.setColour(theme::bone50.withAlpha(0.12f));
        g.drawRoundedRectangle(cap, cap.getHeight() / 2.0f, 1.0f);

        const int cur = currentIndex(cachedPresets_);   // no disk I/O in paint
        const auto num = cur < 0 ? juce::String("--")
                                 : juce::String(cur + 1).paddedLeft('0', 2);
        const auto name = shownPresetName_.isNotEmpty() ? shownPresetName_
                                                        : juce::String("Init");
        g.setFont(fonts::monoSemiBold(9.0f));
        g.setColour(theme::bone50.withAlpha(0.4f));
        g.drawText(num, cap.withX(cap.getX() + 30.0f).withWidth(18.0f),
                   juce::Justification::centredLeft, false);
        g.setFont(fonts::sansSemiBold(12.5f));
        g.setColour(theme::text());
        g.drawText(name, cap.withX(cap.getX() + 48.0f).withWidth(cap.getWidth() - 96.0f),
                   juce::Justification::centred, false);
        // Dirty dot + disclosure.
        g.setColour(shownDirty_ ? theme::ember : theme::bone50.withAlpha(0.12f));
        g.fillEllipse(cap.getRight() - 44.0f, cap.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setFont(fonts::mono(8.0f));
        g.setColour(theme::bone50.withAlpha(0.45f));
        g.drawText(juce::String::fromUTF8("\xe2\x96\xbe"),
                   cap.withX(cap.getRight() - 34.0f).withWidth(12.0f),
                   juce::Justification::centred, false);
    }

    // PRESETS caption under the capsule (same idiom as the logo tagline).
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.32f));
    g.setColour(theme::bone50.withAlpha(0.42f));
    g.drawText("PRESETS",
               juce::Rectangle<float>(float(kCapsuleX), 43.0f, float(kCapsuleW), 7.0f),
               juce::Justification::centred, false);

    // A/B: letters only — active slot reads in ember, inactive dim bone.
    const auto slotLetter = [&] (const juce::TextButton& b, const juce::String& letter,
                                 bool active) {
        g.setColour(active ? theme::ember : theme::bone50.withAlpha(0.5f));
        g.setFont(fonts::monoSemiBold(9.5f));
        g.drawText(letter, b.getBounds().toFloat(), juce::Justification::centred, false);
    };
    slotLetter(slotA_, "A", !shownSlotB_);
    slotLetter(slotB_, "B", shownSlotB_);

    // Preset arrows: vector chevrons (buttons are bare hit areas). Drawn as
    // paths, not font glyphs — Space Mono's coverage of the ‹ › guillemets
    // is unreliable and rendered them partially.
    {
        const auto chevron = [&] (const juce::TextButton& b, bool pointsLeft) {
            const auto c = b.getBounds().toFloat().getCentre();
            const float hw = 1.8f, hh = 3.6f;   // half-width / half-height
            const float tip = pointsLeft ? c.x - hw : c.x + hw;
            const float back = pointsLeft ? c.x + hw : c.x - hw;
            juce::Path p;
            p.startNewSubPath(back, c.y - hh);
            p.lineTo(tip, c.y);
            p.lineTo(back, c.y + hh);
            g.setColour(theme::bone50.withAlpha(0.6f));
            g.strokePath(p, juce::PathStrokeType { 1.6f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded });
        };
        chevron(prev_, true);
        chevron(next_, false);
    }

    // Settings gear: filled silhouette — eight rounded teeth around a body
    // ring with a punched axle hole. Composited in ONE transparency layer so
    // the tooth/ring overlaps don't stack alpha (they read as bright dashes
    // when filled separately at 0.55).
    {
        const auto c = gearBounds_.toFloat().getCentre();
        const float rBody = 5.6f, rHole = 2.3f;

        juce::Path teeth;
        for (int i = 0; i < 8; ++i) {
            const float a = juce::MathConstants<float>::pi * float(i) / 4.0f;
            juce::Path tooth;
            tooth.addRoundedRectangle(-1.5f, -(rBody + 2.4f), 3.0f, 3.4f, 1.1f);
            teeth.addPath(tooth, juce::AffineTransform::rotation(a)
                                     .translated(c.x, c.y));
        }
        juce::Path body;
        body.setUsingNonZeroWinding(false);   // outer minus hole = donut
        body.addEllipse(c.x - rBody, c.y - rBody, rBody * 2.0f, rBody * 2.0f);
        body.addEllipse(c.x - rHole, c.y - rHole, rHole * 2.0f, rHole * 2.0f);

        g.beginTransparencyLayer(0.55f);
        g.setColour(theme::bone50);
        g.fillPath(teeth);
        g.fillPath(body);
        g.endTransparencyLayer();
    }

    // Meter labels + right divider.
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(float(theme::kWindowW - 60), (H - 22.0f) / 2.0f, 1.0f, 22.0f);
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.2f));
    g.setColour(theme::bone50.withAlpha(0.4f));
    g.drawText("IN", juce::Rectangle<float>(float(theme::kWindowW - 50), 38.0f, 11.0f, 7.0f),
               juce::Justification::centred, false);
    g.drawText("OUT", juce::Rectangle<float>(float(theme::kWindowW - 38), 38.0f, 15.0f, 7.0f),
               juce::Justification::centred, false);
}
