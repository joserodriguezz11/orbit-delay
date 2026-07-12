#include "OrbitHeader.h"
#include "OrbitFonts.h"
#include "Parameters.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

namespace {
constexpr int kCapsuleX = 130, kCapsuleW = 246, kCapsuleH = 30;

// Round nav/slot buttons drawn flat in the header idiom.
void styleRound(juce::TextButton& b) {
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
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
    const auto list = allPresets();
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
        repaint();
    }
}

void OrbitHeader::resized() {
    prev_.setBounds(kCapsuleX + 4, (52 - 24) / 2, 24, 24);
    next_.setBounds(kCapsuleX + kCapsuleW - 28, (52 - 24) / 2, 24, 24);
    presetCapsule_ = { kCapsuleX + 30, (52 - kCapsuleH) / 2,
                       kCapsuleW - 60, kCapsuleH };
    slotA_.setBounds(kCapsuleX + kCapsuleW + 10, (52 - 24) / 2, 24, 24);
    slotB_.setBounds(kCapsuleX + kCapsuleW + 38, (52 - 24) / 2, 24, 24);

    // Right side, leaving room for the meters block (design order).
    const int W = theme::kWindowW;
    seg_.setBounds(W - 290 - 156, (52 - 28) / 2, 156, 28);
    freeze_.setBounds(W - 290 + 10, (52 - 26) / 2, 66, 26);
    inMeter_.setBounds(W - 46, 8, 5, 26);
    outMeter_.setBounds(W - 33, 8, 5, 26);
}

void OrbitHeader::mouseUp(const juce::MouseEvent& e) {
    if (presetCapsule_.contains(e.getPosition()) && onBrowserToggle != nullptr)
        onBrowserToggle();
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

    // Logo: two rings + red core, then the stretched wordmark.
    g.setColour(theme::bone50.withAlpha(0.85f));
    g.drawEllipse(20.0f, 16.0f, 20.0f, 20.0f, 1.4f);
    g.setColour(theme::bone50.withAlpha(0.26f));
    g.drawEllipse(24.0f, 20.0f, 12.0f, 12.0f, 1.0f);
    g.setColour(theme::ember);
    g.fillEllipse(28.0f, 24.0f, 4.0f, 4.0f);
    {
        juce::Graphics::ScopedSaveState save { g };
        g.addTransform(juce::AffineTransform::scale(1.18f, 1.0f, 51.0f, 0.0f));
        g.setFont(fonts::tracked(fonts::sansExtraBold(16.0f), 0.06f));
        g.setColour(theme::text());
        g.drawText("ORBIT", juce::Rectangle<float>(51.0f, 15.0f, 90.0f, 16.0f),
                   juce::Justification::centredLeft, false);
    }
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.32f));
    g.setColour(theme::bone50.withAlpha(0.42f));
    g.drawText("4-TAP ECHO", juce::Rectangle<float>(51.0f, 33.0f, 90.0f, 7.0f),
               juce::Justification::centredLeft, false);

    // Preset capsule.
    {
        const auto cap = juce::Rectangle<float>(float(kCapsuleX), (H - kCapsuleH) / 2.0f,
                                                float(kCapsuleW), float(kCapsuleH));
        g.setColour(theme::bone50.withAlpha(0.04f));
        g.fillRoundedRectangle(cap, cap.getHeight() / 2.0f);
        g.setColour(theme::bone50.withAlpha(0.12f));
        g.drawRoundedRectangle(cap, cap.getHeight() / 2.0f, 1.0f);

        const auto list = allPresets();
        const int cur = currentIndex(list);
        const auto num = cur < 0 ? juce::String("--")
                                 : juce::String(cur + 1).paddedLeft('0', 2);
        const auto name = shownPresetName_.isNotEmpty() ? shownPresetName_
                                                        : juce::String("Init");
        g.setFont(fonts::monoSemiBold(9.0f));
        g.setColour(theme::bone50.withAlpha(0.4f));
        g.drawText(num, cap.withX(cap.getX() + 34.0f).withWidth(20.0f),
                   juce::Justification::centredLeft, false);
        g.setFont(fonts::sansSemiBold(12.5f));
        g.setColour(theme::text());
        g.drawText(name, cap.withX(cap.getX() + 58.0f).withWidth(cap.getWidth() - 116.0f),
                   juce::Justification::centred, false);
        // Dirty dot + disclosure.
        g.setColour(shownDirty_ ? theme::ember : theme::bone50.withAlpha(0.12f));
        g.fillEllipse(cap.getRight() - 52.0f, cap.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setFont(fonts::mono(8.0f));
        g.setColour(theme::bone50.withAlpha(0.45f));
        g.drawText(juce::String::fromUTF8("\xe2\x96\xbe"),
                   cap.withX(cap.getRight() - 42.0f).withWidth(14.0f),
                   juce::Justification::centred, false);
    }

    // A/B active states ride on top of the flat TextButtons.
    const auto slotRing = [&] (const juce::TextButton& b, bool active) {
        const auto r = b.getBounds().toFloat();
        if (active) {
            g.setColour(theme::ember);
            g.fillEllipse(r);
            g.setColour(theme::text());
        } else {
            g.setColour(theme::bone50.withAlpha(0.16f));
            g.drawEllipse(r.reduced(0.5f), 1.0f);
            g.setColour(theme::bone50.withAlpha(0.5f));
        }
        g.setFont(fonts::monoSemiBold(9.5f));
        g.drawText(b.getButtonText(), r, juce::Justification::centred, false);
    };
    slotRing(slotA_, !shownSlotB_);
    slotRing(slotB_, shownSlotB_);

    // Meter labels + right divider.
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(float(theme::kWindowW - 60), (H - 22.0f) / 2.0f, 1.0f, 22.0f);
    g.setFont(fonts::tracked(fonts::mono(6.5f), 0.2f));
    g.setColour(theme::bone50.withAlpha(0.4f));
    g.drawText("IN", juce::Rectangle<float>(float(theme::kWindowW - 50), 37.0f, 13.0f, 7.0f),
               juce::Justification::centred, false);
    g.drawText("OUT", juce::Rectangle<float>(float(theme::kWindowW - 38), 37.0f, 15.0f, 7.0f),
               juce::Justification::centred, false);
}
