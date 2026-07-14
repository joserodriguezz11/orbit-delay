// Right rail (BLEND/SPACE/MOTION/FILTER) + tap strip (4 rows): every control
// binds to the real parameter layout through attachments, and value text
// follows the mockup's formatters against the engine's actual ranges.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestProcessor.h"
#include "gui/OrbitRail.h"
#include "gui/OrbitTapStrip.h"

using Catch::Approx;

namespace {
float raw(TestProcessor& p, const juce::String& id) {
    return p.apvts.getRawParameterValue(id)->load();
}
void setParam(TestProcessor& p, const juce::String& id, float plainValue) {
    auto* param = p.apvts.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(plainValue));
}
} // namespace

// ------------------------------------------------------------------- rail

TEST_CASE("rail knobs bind both directions to the mix/motion/filter params") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitRail rail { proc.apvts, [] { return 0.0f; } };

    // Knob -> parameter.
    rail.knob(OrbitRail::Knob::Mix).setValue(0.75, juce::sendNotificationSync);
    CHECK(raw(proc, "mix_drywet") == Approx(0.75f));
    // Parameter -> knob.
    setParam(proc, "mod_rate", 4.0f);
    CHECK(rail.knob(OrbitRail::Knob::ModRate).getValue() == Approx(4.0));
    // Switch -> parameter.
    rail.pingPong().setToggleState(true, juce::sendNotification);
    CHECK(raw(proc, "mix_pingpong") == Approx(1.0f));
}

TEST_CASE("rail value text follows the mockup formatters on engine ranges") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitRail rail { proc.apvts, [] { return 0.0f; } };

    CHECK(rail.knob(OrbitRail::Knob::Mix).getTextFromValue(0.48) == "48%");
    CHECK(rail.knob(OrbitRail::Knob::Duck).getTextFromValue(0.28) == "28%");
    // Width: engine 0..2 shown as 0..100%.
    CHECK(rail.knob(OrbitRail::Knob::Width).getTextFromValue(1.4) == "70%");
    CHECK(rail.knob(OrbitRail::Knob::Width).getTextFromValue(2.0) == "100%");
    // Rate: two decimals under 2, one above.
    CHECK(rail.knob(OrbitRail::Knob::ModRate).getTextFromValue(0.8) == "0.80");
    CHECK(rail.knob(OrbitRail::Knob::ModRate).getTextFromValue(3.5) == "3.5");
    // Low cut in plain Hz, high cut in k.
    CHECK(rail.knob(OrbitRail::Knob::LowCut).getTextFromValue(160.0) == "160");
    CHECK(rail.knob(OrbitRail::Knob::HighCut).getTextFromValue(8600.0) == "8.6k");
    CHECK(rail.knob(OrbitRail::Knob::HighCut).getTextFromValue(14000.0) == "14k");
}

TEST_CASE("rail paints ink") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitRail rail { proc.apvts, [] { return 0.3f; } };
    rail.setSize(orbit::gui::theme::kRailW, orbit::gui::theme::kPadH);
    rail.resized();
    juce::Image img { juce::Image::ARGB, rail.getWidth(), rail.getHeight(), true };
    juce::Graphics g { img };
    rail.paintEntireComponent(g, true);
    bool ink = false;
    for (int y = 0; y < img.getHeight() && !ink; y += 5)
        for (int x = 0; x < img.getWidth() && !ink; x += 5)
            ink = img.getPixelAt(x, y).getAlpha() > 0;
    CHECK(ink);
}

// -------------------------------------------------------------- tap strip

TEST_CASE("tap rows bind dot/pitch/reverse to their tap params") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };

    // Enable dot -> tap1_enabled.
    strip.dot(0).setToggleState(true, juce::sendNotification);
    CHECK(raw(proc, "tap1_enabled") == Approx(1.0f));
    // Pitch knob two-way on tap 3.
    strip.pitchKnob(2).setValue(7.0, juce::sendNotificationSync);
    CHECK(raw(proc, "tap3_pitch") == Approx(7.0f));
    CHECK(strip.pitchKnob(2).getTextFromValue(7.0) == "+7");
    // REV pill -> tap2_reverse.
    strip.revPill(1).setToggleState(true, juce::sendNotification);
    CHECK(raw(proc, "tap2_reverse") == Approx(1.0f));
}

TEST_CASE("sync pill maps the boolean gesture onto the division choice") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };

    // 500ms at 120bpm is exactly a quarter note.
    setParam(proc, "tap1_time", 500.0f);
    strip.setSyncEnabled(0, true);
    const int quarterIndex = int(orbit::dsp::SyncDivision::Quarter);
    CHECK(int(raw(proc, "tap1_sync")) == quarterIndex);
    CHECK(strip.timeText(0) == "1/4");

    // Turning sync off returns to Free and keeps the effective time.
    strip.setSyncEnabled(0, false);
    CHECK(int(raw(proc, "tap1_sync")) == int(orbit::dsp::SyncDivision::Free));
    CHECK(raw(proc, "tap1_time") == Approx(500.0f).margin(0.5f));
    CHECK(strip.timeText(0) == "500ms");
}

TEST_CASE("selecting a row notifies once with the row index") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };
    int selected = -1;
    strip.onSelect = [&] (int i) { selected = i; };
    strip.selectRow(3);
    CHECK(selected == 3);
}

TEST_CASE("tap strip pills and pitch knob don't overlap at the 900px window width") {
    // Regression guard for the 900x550 window rebase: each card shrank from
    // 281px to ~211px and a fixed pillX offset let the PITCH knob overlap
    // the SYNC/REV pills by 11px. dot/sync/rev/pitch are siblings within the
    // same row (card), so their getBounds() share one coordinate space and
    // are directly comparable without translating into strip-space.
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };
    strip.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kTapStripH);

    for (int i = 0; i < 4; ++i) {
        const std::vector<juce::Rectangle<int>> rects {
            strip.dot(i).getBounds(),
            strip.syncPill(i).getBounds(),
            strip.revPill(i).getBounds(),
            strip.pitchKnob(i).getBounds(),
        };
        for (size_t a = 0; a < rects.size(); ++a)
            for (size_t b = a + 1; b < rects.size(); ++b) {
                INFO("tap " << i << ": rect " << int(a) << " vs " << int(b));
                CHECK_FALSE(rects[a].intersects(rects[b]));
            }
        // >=6px clearance between the pills' right edge and the knob's left
        // edge — the specific gap that regressed.
        INFO("tap " << i);
        CHECK(strip.pitchKnob(i).getX() - strip.syncPill(i).getRight() >= 6);
        CHECK(strip.pitchKnob(i).getX() - strip.revPill(i).getRight() >= 6);
    }
}

TEST_CASE("rail sections distribute evenly over the full column height") {
    // Fixed content is 361 design px; three equal gaps absorb the rest.
    CHECK(OrbitRail::sectionGap(402.0f) == Approx((402.0f - 361.0f) / 3.0f));
    // Never collapses below the 6px floor, even in a too-short column.
    CHECK(OrbitRail::sectionGap(300.0f) == Approx(6.0f));
}
