#include "OrbitEditor.h"
#include "Parameters.h"

namespace theme = orbit::gui::theme;
using orbit::dsp::SyncDivision;

namespace {

// Nearest non-Free division to a time in ms at the given tempo.
int nearestDivision(float ms, double bpm) {
    int best = int(SyncDivision::Quarter);
    float bestD = 1.0e9f;
    for (int d = 1; d < int(SyncDivision::NumDivisions); ++d) {
        const float dms = orbit::dsp::divisionToSeconds(SyncDivision(d), bpm) * 1000.0f;
        const float diff = std::abs(dms - ms);
        if (diff < bestD) {
            bestD = diff;
            best = d;
        }
    }
    return best;
}

} // namespace

OrbitEditor::OrbitEditor(OrbitAudioProcessor& proc)
    : juce::AudioProcessorEditor(proc),
      proc_(proc),
      rail_(proc.apvts,
            [&proc] { return 1.0f - proc.vizFeed().readLevels().duckGain; }),
      strip_(proc.apvts, [&proc] { return proc.currentBpm(); }),
      header_(proc),
      browser_(proc.presetManager()) {
    addAndMakeVisible(pad_);
    addAndMakeVisible(rail_);
    addAndMakeVisible(strip_);
    addAndMakeVisible(header_);
    addChildComponent(browser_);   // hidden until the capsule opens it

    header_.onBrowserToggle = [this] {
        if (!browser_.isVisible())
            browser_.refresh();    // pick up user presets saved mid-session
        browser_.setVisible(!browser_.isVisible());
        browser_.toFront(false);
    };
    browser_.onClose = [this] { browser_.setVisible(false); };

    pad_.setEventSource(&proc.vizFeed());

    // Selection flows both ways through the editor.
    pad_.onSelect = [this] (int i) { setSelectedTap(i); };
    strip_.onSelect = [this] (int i) { setSelectedTap(i); };
    pad_.onOrbMove = [this] (int i, float x, float y) { writeOrb(i, x, y); };

    // Any tap parameter change refreshes that tap's pad view + strip colour.
    for (int i = 0; i < 4; ++i) {
        for (const auto& id : { orbit::params::tapTimeId(i),
                                orbit::params::tapFeedbackId(i),
                                orbit::params::tapSyncId(i),
                                orbit::params::tapEnabledId(i),
                                orbit::params::tapReverseId(i) })
            viewAtts_.push_back(std::make_unique<juce::ParameterAttachment>(
                *proc.apvts.getParameter(id),
                [this, i] (float) { refreshTap(i); }, nullptr));
    }
    viewAtts_.push_back(std::make_unique<juce::ParameterAttachment>(
        *proc.apvts.getParameter(orbit::params::kCharacterModeId),
        [this] (float v) {
            pad_.setCharacter(orbit::dsp::CharacterStage::Mode(int(v)));
            for (int i = 0; i < 4; ++i)
                refreshTap(i);   // character retints every tap colour
        },
        nullptr));
    viewAtts_.push_back(std::make_unique<juce::ParameterAttachment>(
        *proc.apvts.getParameter(orbit::params::kFreezeId),
        [this] (float v) { pad_.setFreeze(v > 0.5f); }, nullptr));

    for (auto& att : viewAtts_)
        att->sendInitialUpdate();
    refreshSyncGrid();

    // Slow tick: host tempo changes move the sync grid without a param edit.
    startTimer(500);

    setResizable(true, true);
    constrainer_.setFixedAspectRatio(double(kBaseW) / double(kBaseH));
    constrainer_.setSizeLimits(kBaseW, kBaseH, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer_);
    setSize(kBaseW, kBaseH);
}

OrbitEditor::~OrbitEditor() = default;

void OrbitEditor::timerCallback() {
    refreshSyncGrid();
}

void OrbitEditor::setSelectedTap(int i) {
    pad_.setSelected(i);
    strip_.setSelected(i);
    refreshSyncGrid();
}

void OrbitEditor::refreshTap(int i) {
    auto& apvts = proc_.apvts;
    const auto rawf = [&apvts] (const juce::String& id) {
        return apvts.getRawParameterValue(id)->load();
    };

    const int div = int(rawf(orbit::params::tapSyncId(i)));
    const bool synced = div != int(SyncDivision::Free);
    const float ms = synced
        ? orbit::dsp::divisionToSeconds(SyncDivision(div), proc_.currentBpm()) * 1000.0f
        : rawf(orbit::params::tapTimeId(i));

    OrbPad::TapView view;
    view.on = rawf(orbit::params::tapEnabledId(i)) > 0.5f;
    view.x = theme::msToX(ms);
    view.y = juce::jlimit(0.0f, 1.0f, rawf(orbit::params::tapFeedbackId(i)) / 0.95f);
    view.synced = synced;
    view.reversed = rawf(orbit::params::tapReverseId(i)) > 0.5f;
    pad_.setTap(i, view);

    const auto& ch = theme::characterTheme(orbit::dsp::CharacterStage::Mode(
        int(rawf(orbit::params::kCharacterModeId))));
    strip_.setTapAccent(i, theme::lchColour(
        theme::tapLch(view.x, view.y, theme::kDefaultWarmField, ch), 1.0f));
}

void OrbitEditor::refreshSyncGrid() {
    // Gridlines at every real division that lands inside the pad's ms range.
    std::vector<float> xs;
    const double bpm = proc_.currentBpm();
    for (int d = 1; d < int(SyncDivision::NumDivisions); ++d) {
        const float ms = orbit::dsp::divisionToSeconds(SyncDivision(d), bpm) * 1000.0f;
        if (ms >= 40.0f && ms <= 900.0f)
            xs.push_back(theme::msToX(ms));
    }
    pad_.setSyncGridX(std::move(xs));
}

void OrbitEditor::writeOrb(int i, float x, float y) {
    auto& apvts = proc_.apvts;

    auto* fbParam = apvts.getParameter(orbit::params::tapFeedbackId(i));
    fbParam->setValueNotifyingHost(fbParam->convertTo0to1(y * 0.95f));

    const int div = int(apvts.getRawParameterValue(orbit::params::tapSyncId(i))->load());
    if (div != int(SyncDivision::Free)) {
        // Synced taps ride the division grid; free time stays untouched.
        const int nearest = nearestDivision(theme::msOfX(x), proc_.currentBpm());
        if (nearest != div) {
            auto* syncParam = apvts.getParameter(orbit::params::tapSyncId(i));
            syncParam->setValueNotifyingHost(syncParam->convertTo0to1(float(nearest)));
        }
    } else {
        auto* timeParam = apvts.getParameter(orbit::params::tapTimeId(i));
        timeParam->setValueNotifyingHost(timeParam->convertTo0to1(theme::msOfX(x)));
    }
}

void OrbitEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::panel);
}

void OrbitEditor::resized() {
    // Children live in design pixels; one transform scales everything.
    const auto s = float(scale());
    const auto placeScaled = [s] (juce::Component& c, int x, int y, int w, int h) {
        c.setTransform(juce::AffineTransform::scale(s));
        c.setBounds(x, y, w, h);
    };
    placeScaled(pad_, 0, theme::kHeaderH, theme::kPadW, theme::kPadH);
    placeScaled(rail_, theme::kPadW, theme::kHeaderH, theme::kRailW, theme::kPadH);
    placeScaled(strip_, 0, theme::kHeaderH + theme::kPadH, theme::kWindowW, theme::kTapStripH);
    placeScaled(header_, 0, 0, theme::kWindowW, theme::kHeaderH);
    placeScaled(browser_, 0, 0, theme::kWindowW, theme::kWindowH);
}
