// v2 control set: OrbitKnob (arc knob with spring settle + design drag
// gestures), OrbitPill (SYNC/REV/FREEZE), OrbitSwitch (P-PONG), OrbitSeg
// (CLEAN/TAPE/GRIT) and OrbitMeter (peak-hold level bar). Interaction math
// is tested through pure helpers; painting through ink-on-image smoke tests
// (mirrors OrbitLookAndFeelTests).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbitControls.h"

using Catch::Approx;

namespace {
bool imageHasInk(const juce::Image& img) {
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() > 0)
                return true;
    return false;
}

juce::Image paintToImage(juce::Component& c) {
    juce::Image img { juce::Image::ARGB, c.getWidth(), c.getHeight(), true };
    juce::Graphics g { img };
    c.paintEntireComponent(g, true);
    return img;
}
} // namespace

// ------------------------------------------------------------ OrbitKnob

TEST_CASE("OrbitKnob drag maps 180px to the full range with 0.22 fine mode") {
    // Mockup: nv = sv + (dyUp * range / 180) * (shift ? 0.22 : 1), snapped.
    CHECK(OrbitKnob::draggedValue(50.0, 90.0, 0.0, 100.0, 1.0, false)
          == Approx(100.0));   // half the drag span = half the range
    CHECK(OrbitKnob::draggedValue(50.0, -90.0, 0.0, 100.0, 1.0, false)
          == Approx(0.0));
    CHECK(OrbitKnob::draggedValue(50.0, 90.0, 0.0, 100.0, 1.0, true)
          == Approx(61.0));    // 50 + 50*0.22 = 61
    // Snaps to step and clamps.
    CHECK(OrbitKnob::draggedValue(0.0, 900.0, 0.0, 100.0, 5.0, false)
          == Approx(100.0));
    CHECK(OrbitKnob::draggedValue(0.35, 9.0, 0.1, 8.0, 0.05, false)
          == Approx(0.75).margin(1e-9)); // 0.35+0.395 snapped to 0.05 grid
}

TEST_CASE("OrbitKnob wheel steps range/80 coarse and one step fine") {
    // Mockup: inc = shift ? step : max(step, range/80); minus sign of deltaY.
    CHECK(OrbitKnob::wheeledValue(50.0, 1.0, 0.0, 100.0, 1.0, false)
          == Approx(49.0).margin(0.3));  // down-scroll decreases
    CHECK(OrbitKnob::wheeledValue(50.0, -1.0, 0.0, 100.0, 1.0, true)
          == Approx(51.0));
}

TEST_CASE("OrbitKnob double-click returns to the default value") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitKnob knob;
    knob.setRange(0.0, 100.0, 1.0);
    knob.setKnobDefault(35.0);
    knob.setValue(80.0, juce::dontSendNotification);
    knob.resetToDefault();
    CHECK(knob.getValue() == Approx(35.0));
}

TEST_CASE("OrbitKnob spring settles the displayed proportion onto the target") {
    // Design springFeel default 2 -> stiff spring; a dozen 60fps ticks lands it.
    OrbitKnob::Spring s;
    s.snapTo(0.0f);
    for (int i = 0; i < 60; ++i) s.tick(1.0f, 1.0f / 60.0f);
    CHECK(s.position() == Approx(1.0f).margin(0.002f));
    // And an in-flight tick sits strictly between start and target.
    OrbitKnob::Spring mid;
    mid.snapTo(0.0f);
    mid.tick(1.0f, 1.0f / 60.0f);
    CHECK(mid.position() > 0.0f);
    CHECK(mid.position() < 1.0f);
}

TEST_CASE("OrbitKnob paints ink at every design size") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    for (int size : { 34, 44, 56 }) {
        OrbitKnob knob;
        knob.setRange(0.0, 100.0, 1.0);
        knob.setValue(48.0, juce::dontSendNotification);
        knob.setSize(size, size);
        CHECK(imageHasInk(paintToImage(knob)));
    }
}

// ------------------------------------------------------------- OrbitPill

TEST_CASE("OrbitPill is a click-toggling button and paints both states") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitPill pill { "SYNC" };
    pill.setSize(52, 20);
    // Clicking must flip the state — that behaviour is JUCE's, gated on this flag.
    CHECK(pill.getClickingTogglesState());
    CHECK_FALSE(pill.getToggleState());
    CHECK(imageHasInk(paintToImage(pill)));           // off: outline + label
    pill.setToggleState(true, juce::dontSendNotification);
    CHECK(imageHasInk(paintToImage(pill)));           // on: accent fill
}

// ----------------------------------------------------------- OrbitSwitch

TEST_CASE("OrbitSwitch reports the v2 40x22 footprint and paints ink") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitSwitch sw;
    CHECK(OrbitSwitch::kWidth == 40);
    CHECK(OrbitSwitch::kHeight == 22);
    sw.setSize(OrbitSwitch::kWidth, OrbitSwitch::kHeight);
    sw.setToggleState(true, juce::dontSendNotification);
    CHECK(imageHasInk(paintToImage(sw)));
}

// -------------------------------------------------------------- OrbitSeg

TEST_CASE("OrbitSeg selects segments and notifies with the new index") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitSeg seg { { "CLEAN", "TAPE", "GRIT" } };
    seg.setSize(160, 28);
    int notified = -1;
    seg.onChange = [&] (int i) { notified = i; };
    seg.setSelectedIndex(1, juce::sendNotification);
    CHECK(notified == 1);
    CHECK(seg.selectedIndex() == 1);
    // Silent set does not notify.
    notified = -1;
    seg.setSelectedIndex(2, juce::dontSendNotification);
    CHECK(notified == -1);
    CHECK(seg.selectedIndex() == 2);
    CHECK(imageHasInk(paintToImage(seg)));
}

// ------------------------------------------------------------ OrbitMeter

TEST_CASE("meter peak-hold rides up instantly, holds 380ms, then decays") {
    OrbitMeter::PeakState st;
    // Rising signal: peak tracks it immediately.
    OrbitMeter::stepPeak(st, 0.8f, 1.0, 1.0f / 60.0f);
    CHECK(st.peak == Approx(0.8f));
    // Signal drops: within the hold window the peak stays.
    OrbitMeter::stepPeak(st, 0.1f, 1.2, 1.0f / 60.0f);
    CHECK(st.peak == Approx(0.8f));
    // After 380ms the peak decays at 1.4/s.
    OrbitMeter::stepPeak(st, 0.1f, 1.5, 1.0f / 60.0f);
    CHECK(st.peak < 0.8f);
    CHECK(st.peak == Approx(0.8f - 1.4f / 60.0f).margin(1e-4));
}

TEST_CASE("OrbitMeter paints ink from its level getter") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitMeter meter { [] { return 0.7f; } };
    meter.setSize(5, 26);
    meter.refreshNow();  // pull a level outside the vblank loop
    CHECK(imageHasInk(paintToImage(meter)));
}
