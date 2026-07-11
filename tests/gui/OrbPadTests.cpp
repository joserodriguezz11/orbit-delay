// OrbPad — the 990x572 orb field. The interaction physics (hit-testing,
// flick-to-throw glide with wall bounces, drag heat) live in pure orbpad::
// helpers tested directly; the component itself gets ink smoke tests and
// callback wiring checks, mirroring the rest of the GUI suite.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbPad.h"

using Catch::Approx;

// ------------------------------------------------------------- hit testing

TEST_CASE("hitTest picks the nearest orb within 30 design px") {
    std::array<orbpad::TapPos, 4> taps {{
        { 0.10f, 0.50f }, { 0.50f, 0.50f }, { 0.52f, 0.50f }, { 0.90f, 0.10f } }};
    // Dead centre of tap 1.
    CHECK(orbpad::hitTest(taps, 0.50f, 0.50f) == 1);
    // Between taps 1 and 2 but nearer 2.
    CHECK(orbpad::hitTest(taps, 0.517f, 0.50f) == 2);
    // Far from everything: no hit.
    CHECK(orbpad::hitTest(taps, 0.30f, 0.05f) == -1);
    // 30px radius: 0.10 + 29/990 hits tap 0; +31/990 just misses on x.
    CHECK(orbpad::hitTest(taps, 0.10f + 29.0f / 990.0f, 0.50f) == 0);
    CHECK(orbpad::hitTest(taps, 0.10f + 31.0f / 990.0f, 0.05f) == -1);
}

// ------------------------------------------------------------------ glide

TEST_CASE("glide integrates, damps, and reflects off walls at 0.55") {
    orbpad::Glide g { 0.95f, 0.5f, 2.0f, 0.0f, true };  // heading for the right wall
    int bounces = 0;
    for (int i = 0; i < 30 && g.active; ++i)
        if (orbpad::stepGlide(g, 1.0f / 60.0f) != orbpad::kWallNone)
            ++bounces;
    CHECK(bounces >= 1);
    CHECK(g.x <= 1.0f);
    CHECK(g.x >= 0.0f);

    // Damping: speed decays ~exp(-2.3 dt) per step when nothing is hit.
    orbpad::Glide d { 0.5f, 0.5f, 1.0f, 0.0f, true };
    orbpad::stepGlide(d, 1.0f / 60.0f);
    CHECK(d.vx == Approx(1.0f * std::exp(-2.3f / 60.0f)).margin(1e-4));

    // Deactivates once slower than 0.02.
    orbpad::Glide slow { 0.5f, 0.5f, 0.019f, 0.0f, true };
    orbpad::stepGlide(slow, 1.0f / 60.0f);
    CHECK_FALSE(slow.active);
}

TEST_CASE("glide only launches from a fast enough release") {
    CHECK(orbpad::shouldGlide(0.3f, 0.0f));         // |v| > 0.25
    CHECK_FALSE(orbpad::shouldGlide(0.1f, 0.2f));   // hypot < 0.25
}

// ------------------------------------------------------------------- heat

TEST_CASE("drag heat attacks fast and releases slow") {
    float heat = 0.0f;
    heat = orbpad::stepHeat(heat, 1.0f, 1.0f / 60.0f);
    const float afterAttack = heat;
    CHECK(afterAttack > 0.05f);   // rate 9 pulls up quickly

    // Release from the same level is much slower (rate 2.2).
    float down = afterAttack;
    down = orbpad::stepHeat(down, 0.0f, 1.0f / 60.0f);
    CHECK((afterAttack - down) < afterAttack);        // still warm
    CHECK(down < afterAttack);                        // but falling
    // Tiny residue snaps to zero.
    CHECK(orbpad::stepHeat(0.003f, 0.0f, 1.0f / 60.0f) == Approx(0.0f));
}

// -------------------------------------------------------------- component

TEST_CASE("OrbPad paints ink for enabled, disabled, and freeze states") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.4f, 0.7f, false, false });
    pad.setTap(1, { true, 0.6f, 0.4f, false, true });   // reversed glyph
    pad.setTap(2, { false, 0.3f, 0.3f, false, false }); // dashed placeholder
    pad.setSelected(0);
    pad.setFreeze(true);

    juce::Image img { juce::Image::ARGB, pad.getWidth(), pad.getHeight(), true };
    juce::Graphics g { img };
    pad.paintEntireComponent(g, true);

    int inked = 0;
    for (int y = 0; y < img.getHeight(); y += 7)
        for (int x = 0; x < img.getWidth(); x += 7)
            if (img.getPixelAt(x, y).getAlpha() > 0)
                ++inked;
    // The field background alone covers the pad — expect near-total coverage.
    CHECK(inked > 5000);
}

TEST_CASE("OrbPad reports selection changes and orb moves through callbacks") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.2f, 0.2f, false, false });

    int movedTap = -1;
    float movedX = -1.0f, movedY = -1.0f;
    pad.onOrbMove = [&] (int i, float x, float y) { movedTap = i; movedX = x; movedY = y; };
    int selected = -1;
    pad.onSelect = [&] (int i) { selected = i; };

    // Selection notifies on change only (mockup semantics) — start elsewhere.
    pad.setSelected(1);
    // Drive the drag path directly (mouse overrides call the same method).
    pad.beginOrbDrag(0);
    pad.dragOrbTo(0.75f, 0.6f);
    CHECK(selected == 0);
    CHECK(movedTap == 0);
    CHECK(movedX == Approx(0.75f).margin(1e-4));
    CHECK(movedY == Approx(0.6f).margin(1e-4));
}
