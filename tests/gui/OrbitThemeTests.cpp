#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbitTheme.h"
#include "dsp/CharacterStage.h"

using Catch::Approx;
using orbit::dsp::CharacterStage;
namespace theme = orbit::gui::theme;

// ---------------------------------------------------------------- layout

TEST_CASE("v2 layout metrics partition the 1180x720 window exactly") {
    CHECK(theme::kWindowW == 1180);
    CHECK(theme::kWindowH == 720);
    // Vertical: header + orb pad + tap strip fill the window.
    CHECK(theme::kHeaderH + theme::kPadH + theme::kTapStripH == theme::kWindowH);
    // Horizontal: orb pad + right rail fill the window.
    CHECK(theme::kPadW + theme::kRailW == theme::kWindowW);
    // Values pinned to the v2 mockup.
    CHECK(theme::kHeaderH == 52);
    CHECK(theme::kPadW == 990);
    CHECK(theme::kPadH == 572);
    CHECK(theme::kTapStripH == 96);
}

TEST_CASE("v2 type families are Archivo / IBM Plex Mono / Syne watermark") {
    CHECK(juce::String(theme::fontSans) == "Archivo");
    CHECK(juce::String(theme::fontMono) == "IBM Plex Mono");
    CHECK(juce::String(theme::fontWatermark) == "Syne");
}

// ------------------------------------------------------- character themes

TEST_CASE("characterTheme carries the v2 field values per mode") {
    const auto clean = theme::characterTheme(CharacterStage::Mode::Clean);
    const auto tape  = theme::characterTheme(CharacterStage::Mode::Tape);
    const auto grit  = theme::characterTheme(CharacterStage::Mode::Grit);

    // Distinct field backgrounds per character.
    CHECK(clean.bgTop != tape.bgTop);
    CHECK(tape.bgTop != grit.bgTop);

    // Design constants: tape glows warmer/softer, grit adds scanlines.
    CHECK(clean.glow == Approx(1.0f));
    CHECK(tape.glow == Approx(1.3f));
    CHECK(grit.glow == Approx(0.85f));
    CHECK(clean.scan == Approx(0.0f));
    CHECK(tape.scan == Approx(0.0f));
    CHECK(grit.scan == Approx(0.45f));
    CHECK(tape.hueShift == Approx(16.0f));
    CHECK(grit.hueShift == Approx(-8.0f));
    CHECK(tape.sat == Approx(0.85f));
    CHECK(grit.sat == Approx(1.18f));
    CHECK(tape.soft == Approx(1.6f));
}

// ----------------------------------------------------- position -> colour

TEST_CASE("posLch warm mode maps pad position to the design OKLCH ramps") {
    // x=0, y=0: L = 0.55, C = 0.24, H = 29.
    const auto o = theme::posLch(0.0f, 0.0f, /*warm*/ true);
    CHECK(o.l == Approx(0.55f));
    CHECK(o.c == Approx(0.24f));
    CHECK(o.h == Approx(29.0f));
    // x=1 pushes hue to 29+61=90 and lightness up by 0.10.
    const auto r = theme::posLch(1.0f, 0.0f, true);
    CHECK(r.h == Approx(90.0f));
    CHECK(r.l == Approx(0.65f));
    // Warm mode ignores y for hue.
    CHECK(theme::posLch(0.5f, 0.9f, true).h == Approx(theme::posLch(0.5f, 0.1f, true).h));
}

TEST_CASE("posLch full mode swings hue with y and wraps below zero") {
    // x=0, y=0.5: h = 29 - 49.5 = -20.5 -> wraps to 339.5.
    const auto o = theme::posLch(0.0f, 0.5f, /*warm*/ false);
    CHECK(o.h == Approx(339.5f));
}

TEST_CASE("tapLch applies the character sat multiplier and hue shift") {
    const auto tape = theme::characterTheme(CharacterStage::Mode::Tape);
    const auto base = theme::posLch(0.0f, 0.0f, true);
    const auto out  = theme::tapLch(0.0f, 0.0f, true, tape);
    CHECK(out.h == Approx(base.h + 16.0f));
    CHECK(out.c == Approx(base.c * 0.85f));
    CHECK(out.l == Approx(base.l));
}

TEST_CASE("lchColour converts OKLCH to sRGB within 8-bit tolerance") {
    // Known anchor: sRGB pure red is oklch(0.628 0.2577 29.23).
    const auto red = theme::lchColour({0.628f, 0.2577f, 29.23f}, 1.0f);
    CHECK(int(red.getRed())   >= 253);
    CHECK(int(red.getGreen()) <= 3);
    CHECK(int(red.getBlue())  <= 3);
    // Achromatic extremes.
    const auto white = theme::lchColour({1.0f, 0.0f, 0.0f}, 1.0f);
    CHECK(int(white.getRed()) >= 253);
    CHECK(int(white.getGreen()) >= 253);
    CHECK(int(white.getBlue()) >= 253);
    const auto black = theme::lchColour({0.0f, 0.0f, 0.0f}, 1.0f);
    CHECK(int(black.getRed()) <= 2);
    // Alpha passes straight through.
    CHECK(theme::lchColour({0.5f, 0.1f, 180.0f}, 0.5f).getFloatAlpha()
          == Approx(0.5f).margin(0.005f));
}

// --------------------------------------------------------- time mapping

TEST_CASE("pad time mapping is the design log curve 40ms..900ms") {
    CHECK(theme::msOfX(0.0f) == Approx(40.0f));
    CHECK(theme::msOfX(1.0f) == Approx(900.0f));
    // Round-trips through the inverse.
    CHECK(theme::msToX(theme::msOfX(0.37f)) == Approx(0.37f).margin(1e-4));
    // Inverse clamps out-of-range parameter values to the pad edges.
    CHECK(theme::msToX(1.0f) == Approx(0.0f));
    CHECK(theme::msToX(2000.0f) == Approx(1.0f));
}

// -------------------------------------------------------------- scales

TEST_CASE("ink and bone scales are ordered dark-to-light by luminance") {
    CHECK(theme::ink900.getPerceivedBrightness() < theme::ink600.getPerceivedBrightness());
    CHECK(theme::bone300.getPerceivedBrightness() < theme::bone50.getPerceivedBrightness());
}
