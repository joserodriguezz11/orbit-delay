// OrbPad — the orb field. The interaction physics (hit-testing,
// flick-to-throw glide with wall bounces, drag heat) live in pure orbpad::
// helpers tested directly; the component itself gets ink smoke tests and
// callback wiring checks, mirroring the rest of the GUI suite.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbPad.h"
#include "gui/OrbitTheme.h"

using Catch::Approx;

namespace {

juce::Image renderPad(OrbPad& pad) {
    juce::Image img { juce::Image::ARGB, pad.getWidth(), pad.getHeight(), true };
    juce::Graphics g { img };
    pad.paintEntireComponent(g, true);
    return img;
}

bool regionIdentical(const juce::Image& a, const juce::Image& b,
                     juce::Rectangle<int> r) {
    for (int y = r.getY(); y < r.getBottom(); ++y)
        for (int x = r.getX(); x < r.getRight(); ++x)
            if (a.getPixelAt(x, y) != b.getPixelAt(x, y))
                return false;
    return true;
}

} // namespace

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
    // 30px radius: 0.10 + 29/kPadW hits tap 0; +31/kPadW just misses on x.
    CHECK(orbpad::hitTest(taps, 0.10f + 29.0f / float(orbit::gui::theme::kPadW), 0.50f) == 0);
    CHECK(orbpad::hitTest(taps, 0.10f + 31.0f / float(orbit::gui::theme::kPadW), 0.05f) == -1);
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

    int inked = 0, samples = 0;
    for (int y = 0; y < img.getHeight(); y += 7)
        for (int x = 0; x < img.getWidth(); x += 7) {
            ++samples;
            if (img.getPixelAt(x, y).getAlpha() > 0)
                ++inked;
        }
    // The field background alone covers the pad — expect near-total coverage
    // at any pad size.
    CHECK(inked > (samples * 9) / 10);
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

TEST_CASE("an active glide keeps steering the thrown tap after selection changes") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    for (int i = 0; i < 4; ++i)
        pad.setTap(i, { true, 0.5f, 0.5f, false, false });

    // Throw tap 0: two fast moves build EMA velocity above the flick threshold.
    pad.beginOrbDrag(0);
    pad.dragOrbTo(0.5f, 0.5f);
    pad.dragOrbTo(0.6f, 0.5f);
    pad.endOrbDrag();

    // The user clicks tap 2's strip card while the orb is still flying.
    pad.setSelected(2);

    int movedTap = -1;
    pad.onOrbMove = [&] (int i, float, float) { movedTap = i; };
    pad.animationTick();

    CHECK(movedTap == 0);        // the glide still steers the thrown tap...
    CHECK(pad.selected() == 2);  // ...and selection stays where the user put it
}

TEST_CASE("orb drags emit gesture begin/end; a flick holds the gesture until the glide settles") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    for (int i = 0; i < 4; ++i)
        pad.setTap(i, { true, 0.5f, 0.5f, false, false });

    std::vector<std::pair<int, bool>> gestures;
    pad.onOrbGesture = [&] (int i, bool begin) { gestures.push_back({ i, begin }); };

    // Slow drag: gesture opens on grab and closes on release.
    pad.beginOrbDrag(0);
    REQUIRE(gestures.size() == 1);
    CHECK(gestures[0] == std::pair<int, bool>(0, true));
    pad.dragOrbTo(0.5f, 0.5f);
    pad.endOrbDrag();
    REQUIRE(gestures.size() == 2);
    CHECK(gestures[1] == std::pair<int, bool>(0, false));

    // Flick: the release launches a glide, so the gesture stays open until
    // the glide settles (the tap keeps receiving writes while it flies).
    pad.beginOrbDrag(1);
    pad.dragOrbTo(0.5f, 0.5f);
    pad.dragOrbTo(0.6f, 0.5f);
    pad.endOrbDrag();
    REQUIRE(gestures.size() == 3);
    CHECK(gestures[2] == std::pair<int, bool>(1, true));
    for (int i = 0; i < 20000 && gestures.size() < 4; ++i)
        pad.animationTick();
    REQUIRE(gestures.size() == 4);
    CHECK(gestures[3] == std::pair<int, bool>(1, false));
}

