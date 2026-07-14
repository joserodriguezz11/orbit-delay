#include "OrbPad.h"
#include "OrbitFonts.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

using theme::kPadW;
using theme::kPadH;

// ------------------------------------------------------------------ physics

namespace orbpad {

int hitTest(const std::array<TapPos, 4>& taps, float nx, float ny) {
    int best = -1;
    float bestD = 1.0e9f;
    for (int i = 0; i < 4; ++i) {
        const float d = std::hypot((nx - taps[size_t(i)].x) * float(kPadW),
                                   (ny - taps[size_t(i)].y) * float(kPadH));
        if (d < 30.0f && d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

int stepGlide(Glide& g, float dt) {
    if (!g.active)
        return kWallNone;
    g.x += g.vx * dt;
    g.y += g.vy * dt;
    const float damp = std::exp(-2.3f * dt);
    g.vx *= damp;
    g.vy *= damp;

    int walls = kWallNone;
    if (g.x < 0.0f) { g.x = -g.x;       g.vx = -g.vx * 0.55f; walls |= kWallLeft; }
    if (g.x > 1.0f) { g.x = 2.0f - g.x; g.vx = -g.vx * 0.55f; walls |= kWallRight; }
    if (g.y < 0.0f) { g.y = -g.y;       g.vy = -g.vy * 0.55f; walls |= kWallBottom; }
    if (g.y > 1.0f) { g.y = 2.0f - g.y; g.vy = -g.vy * 0.55f; walls |= kWallTop; }
    g.x = juce::jlimit(0.0f, 1.0f, g.x);
    g.y = juce::jlimit(0.0f, 1.0f, g.y);

    if (std::hypot(g.vx, g.vy) <= 0.02f)
        g.active = false;
    return walls;
}

bool shouldGlide(float vx, float vy) {
    return std::hypot(vx, vy) > 0.25f;
}

float stepHeat(float heat, float target01, float dt) {
    const float rate = target01 > heat ? 9.0f : 2.2f;
    heat += (target01 - heat) * (1.0f - std::exp(-rate * dt));
    return heat < 0.004f ? 0.0f : heat;
}

bool scaleLabelVisible(juce::Rectangle<float> label,
                       juce::Rectangle<float> timeKnob,
                       juce::Rectangle<float> fbKnob) {
    return !label.intersects(timeKnob.expanded(26.0f))
        && !label.intersects(fbKnob.expanded(26.0f));
}

} // namespace orbpad

// ---------------------------------------------------------------- component

OrbPad::OrbPad() {
    setOpaque(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);

    // Riding axis knobs: 56px, tap-linked accents, writing through onOrbMove.
    addAndMakeVisible(timeKnob_);
    addAndMakeVisible(fbKnob_);
    timeKnob_.setLinked(true);
    fbKnob_.setLinked(true);
    fbKnob_.setRange(0.0, 95.0, 1.0);
    fbKnob_.setKnobDefault(40.0);
    fbKnob_.textFromValueFunction = [] (double v) {
        return juce::String(juce::roundToInt(v)) + "%";
    };
    fbKnob_.onValueChange = [this] {
        if (knobGuard_)
            return;
        const auto& tp = taps_[size_t(sel_)];
        if (onOrbMove != nullptr)
            onOrbMove(sel_, tp.x, float(fbKnob_.getValue() / 95.0));
    };
    timeKnob_.onValueChange = [this] {
        if (knobGuard_)
            return;
        const auto& tp = taps_[size_t(sel_)];
        float x;
        if (timeKnobStepped_) {
            const int idx = juce::jlimit(0, int(syncGrid_.size()) - 1,
                                         juce::roundToInt(timeKnob_.getValue()));
            x = syncGrid_[size_t(idx)].x;
        } else {
            x = float(timeKnob_.getValue());
        }
        if (onOrbMove != nullptr)
            onOrbMove(sel_, x, tp.y);
    };
    configureTimeKnob();
    syncKnobsFromTap();
}

int OrbPad::nearestGridIndex(float x) const {
    int best = 0;
    float bestD = 1.0e9f;
    for (int i = 0; i < int(syncGrid_.size()); ++i) {
        const float d = std::abs(syncGrid_[size_t(i)].x - x);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void OrbPad::configureTimeKnob() {
    const bool stepped = taps_[size_t(sel_)].synced && !syncGrid_.empty();
    timeKnobStepped_ = stepped;
    knobGuard_ = true;
    if (stepped) {
        timeKnob_.setRange(0.0, double(syncGrid_.size() - 1), 1.0);
        timeKnob_.setKnobDefault(double(syncGrid_.size() / 2));
        timeKnob_.textFromValueFunction = [this] (double v) {
            const int idx = juce::jlimit(0, int(syncGrid_.size()) - 1,
                                         juce::roundToInt(v));
            return syncGrid_.empty() ? juce::String() : syncGrid_[size_t(idx)].label;
        };
    } else {
        timeKnob_.setRange(0.0, 1.0, 0.004);
        timeKnob_.setKnobDefault(0.5);
        timeKnob_.textFromValueFunction = [] (double v) {
            return juce::String(juce::roundToInt(theme::msOfX(float(v)))) + "ms";
        };
    }
    knobGuard_ = false;
}

void OrbPad::syncKnobsFromTap() {
    const auto& tp = taps_[size_t(sel_)];
    knobGuard_ = true;
    fbKnob_.setValue(std::round(tp.y * 95.0f), juce::sendNotificationSync);
    timeKnob_.setValue(timeKnobStepped_ ? double(nearestGridIndex(tp.x))
                                        : double(tp.x),
                       juce::sendNotificationSync);
    const auto accent = theme::lchColour(theme::tapLch(tp.x, tp.y, warm_, th()), 1.0f);
    timeKnob_.setAccent(accent);
    fbKnob_.setAccent(accent);
    knobGuard_ = false;
    updateKnobPositions();
}

void OrbPad::updateKnobPositions() {
    const auto& tp = taps_[size_t(sel_)];
    const float W = float(kPadW), H = float(kPadH);
    // Mockup containers: TIME rides x along the bottom, FEEDBACK rides y at
    // the left; both clamp so the 56px knob + label stay inside the field.
    const float bx = juce::jlimit(12.0f, W - 98.0f, tp.x * W - 42.0f);
    timeKnob_.setBounds(int(bx + 14.0f), int(H - 94.0f), 56, 56);
    const float ly = juce::jlimit(56.0f, H - 164.0f, (1.0f - tp.y) * H - 50.0f);
    fbKnob_.setBounds(28, int(ly + 12.0f), 56, 56);
}

void OrbPad::resized() {
    updateKnobPositions();
}

const theme::CharacterTheme& OrbPad::th() const {
    return theme::characterTheme(mode_);
}

void OrbPad::setTap(int i, const TapView& tap) {
    if (i < 0 || i >= 4)
        return;
    const bool syncFlipped = i == sel_ && taps_[size_t(i)].synced != tap.synced;
    taps_[size_t(i)] = tap;
    if (i == sel_) {
        if (syncFlipped)
            configureTimeKnob();
        syncKnobsFromTap();
    }
    repaint();
}

void OrbPad::setSelected(int i) {
    if (i == sel_ || i < 0 || i >= 4)
        return;
    sel_ = i;
    configureTimeKnob();
    syncKnobsFromTap();
    repaint();
}

void OrbPad::setCharacter(orbit::dsp::CharacterStage::Mode mode) {
    if (mode == mode_)
        return;
    mode_ = mode;
    repaint();
}

void OrbPad::setFreeze(bool frozen) {
    if (frozen == freeze_)
        return;
    freeze_ = frozen;
    repaint();
}

void OrbPad::setWarmField(bool warm) {
    warm_ = warm;
    repaint();
}

void OrbPad::setSyncGrid(std::vector<SyncGridEntry> grid) {
    syncGrid_ = std::move(grid);
    configureTimeKnob();
    syncKnobsFromTap();
    repaint();
}

// ------------------------------------------------------------------- drag

juce::Point<float> OrbPad::toNorm(juce::Point<float> p) const {
    const float s = float(getWidth()) / float(kPadW);
    return { juce::jlimit(0.0f, 1.0f, p.x / (float(kPadW) * s)),
             juce::jlimit(0.0f, 1.0f, 1.0f - p.y / (float(kPadH) * s)) };
}

void OrbPad::beginOrbDrag(int i) {
    if (i < 0 || i >= 4)
        return;
    glide_.active = false;
    if (sel_ != i) {
        sel_ = i;
        if (onSelect != nullptr)
            onSelect(i);
    }
    dragI_ = i;
    trail_.clear();
    velX_ = velY_ = 0.0f;
    lastX_ = taps_[size_t(i)].x;
    lastY_ = taps_[size_t(i)].y;
    lastMoveT_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
    repaint();
}

void OrbPad::dragOrbTo(float nx, float ny) {
    if (dragI_ < 0)
        return;
    nx = juce::jlimit(0.0f, 1.0f, nx);
    ny = juce::jlimit(0.0f, 1.0f, ny);

    // Pointer velocity EMA (mockup al = 0.5) drives flick detection.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = float(juce::jmax(0.004, now - lastMoveT_));
    velX_ = velX_ * 0.5f + 0.5f * (nx - lastX_) / dt;
    velY_ = velY_ * 0.5f + 0.5f * (ny - lastY_) / dt;
    lastMoveT_ = now;
    lastX_ = nx;
    lastY_ = ny;

    taps_[size_t(dragI_)].x = nx;   // optimistic; the APVTS echoes via setTap
    taps_[size_t(dragI_)].y = ny;
    if (onOrbMove != nullptr)
        onOrbMove(dragI_, nx, ny);
    repaint();
}

void OrbPad::endOrbDrag() {
    if (dragI_ < 0)
        return;
    const int i = dragI_;
    dragI_ = -1;
    if (orbpad::shouldGlide(velX_, velY_)) {
        glide_ = { taps_[size_t(i)].x, taps_[size_t(i)].y, velX_, velY_, true };
        hoverI_ = -1;
        // The glide keeps steering this tap from animationTick.
        sel_ = i;
    }
}

void OrbPad::mouseDown(const juce::MouseEvent& e) {
    const auto p = toNorm(e.position);
    std::array<orbpad::TapPos, 4> pos;
    for (int i = 0; i < 4; ++i)
        pos[size_t(i)] = { taps_[size_t(i)].x, taps_[size_t(i)].y };
    const int hit = orbpad::hitTest(pos, p.x, p.y);
    if (hit < 0)
        return;
    grabOffset_ = { taps_[size_t(hit)].x - p.x, taps_[size_t(hit)].y - p.y };
    beginOrbDrag(hit);
    dragOrbTo(p.x + grabOffset_.x, p.y + grabOffset_.y);
}

void OrbPad::mouseMove(const juce::MouseEvent& e) {
    const auto p = toNorm(e.position);
    std::array<orbpad::TapPos, 4> pos;
    for (int i = 0; i < 4; ++i)
        pos[size_t(i)] = { taps_[size_t(i)].x, taps_[size_t(i)].y };
    const int hover = orbpad::hitTest(pos, p.x, p.y);
    if (hover != hoverI_) {
        hoverI_ = hover;
        setMouseCursor(hover >= 0 ? juce::MouseCursor::DraggingHandCursor
                                  : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void OrbPad::mouseDrag(const juce::MouseEvent& e) {
    const auto p = toNorm(e.position);
    dragOrbTo(p.x + grabOffset_.x, p.y + grabOffset_.y);
}

void OrbPad::mouseUp(const juce::MouseEvent&) {
    endOrbDrag();
}

// -------------------------------------------------------------- animation

bool OrbPad::animating() const {
    if (dragI_ >= 0 || glide_.active || heat_ > 0.0f || speedEma_ > 0.001f)
        return true;
    if (!trail_.empty() || !sparks_.empty() || !flashes_.empty())
        return true;
    for (const float e : fireEnv_)
        if (e > 0.01f)
            return true;
    return false;
}

void OrbPad::burstSparks(float nx, float ny, theme::Lch col) {
    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& rng = juce::Random::getSystemRandom();
    for (int i = 0; i < 8; ++i) {
        const float ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float spd = 110.0f + 300.0f * rng.nextFloat();
        sparks_.push_back({ nx * float(kPadW), (1.0f - ny) * float(kPadH),
                            std::cos(ang) * spd, std::sin(ang) * spd,
                            now, 380.0 + 380.0 * rng.nextDouble(), col });
    }
}

void OrbPad::animationTick() {
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastTick_ > 0.0
        ? juce::jlimit(0.001f, 0.045f, float(now - lastTick_)) : 1.0f / 60.0f;
    lastTick_ = now;

    // Drain real echo fires into halo pulses regardless of visibility state,
    // so the SPSC ring never backs up while the pad idles.
    if (feed_ != nullptr) {
        orbit::viz::TapFireEvent ev;
        while (feed_->popEvent(ev))
            if (ev.tapIndex < 4)
                fireEnv_[ev.tapIndex] = juce::jmax(fireEnv_[ev.tapIndex],
                                                   ev.intensity01);
    }

    if (!animating())
        return;

    const double nowMs = now * 1000.0;

    // Trail + heat while dragging; heat cools after release.
    if (dragI_ >= 0) {
        const auto& tp = taps_[size_t(dragI_)];
        float inst = 0.0f;
        if (!trail_.empty()) {
            const auto& last = trail_.back();
            inst = std::hypot(tp.x - last.x, tp.y - last.y) / dt;
        }
        speedEma_ += (inst - speedEma_) * 0.18f;
        trail_.push_back({ tp.x, tp.y, nowMs, speedEma_ });
        if (trail_.size() > 200)
            trail_.erase(trail_.begin());
    } else {
        speedEma_ *= std::exp(-3.0f * dt);
    }
    heat_ = orbpad::stepHeat(heat_, juce::jlimit(0.0f, 1.0f, speedEma_ / 2.4f), dt);

    // Glide steers the thrown orb and reports every step like a live drag.
    if (glide_.active) {
        const int walls = orbpad::stepGlide(glide_, dt);
        taps_[size_t(sel_)].x = glide_.x;
        taps_[size_t(sel_)].y = glide_.y;
        if (onOrbMove != nullptr)
            onOrbMove(sel_, glide_.x, glide_.y);
        if (walls != orbpad::kWallNone) {
            if (walls & orbpad::kWallLeft)   flashes_.push_back({ 0, nowMs });
            if (walls & orbpad::kWallRight)  flashes_.push_back({ 1, nowMs });
            if (walls & orbpad::kWallTop)    flashes_.push_back({ 2, nowMs });
            if (walls & orbpad::kWallBottom) flashes_.push_back({ 3, nowMs });
            while (flashes_.size() > 6)
                flashes_.erase(flashes_.begin());
            const auto& tp = taps_[size_t(sel_)];
            burstSparks(glide_.x, glide_.y, theme::tapLch(tp.x, tp.y, warm_, th()));
        }
    }

    // Sparks fly and fade; stale trail/flash entries drop off.
    const float sparkDamp = std::exp(-2.2f * dt);
    for (auto& sp : sparks_) {
        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.vx *= sparkDamp;
        sp.vy *= sparkDamp;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(),
                                 [nowMs] (const Spark& sp) { return nowMs - sp.t0 >= sp.life; }),
                  sparks_.end());
    trail_.erase(std::remove_if(trail_.begin(), trail_.end(),
                                [nowMs] (const TrailPoint& q) { return nowMs - q.t > 900.0; }),
                 trail_.end());
    flashes_.erase(std::remove_if(flashes_.begin(), flashes_.end(),
                                  [nowMs] (const Flash& f) { return nowMs - f.t0 >= 520.0; }),
                   flashes_.end());
    for (auto& e : fireEnv_)
        e *= std::exp(-4.5f * dt);

    repaint();
}

// ------------------------------------------------------------------ paint

void OrbPad::paint(juce::Graphics& g) {
    const float scale = float(getWidth()) / float(kPadW);
    g.addTransform(juce::AffineTransform::scale(scale));
    const float W = float(kPadW), H = float(kPadH);
    const auto& ch = th();
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    // Field background: character-tinted radial falloff.
    {
        juce::ColourGradient bg { ch.bgTop, W * 0.5f, H * 0.42f,
                                  ch.bgBot, W * 0.5f, H * 0.42f + 720.0f, true };
        g.setGradientFill(bg);
        g.fillRect(0.0f, 0.0f, W, H);
    }

    const auto& selT = taps_[size_t(sel_)];
    const float spx = selT.x * W, spy = (1.0f - selT.y) * H;
    const auto selCol = theme::tapLch(selT.x, selT.y, warm_, ch);
    const auto acc = [&] (theme::Lch c, float a) { return theme::lchColour(c, a); };

    // Velocity aura: the selected orb suffuses the field, blooming with heat.
    {
        const float auraA = (0.13f + 0.32f * heat_) * ch.glow;
        const float r1 = 300.0f + 150.0f * heat_;
        juce::ColourGradient a1 { acc(selCol, auraA), spx, spy,
                                  acc(selCol, 0.0f), spx, spy + r1, true };
        a1.addColour(0.55, acc(selCol, auraA * 0.35f));
        g.setGradientFill(a1);
        g.fillRect(0.0f, 0.0f, W, H);
        juce::ColourGradient a2 { acc(selCol, 0.09f + 0.11f * heat_), spx, spy,
                                  acc(selCol, 0.0f), spx, spy + 680.0f, true };
        g.setGradientFill(a2);
        g.fillRect(0.0f, 0.0f, W, H);
    }

    // Watermark.
    g.setFont(fonts::watermark(190.0f));
    g.setColour(theme::bone50.withAlpha(0.032f));
    g.drawText("ORBIT", juce::Rectangle<float>(0.0f, H / 2.0f + 8.0f - 95.0f, W, 190.0f),
               juce::Justification::centred, false);
    // (removed) SYN·FX·001 — STEREO MULTI-TAP ECHO sub-line (spec: text removal)

    // Orbit rings; the ring passing near the selected orb lights up.
    {
        const float prad = std::hypot(spx - W / 2.0f, spy - H / 2.0f);
        for (float r = 44.0f; r < 640.0f; r += 44.0f) {
            const float near = std::exp(-std::abs(prad - r) / 24.0f);
            const float a = 0.028f + 0.14f * near * (0.4f + heat_ * 0.6f);
            g.setColour(near > 0.3f ? acc(selCol, a) : theme::bone50.withAlpha(a));
            g.drawEllipse(W / 2.0f - r, H / 2.0f - r, r * 2.0f, r * 2.0f, 1.0f);
        }
    }

    // Sync gridlines for the selected synced tap.
    if (selT.synced && !syncGrid_.empty()) {
        const float dashes[] = { 3.0f, 6.0f };
        g.setColour(acc(selCol, 0.14f));
        for (const auto& entry : syncGrid_)
            g.drawDashedLine({ { entry.x * W, 0.0f }, { entry.x * W, H } },
                             dashes, 2, 1.0f);
    }

    // Tap halos (two soft radial layers), pulsed by real echo fires.
    for (int i = 0; i < 4; ++i) {
        const auto& tp = taps_[size_t(i)];
        if (!tp.on)
            continue;
        const float px = tp.x * W, py = (1.0f - tp.y) * H;
        const auto col = theme::tapLch(tp.x, tp.y, warm_, ch);
        const float selB = i == sel_ ? 0.05f : 0.0f;
        const float fire = fireEnv_[size_t(i)] * 0.10f;
        const float r1 = 150.0f * ch.soft;
        juce::ColourGradient g1 { acc(col, (0.07f + selB + fire) * ch.glow), px, py,
                                  acc(col, 0.0f), px, py + r1, true };
        g.setGradientFill(g1);
        g.fillRect(px - r1, py - r1, r1 * 2.0f, r1 * 2.0f);
        const float r2 = 64.0f * ch.soft;
        juce::ColourGradient g2 { acc(col, (0.24f + selB + fire) * ch.glow), px, py,
                                  acc(col, 0.0f), px, py + r2, true };
        g.setGradientFill(g2);
        g.fillRect(px - r2, py - r2, r2 * 2.0f, r2 * 2.0f);
    }

    // Vignette.
    {
        juce::ColourGradient vg { juce::Colour { 0x00030307 }, W / 2.0f, H * 0.44f,
                                  juce::Colour { 0x80030307 }, W / 2.0f, H * 0.44f + 660.0f,
                                  true };
        g.setGradientFill(vg);
        g.fillRect(0.0f, 0.0f, W, H);
    }

    // Drag trail: wide faint underlay + thin bright core, fading over 900ms.
    for (size_t i = 1; i < trail_.size(); ++i) {
        const auto& q = trail_[i];
        const float age = float((nowMs - q.t) / 900.0);
        if (age > 1.0f)
            continue;
        const auto& q0 = trail_[i - 1];
        const float qs = juce::jmin(1.0f, q.sp / 2.4f);
        if (qs < 0.03f)
            continue;
        const auto col = theme::tapLch(q.x, q.y, warm_, ch);
        const juce::Line<float> seg { q0.x * W, (1.0f - q0.y) * H,
                                      q.x * W, (1.0f - q.y) * H };
        g.setColour(acc(col, (1.0f - age) * 0.12f * juce::jmax(0.3f, qs)));
        g.drawLine(seg, 4.0f + qs * 9.0f);
        g.setColour(acc(col, (1.0f - age) * 0.5f * juce::jmax(0.3f, qs)));
        g.drawLine(seg, 1.0f + qs * 2.4f);
    }

    // Sparks.
    for (const auto& sp : sparks_) {
        const float u = float((nowMs - sp.t0) / sp.life);
        if (u >= 1.0f)
            continue;
        g.setColour(acc(sp.lch, (1.0f - u) * 0.7f));
        g.drawLine({ sp.x, sp.y, sp.x - sp.vx * 0.028f, sp.y - sp.vy * 0.028f }, 1.6f);
    }

    // Wall-bounce flashes.
    for (const auto& f : flashes_) {
        const float u = float((nowMs - f.t0) / 520.0);
        if (u >= 1.0f)
            continue;
        const float a = (1.0f - u) * 0.3f;
        const auto c0 = acc(selCol, a), c1 = acc(selCol, 0.0f);
        juce::Rectangle<float> r;
        juce::ColourGradient grad;
        switch (f.wall) {
            case 0:  r = { 0.0f, 0.0f, 80.0f, H };
                     grad = { c0, 0.0f, 0.0f, c1, 80.0f, 0.0f, false }; break;
            case 1:  r = { W - 80.0f, 0.0f, 80.0f, H };
                     grad = { c0, W, 0.0f, c1, W - 80.0f, 0.0f, false }; break;
            case 2:  r = { 0.0f, 0.0f, W, 80.0f };
                     grad = { c0, 0.0f, 0.0f, c1, 0.0f, 80.0f, false }; break;
            default: r = { 0.0f, H - 80.0f, W, 80.0f };
                     grad = { c0, 0.0f, H, c1, 0.0f, H - 80.0f, false }; break;
        }
        g.setGradientFill(grad);
        g.fillRect(r);
    }

    // Crosshair through the selected orb.
    g.setColour(acc(selCol, 0.2f + heat_ * 0.2f));
    g.fillRect(spx - 0.5f, 0.0f, 1.0f, H);
    g.fillRect(0.0f, spy - 0.5f, W, 1.0f);

    // Etched rulers: time (bottom) and feedback (left).
    {
        const auto tkb = timeKnob_.getBounds().toFloat();
        const auto fkb = fbKnob_.getBounds().toFloat();
        g.setFont(fonts::monoSemiBold(8.0f));
        const struct { float ms; const char* label; } ticks[] = {
            { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" },
            { 400.0f, "400" }, { 800.0f, "800ms" } };
        for (const auto& tk : ticks) {
            const float gx = std::round(theme::msToX(tk.ms) * W) + 0.5f;
            g.setColour(theme::bone50.withAlpha(0.20f));
            g.fillRect(gx - 0.5f, H - 6.0f, 1.0f, 6.0f);
            const juce::Rectangle<float> lr { gx - 30.0f, H - 19.0f, 60.0f, 10.0f };
            if (orbpad::scaleLabelVisible(lr, tkb, fkb)) {
                g.setColour(theme::bone50.withAlpha(0.28f));
                g.drawText(tk.label, lr,
                           juce::Justification::centred, false);
            }
        }
        for (const float fb : { 25.0f, 50.0f, 75.0f }) {
            const float py = std::round((1.0f - fb / 95.0f) * H) + 0.5f;
            g.setColour(theme::bone50.withAlpha(0.20f));
            g.fillRect(0.0f, py - 0.5f, 6.0f, 1.0f);
            const juce::Rectangle<float> lr { 9.0f, py - 5.0f, 24.0f, 10.0f };
            if (orbpad::scaleLabelVisible(lr, tkb, fkb)) {
                g.setColour(theme::bone50.withAlpha(0.28f));
                g.drawText(juce::String(int(fb)), lr,
                           juce::Justification::centredLeft, false);
            }
        }
    }

    // Orbs.
    for (int i = 0; i < 4; ++i) {
        const auto& tp = taps_[size_t(i)];
        const float px = tp.x * W, py = (1.0f - tp.y) * H;
        const auto col = theme::tapLch(tp.x, tp.y, warm_, ch);

        if (!tp.on) {
            juce::Path dashed;
            dashed.addEllipse(px - 11.0f, py - 11.0f, 22.0f, 22.0f);
            juce::Path stroked;
            const float dashes[] = { 3.0f, 4.0f };
            juce::PathStrokeType { 1.3f }.createDashedStroke(stroked, dashed, dashes, 2);
            g.setColour(theme::bone50.withAlpha(0.22f));
            g.fillPath(stroked);
            g.setFont(fonts::monoSemiBold(9.0f));
            g.setColour(theme::bone50.withAlpha(0.3f));
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<float>(px - 10.0f, py - 5.0f, 20.0f, 10.0f),
                       juce::Justification::centred, false);
            continue;
        }

        const float pR = i == sel_ ? 14.5f : 12.0f;

        // Glow (shadowBlur stand-in): a tight radial gradient behind the orb.
        {
            const float gr = pR + (14.0f + (i == sel_ ? 5.0f : 0.0f)) * ch.glow;
            juce::ColourGradient glow { acc(col, 0.55f), px, py,
                                        acc(col, 0.0f), px, py + gr, true };
            g.setGradientFill(glow);
            g.fillEllipse(px - gr, py - gr, gr * 2.0f, gr * 2.0f);
        }

        g.setColour(theme::ink900.withAlpha(0.62f));
        g.fillEllipse(px - pR, py - pR, pR * 2.0f, pR * 2.0f);
        g.setColour(acc(col, 1.0f));
        g.drawEllipse(px - pR, py - pR, pR * 2.0f, pR * 2.0f,
                      i == sel_ ? 2.0f : 1.6f);

        if (tp.reversed) {
            // Reverse glyph: open arc with an arrowhead (mockup 0.6..3.7 rad).
            const float ar = pR - 4.5f;
            juce::Path arc;
            arc.addCentredArc(px, py, ar, ar, 0.0f,
                              0.6f + juce::MathConstants<float>::halfPi,
                              3.7f + juce::MathConstants<float>::halfPi, true);
            g.setColour(acc(col, 0.8f));
            g.strokePath(arc, juce::PathStrokeType { 1.2f });
            const float ax = px + ar * std::cos(0.6f), ay = py + ar * std::sin(0.6f);
            juce::Path head;
            head.addTriangle(ax - 3.0f, ay - 1.0f, ax + 2.0f, ay - 3.5f,
                             ax + 2.5f, ay + 2.5f);
            g.fillPath(head);
        } else {
            g.setFont(fonts::monoSemiBold(10.0f));
            g.setColour(theme::bone50);
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<float>(px - 10.0f, py - 5.0f, 20.0f, 10.0f),
                       juce::Justification::centred, false);
        }

        if (i == sel_) {
            g.setColour(acc(col, 0.4f));
            g.drawEllipse(px - pR - 7.0f, py - pR - 7.0f, (pR + 7.0f) * 2.0f,
                          (pR + 7.0f) * 2.0f, 1.2f);
        }
        if (i == hoverI_ && i != sel_) {
            g.setColour(acc(col, 0.45f));
            g.drawEllipse(px - pR - 6.0f, py - pR - 6.0f, (pR + 6.0f) * 2.0f,
                          (pR + 6.0f) * 2.0f, 1.0f);
        }
        if (freeze_) {
            g.setColour(acc(col, 0.55f));
            g.drawEllipse(px - pR - 13.0f, py - pR - 13.0f, (pR + 13.0f) * 2.0f,
                          (pR + 13.0f) * 2.0f, 1.0f);
        }
    }

    // Grit scanlines.
    if (ch.scan > 0.0f) {
        g.setColour(juce::Colours::black.withAlpha(0.16f * ch.scan));
        for (float y = 0.0f; y < H; y += 4.0f)
            g.fillRect(0.0f, y, W, 1.0f);
    }

    // Corner hint.
    g.setFont(fonts::tracked(fonts::mono(8.0f), 0.18f));
    g.setColour(theme::bone50.withAlpha(0.32f));
    g.drawText(juce::String::fromUTF8("DRAG ORB \xc2\xb7 FLICK TO THROW"),
               juce::Rectangle<float>(W - 260.0f - 14.0f, 10.0f, 260.0f, 10.0f),
               juce::Justification::centredRight, false);

    // Riding-knob scrims + labels (the knobs themselves are child components
    // painted after this). Scrim: dark radial pool with a heat-driven glow.
    const auto knobScrim = [&] (const OrbitKnob& k, const juce::String& label,
                                const juce::String& axis) {
        const float cx = float(k.getX()) + 28.0f, cy = float(k.getY()) + 28.0f;
        juce::ColourGradient pool { theme::panel.withAlpha(0.92f), cx, cy,
                                    theme::panel.withAlpha(0.0f), cx, cy + 52.0f, true };
        pool.addColour(0.62, theme::panel.withAlpha(0.55f));
        g.setGradientFill(pool);
        g.fillEllipse(cx - 52.0f, cy - 52.0f, 104.0f, 104.0f);
        if (heat_ > 0.0f) {
            g.setColour(acc(selCol, (0.2f + heat_ * 0.15f) * 0.5f));
            g.drawEllipse(cx - 31.0f, cy - 31.0f, 62.0f, 62.0f, 3.0f);
        }
        g.setFont(fonts::tracked(fonts::monoSemiBold(7.5f), 0.2f));
        const float ty = cy + 32.0f;
        const float lw = 90.0f;
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawText(label + " ", juce::Rectangle<float>(cx - lw / 2.0f, ty, lw - 14.0f, 9.0f),
                   juce::Justification::centredRight, false);
        g.setColour(acc(selCol, 1.0f));
        g.drawText(axis, juce::Rectangle<float>(cx + lw / 2.0f - 14.0f, ty, 14.0f, 9.0f),
                   juce::Justification::centredLeft, false);
    };
    knobScrim(timeKnob_, "TIME", juce::String::fromUTF8("\xc2\xb7X"));
    knobScrim(fbKnob_, "FEEDBACK", juce::String::fromUTF8("\xc2\xb7Y"));
}
