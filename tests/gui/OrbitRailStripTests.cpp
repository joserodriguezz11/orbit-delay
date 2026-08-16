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

TEST_CASE("tap strip named children don't overlap at the pad-wide strip") {
    // Regression guard across re-bases: dot/sync/rev/pitch are siblings
    // within the same row (card), so their getBounds() share one coordinate
    // space and are directly comparable without translating into strip-space.
    // (The generic all-children sweep below covers the same ground; this one
    // names the controls so a failure reads immediately.)
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };
    strip.setBounds(0, 0, orbit::gui::theme::kPadW, orbit::gui::theme::kTapStripH);

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
    }
}

TEST_CASE("rail sections distribute evenly over the full column height") {
    // Fixed content is 361 design px; three equal gaps absorb the rest.
    // The full-height rail column (376) sits just under the fixed content +
    // 3 floored gaps; the floor eats decorative bottom padding, not content.
    CHECK(OrbitRail::sectionGap(float(orbit::gui::theme::kRailH)) == Approx(6.0f));
    // A taller column distributes the surplus across the three gaps.
    CHECK(OrbitRail::sectionGap(402.0f) == Approx((402.0f - 361.0f) / 3.0f));
    // Never collapses below the 6px floor, even in a too-short column.
    CHECK(OrbitRail::sectionGap(300.0f) == Approx(6.0f));
}

TEST_CASE("rail right column anchors to the right padding — no dead margin") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitRail rail { proc.apvts, [] { return 0.0f; } };
    rail.setBounds(0, 0, orbit::gui::theme::kRailW, orbit::gui::theme::kRailH);

    const int rightEdge = orbit::gui::theme::kRailW - 16;
    CHECK(rail.knob(OrbitRail::Knob::Duck).getRight() == rightEdge);
    CHECK(rail.knob(OrbitRail::Knob::ModRate).getRight() == rightEdge);
    CHECK(rail.knob(OrbitRail::Knob::HighCut).getRight() == rightEdge);
    CHECK(rail.pingPong().getRight() == rightEdge);
}

TEST_CASE("tap strip card children never overlap and the dot sits top-left") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    OrbitTapStrip strip { proc.apvts, [] { return 120.0; } };
    strip.setBounds(0, 0, orbit::gui::theme::kPadW, orbit::gui::theme::kTapStripH);

    int rowsChecked = 0;
    for (auto* row : strip.getChildren()) {
        std::vector<juce::Rectangle<int>> rects;
        bool cornerDot = false;
        for (auto* c : row->getChildren())
            if (c->isVisible() && !c->getBounds().isEmpty()) {
                rects.push_back(c->getBounds());
                // The enable dot lives in the card's top-left corner.
                if (c->getBounds() == juce::Rectangle<int>(8, 4, 16, 16))
                    cornerDot = true;
            }
        CHECK(cornerDot);
        for (size_t i = 0; i < rects.size(); ++i)
            for (size_t j = i + 1; j < rects.size(); ++j) {
                INFO("row " << rowsChecked << ": rect " << int(i) << " vs " << int(j));
                CHECK(!rects[i].intersects(rects[j]));
            }
        ++rowsChecked;
    }
    CHECK(rowsChecked == 4);
}