TEST_CASE("grabbing an orb mid-glide closes the glide's gesture first") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    for (int i = 0; i < 4; ++i)
        pad.setTap(i, { true, 0.5f, 0.5f, false, false });

    std::vector<std::pair<int, bool>> gestures;
    pad.onOrbGesture = [&] (int i, bool begin) { gestures.push_back({ i, begin }); };

    pad.beginOrbDrag(0);
    pad.dragOrbTo(0.5f, 0.5f);
    pad.dragOrbTo(0.6f, 0.5f);
    pad.endOrbDrag();            // tap 0 glides; its gesture is open

    pad.beginOrbDrag(2);         // grab tap 2 while tap 0 still flies
    REQUIRE(gestures.size() == 3);
    CHECK(gestures[1] == std::pair<int, bool>(0, false));  // glide closed...
    CHECK(gestures[2] == std::pair<int, bool>(2, true));   // ...before the grab opens
    pad.endOrbDrag();
}

// -------------------------------------------------------- riding knobs

TEST_CASE("riding knobs mirror the selected tap and write back through onOrbMove") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.5f, 0.6f, false, false });

    // Mirror: TIME shows the x position (190ms on the log curve), FEEDBACK
    // shows round(y*98) — the knob reads in true feedback % (param max 0.98).
    CHECK(pad.timeKnob().getValue() == Catch::Approx(0.5).margin(1e-3));
    CHECK(pad.timeKnob().getTextFromValue(0.5) == "190ms");
    CHECK(pad.fbKnob().getValue() == Catch::Approx(59.0));
    CHECK(pad.fbKnob().getTextFromValue(59.0) == "59%");

    // Write-back: turning a knob is an orb move on the selected tap.
    int movedTap = -1; float mx = -1.0f, my = -1.0f;
    pad.onOrbMove = [&] (int i, float x, float y) { movedTap = i; mx = x; my = y; };
    pad.timeKnob().setValue(0.75, juce::sendNotificationSync);
    CHECK(movedTap == 0);
    // 0.75 lands off the knob's 0.004 step grid and snaps to 0.752 — the
    // same rounding the mockup's snapTo applies.
    CHECK(mx == Catch::Approx(0.75f).margin(0.003));
    CHECK(my == Catch::Approx(0.6f).margin(1e-3));

    pad.fbKnob().setValue(49.0, juce::sendNotificationSync);
    CHECK(my == Catch::Approx(0.5f).margin(1e-3));
}

TEST_CASE("riding-knob double-click defaults match the tap parameter defaults") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.2f, 0.2f, false, false });

    float mx = -1.0f, my = -1.0f;
    pad.onOrbMove = [&] (int, float x, float y) { mx = x; my = y; };

    // FEEDBACK resets to the parameter default 0.35 (35%)...
    pad.fbKnob().resetToDefault();
    CHECK(pad.fbKnob().getValue() == Catch::Approx(35.0));
    CHECK(my == Catch::Approx(0.35f / 0.98f).margin(1e-3));
    // ...and free-mode TIME to the parameter default 350ms.
    pad.timeKnob().resetToDefault();
    CHECK(mx == Catch::Approx(orbit::gui::theme::msToX(350.0f)).margin(0.003f));
}

TEST_CASE("synced taps flip the TIME knob into stepped division mode") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    using orbit::gui::theme::msToX;
    pad.setSyncGrid({ { msToX(250.0f), "1/8" }, { msToX(500.0f), "1/4" },
                      { msToX(750.0f), "1/4 D" } });
    pad.setTap(0, { true, msToX(500.0f), 0.4f, true, false });

    // Stepped: value is the grid index of the tap's position.
    CHECK(pad.timeKnob().getMaximum() == Catch::Approx(2.0));
    CHECK(pad.timeKnob().getValue() == Catch::Approx(1.0));
    CHECK(pad.timeKnob().getTextFromValue(1.0) == "1/4");

    // Stepping emits the grid x, which the editor snaps onto the division.
    float mx = -1.0f;
    pad.onOrbMove = [&] (int, float x, float) { mx = x; };
    pad.timeKnob().setValue(0.0, juce::sendNotificationSync);
    CHECK(mx == Catch::Approx(msToX(250.0f)).margin(1e-4));
}

TEST_CASE("scale labels hide under a riding knob and return when it moves off") {
    const juce::Rectangle<float> timeKnob { 300.0f, 308.0f, 56.0f, 56.0f };
    const juce::Rectangle<float> fbKnob   {  28.0f, 120.0f, 56.0f, 56.0f };

    // Bottom ms label directly under the TIME knob — hidden (26px halo).
    CHECK(!orbpad::scaleLabelVisible({ 298.0f, 383.0f, 60.0f, 10.0f }, timeKnob, fbKnob));
    // Same label with TIME knob far away — visible.
    CHECK(orbpad::scaleLabelVisible({ 298.0f, 383.0f, 60.0f, 10.0f },
                                    { 600.0f, 308.0f, 56.0f, 56.0f }, fbKnob));
    // Left 50% label beside the FEEDBACK knob — hidden.
    CHECK(!orbpad::scaleLabelVisible({ 9.0f, 145.0f, 24.0f, 10.0f }, timeKnob, fbKnob));
    // Left label well below the FEEDBACK knob — visible.
    CHECK(orbpad::scaleLabelVisible({ 9.0f, 320.0f, 24.0f, 10.0f }, timeKnob, fbKnob));
}

TEST_CASE("watermark height scales down to fit the pad, never up") {
    // Wider than available: scales proportionally.
    CHECK(orbpad::watermarkHeight(190.0f, 900.0f, 690.0f)
          == Approx(190.0f * 690.0f / 900.0f));
    // Fits already: stays at base height.
    CHECK(orbpad::watermarkHeight(190.0f, 600.0f, 690.0f) == Approx(190.0f));
    // Degenerate width: clamps to a positive floor, no divide-by-zero.
    CHECK(orbpad::watermarkHeight(190.0f, 0.0f, 690.0f) == Approx(190.0f));
}

// ------------------------------------------------------ viz envelopes (W4)

TEST_CASE("viz envelope attacks at the attack rate and releases at the release rate") {
    const float dt = 1.0f / 60.0f;
    // Rising: one step toward 1 at attack rate 8.
    const float up = orbpad::stepEnvelope(0.0f, 1.0f, 8.0f, 2.0f, dt);
    CHECK(up == Approx(1.0f - std::exp(-8.0f * dt)).margin(1e-4));
    // Falling from that level uses the (slower) release rate 2.
    const float down = orbpad::stepEnvelope(up, 0.0f, 8.0f, 2.0f, dt);
    CHECK(down == Approx(up * std::exp(-2.0f * dt)).margin(1e-4));
    CHECK(down < up);
    // Never overshoots the target.
    float v = 0.0f;
    for (int i = 0; i < 2000; ++i)
        v = orbpad::stepEnvelope(v, 0.7f, 8.0f, 2.0f, dt);
    CHECK(v == Approx(0.7f).margin(1e-3));
    CHECK(v <= 0.7f + 1e-4f);
    // A hair from the target it snaps exactly, so repaint gates can settle.
    CHECK(orbpad::stepEnvelope(0.7004f, 0.7f, 8.0f, 2.0f, dt) == Approx(0.7f));
}

TEST_CASE("duck offset pushes an orb away from pad centre, capped, centre-safe") {
    constexpr float cx = float(orbit::gui::theme::kPadW) / 2.0f;
    constexpr float cy = float(orbit::gui::theme::kPadH) / 2.0f;

    // Right of centre: pushed further right, no vertical component.
    const auto r = orbpad::duckOffset(cx + 100.0f, cy, 1.0f);
    CHECK(r.x == Approx(7.0f).margin(1e-3));
    CHECK(r.y == Approx(0.0f).margin(1e-3));
    // Above centre: pushed further up (negative y in pixel space).
    const auto u = orbpad::duckOffset(cx, cy - 80.0f, 1.0f);
    CHECK(u.x == Approx(0.0f).margin(1e-3));
    CHECK(u.y == Approx(-7.0f).margin(1e-3));
    // Half duck scales linearly.
    const auto h = orbpad::duckOffset(cx + 100.0f, cy, 0.5f);
    CHECK(h.x == Approx(3.5f).margin(1e-3));
    // Dead centre: zero offset, no NaN from normalising a zero vector.
    const auto c = orbpad::duckOffset(cx, cy, 1.0f);
    CHECK(c.x == 0.0f);
    CHECK(c.y == 0.0f);
    // No duck: zero regardless of position.
    const auto z = orbpad::duckOffset(cx + 100.0f, cy + 50.0f, 0.0f);
    CHECK(z.x == 0.0f);
    CHECK(z.y == 0.0f);
}

TEST_CASE("frozen tint desaturates and swings hue toward ice blue") {
    const orbit::gui::theme::Lch warm { 0.55f, 0.24f, 29.0f };
    // mix 0: identity.
    const auto same = orbpad::frozenLch(warm, 0.0f);
    CHECK(same.l == Approx(warm.l));
    CHECK(same.c == Approx(warm.c));
    CHECK(same.h == Approx(warm.h));
    // mix 1: hue lands on the ice hue, chroma collapses to a quarter,
    // lightness lifts slightly (frost reads brighter, not darker).
    const auto iced = orbpad::frozenLch(warm, 1.0f);
    CHECK(iced.h == Approx(250.0f).margin(0.5f));
    CHECK(iced.c == Approx(warm.c * 0.25f).margin(1e-3));
    CHECK(iced.l > warm.l);
    // Half mix sits between, and the hue takes the short way round the wheel
    // (29 -> 250 shortest path is downward through red, not up through green).
    const auto half = orbpad::frozenLch(warm, 0.5f);
    CHECK(half.c == Approx((warm.c + iced.c) / 2.0f).margin(1e-3));
    CHECK((half.h < warm.h || half.h > 250.0f));  // wrapped path, not 139.5
}

TEST_CASE("fire envelope decays when live and holds under freeze") {
    const float dt = 1.0f / 60.0f;
    const float live = orbpad::stepFire(0.8f, false, dt);
    CHECK(live == Approx(0.8f * std::exp(-4.5f * dt)).margin(1e-4));
    // Frozen: the pulse holds — the buffer is held, so is its light.
    CHECK(orbpad::stepFire(0.8f, true, dt) == Approx(0.8f));
}

TEST_CASE("audio energy breathes the pad field") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    orbit::viz::VizFeed feed;
    feed.prepare();

    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.4f, 0.7f, false, false });
    pad.setSelected(0);
    pad.setEventSource(&feed);

    const auto quiet = renderPad(pad);

    orbit::viz::LevelSnapshot loud;
    loud.outRms = 0.5f;
    loud.duckGain = 1.0f;
    feed.writeLevels(loud);
    for (int i = 0; i < 4000; ++i)
        pad.animationTick();

    // The ambient envelope has risen — rings and halos brighten.
    CHECK(!regionIdentical(quiet, renderPad(pad), quiet.getBounds()));
}

TEST_CASE("ducking nudges the painted orbs off their true position") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    orbit::viz::VizFeed feed;
    feed.prepare();

    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.85f, 0.85f, false, false });  // far from centre
    pad.setSelected(0);
    pad.setEventSource(&feed);

    const auto rest = renderPad(pad);

    orbit::viz::LevelSnapshot ducked;
    ducked.duckGain = 0.2f;   // heavy ducking, no output energy
    feed.writeLevels(ducked);
    for (int i = 0; i < 4000; ++i)
        pad.animationTick();

    CHECK(!regionIdentical(rest, renderPad(pad), rest.getBounds()));
}

TEST_CASE("freeze ices the whole field, not just the orb frost rings") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbPad pad;
    pad.setSize(orbit::gui::theme::kPadW, orbit::gui::theme::kPadH);
    pad.setTap(0, { true, 0.4f, 0.7f, false, false });
    pad.setSelected(0);

    const auto live = renderPad(pad);

    pad.setFreeze(true);
    for (int i = 0; i < 6000; ++i)
        pad.animationTick();

    // A corner far from every orb: the per-orb frost rings can't reach it —
    // only the field-wide ice tint can change these pixels.
    CHECK(!regionIdentical(live, renderPad(pad), { 0, 0, 60, 60 }));
}
