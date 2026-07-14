#include "OrbitControls.h"
#include "OrbitFonts.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

namespace {

// Mockup gesture constants.
constexpr double kDragSpanPx = 180.0;   // full range per 180px of travel
constexpr double kFineFactor = 0.22;    // shift-drag multiplier
constexpr double kWheelDiv   = 80.0;    // coarse wheel = range/80

// Mockup spring at feel=2, seed=0.3:
// k = max(50, 430 - feel*3.4) * (1 + seed*0.35); c = 1.75*sqrt(k)*(0.78 + seed*0.22).
constexpr float kSpringK = float((430.0 - 2.0 * 3.4) * (1.0 + 0.3 * 0.35));
const float kSpringC = 1.75f * std::sqrt(kSpringK) * (0.78f + 0.3f * 0.22f);

double snapTo(double v, double step) {
    return step > 0.0 ? std::round(v / step) * step : v;
}

} // namespace

// ------------------------------------------------------------------ OrbitKnob

OrbitKnob::OrbitKnob() {
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

double OrbitKnob::draggedValue(double startValue, double dyUp, double min,
                               double max, double step, bool fine) {
    const double range = max - min;
    double nv = startValue + (dyUp * range / kDragSpanPx) * (fine ? kFineFactor : 1.0);
    return juce::jlimit(min, max, snapTo(nv, step));
}

double OrbitKnob::wheeledValue(double current, double domDeltaY, double min,
                               double max, double step, bool fine) {
    const double range = max - min;
    const double inc = fine ? step : juce::jmax(step, range / kWheelDiv);
    const double sign = domDeltaY > 0.0 ? 1.0 : (domDeltaY < 0.0 ? -1.0 : 0.0);
    double nv = current - sign * inc;
    return juce::jlimit(min, max, snapTo(nv, step));
}

void OrbitKnob::Spring::tick(float target, float dt) {
    dt = juce::jlimit(0.001f, 0.045f, dt);
    const float acc = kSpringK * (target - pos_) - kSpringC * vel_;
    vel_ += acc * dt;
    pos_ += vel_ * dt;
    if (settled(target)) {
        pos_ = target;
        vel_ = 0.0f;
    }
}

bool OrbitKnob::Spring::settled(float target) const {
    return std::abs(target - pos_) < 0.0007f && std::abs(vel_) < 0.008f;
}

void OrbitKnob::springTick() {
    const auto target = float(valueToProportionOfLength(getValue()));
    if (!springInitialised_) {
        spring_.snapTo(target);
        springInitialised_ = true;
        return;
    }
    if (spring_.settled(target) && spring_.position() == target)
        return;
    spring_.tick(target, 1.0f / 60.0f);
    repaint();
}

void OrbitKnob::mouseDown(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    dragStartValue_ = getValue();
}

void OrbitKnob::mouseDrag(const juce::MouseEvent& e) {
    const double dyUp = double(-e.getDistanceFromDragStartY());
    setValue(draggedValue(dragStartValue_, dyUp, getMinimum(), getMaximum(),
                          getInterval(), e.mods.isShiftDown()),
             juce::sendNotificationSync);
}

void OrbitKnob::mouseDoubleClick(const juce::MouseEvent&) {
    resetToDefault();
}

void OrbitKnob::mouseWheelMove(const juce::MouseEvent& e,
                               const juce::MouseWheelDetails& wheel) {
    // JUCE deltaY is positive scrolling up; the mockup math uses DOM signs.
    const double domDeltaY = double(-wheel.deltaY);
    if (domDeltaY == 0.0)
        return;
    setValue(wheeledValue(getValue(), domDeltaY, getMinimum(), getMaximum(),
                          getInterval(), e.mods.isShiftDown()),
             juce::sendNotificationSync);
}

void OrbitKnob::paint(juce::Graphics& g) {
    const float S = float(juce::jmin(getWidth(), getHeight()));
    const float r = S / 2.0f - 5.0f;
    const float cx = float(getWidth()) / 2.0f, cy = float(getHeight()) / 2.0f;

    // Displayed proportion: sprung when animating, live during first paint.
    const float target = float(valueToProportionOfLength(getValue()));
    const float n = springInitialised_ ? juce::jlimit(0.0f, 1.0f, spring_.position())
                                       : target;
    const float va = -135.0f + n * 270.0f;
    const auto rad = [] (float deg) { return juce::degreesToRadians(deg); };

    // Ticks (40px and up): 11 marks every 27°, emphasis at the extremes/centre.
    if (S >= 40.0f) {
        for (int ti = 0; ti <= 10; ++ti) {
            const float ta = rad(-135.0f + float(ti) * 27.0f);
            const auto p0 = juce::Point<float>(cx, cy).getPointOnCircumference(r + 2.2f, ta);
            const auto p1 = juce::Point<float>(cx, cy).getPointOnCircumference(r + 4.6f, ta);
            const bool emphasis = ti == 0 || ti == 5 || ti == 10;
            g.setColour(theme::bone50.withAlpha(emphasis ? 0.30f : 0.14f));
            g.drawLine({ p0, p1 }, 1.0f);
        }
    }

    // Cap: radial gradient dark centre, hairline ring.
    {
        const float capR = r * 0.62f;
        juce::ColourGradient grad { juce::Colour { 0xff181822 },
                                    cx, cy - capR * 0.36f,
                                    juce::Colour { 0xff0a0a10 },
                                    cx, cy + capR,
                                    true };
        g.setGradientFill(grad);
        g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        g.setColour(theme::bone50.withAlpha(0.10f));
        g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);
    }

    const float arcW = S >= 56.0f ? 3.6f : 3.0f;

    // Track arc: full sweep.
    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, rad(-135.0f), rad(135.0f), true);
    g.setColour(theme::track());
    g.strokePath(track, { arcW, juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded });

    // Value dot: rides the track arc at the value angle (halo underneath).
    {
        const auto dc = juce::Point<float>(cx, cy).getPointOnCircumference(r, rad(va));
        const float dotR = arcW * 0.9f;
        const auto col = linked_ ? accent_ : theme::manual();
        g.setColour(col.withAlpha(0.22f));
        g.fillEllipse(dc.x - dotR * 2.2f, dc.y - dotR * 2.2f, dotR * 4.4f, dotR * 4.4f);
        g.setColour(col);
        g.fillEllipse(dc.x - dotR, dc.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // Centred value text (size-dependent, shrinks past 6 characters).
    {
        const auto txt = getTextFromValue(getValue());
        float fs = S >= 64.0f ? 12.5f : S >= 56.0f ? 11.0f
                 : S >= 48.0f ? 10.0f : S >= 40.0f ? 9.5f : 8.5f;
        if (txt.length() > 6)
            fs *= 0.82f;
        g.setFont(fonts::monoSemiBold(fs));
        g.setColour(theme::text());
        g.drawText(txt, getLocalBounds(), juce::Justification::centred, false);
    }
}

// ------------------------------------------------------------------ OrbitPill

OrbitPill::OrbitPill(const juce::String& label) : juce::Button(label) {
    setClickingTogglesState(true);
}

void OrbitPill::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    juce::ignoreUnused(down);
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();
    const float radius = b.getHeight() / 2.0f;

    if (on) {
        g.setColour(accent_);
        g.fillRoundedRectangle(b, radius);
    } else if (highlighted) {
        g.setColour(theme::well());
        g.fillRoundedRectangle(b, radius);
    }
    g.setColour(on ? accent_ : theme::line2());
    g.drawRoundedRectangle(b, radius, 1.0f);

    g.setColour(on ? theme::ink900 : theme::textDim());
    g.setFont(fonts::tracked(fonts::monoSemiBold(8.0f), 0.12f));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

// ---------------------------------------------------------------- OrbitSwitch

void OrbitSwitch::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    juce::ignoreUnused(highlighted, down);
    const bool on = getToggleState();
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = b.getHeight() / 2.0f;

    g.setColour(on ? theme::ember : theme::well());
    g.fillRoundedRectangle(b, radius);
    g.setColour(on ? theme::ember : theme::line2());
    g.drawRoundedRectangle(b, radius, 1.0f);

    // Thumb: 16px circle, 2px inset, slides left->right.
    const float d = b.getHeight() - 5.0f;
    const float x = on ? b.getRight() - d - 2.0f : b.getX() + 2.0f;
    g.setColour(on ? theme::bone50 : theme::textDim());
    g.fillEllipse(x, b.getCentreY() - d / 2.0f, d, d);
}

// ------------------------------------------------------------------- OrbitSeg

OrbitSeg::OrbitSeg(juce::StringArray options) : options_(std::move(options)) {}

void OrbitSeg::setSelectedIndex(int index, juce::NotificationType notify) {
    index = juce::jlimit(0, options_.size() - 1, index);
    if (index == selected_)
        return;
    selected_ = index;
    repaint();
    if (notify != juce::dontSendNotification && onChange != nullptr)
        onChange(selected_);
}

juce::Rectangle<float> OrbitSeg::segmentBounds(int index) const {
    // Well padding 3, gap 2, equal-width segments (mockup Seg).
    auto inner = getLocalBounds().toFloat().reduced(3.0f);
    const float gap = 2.0f;
    const float w = (inner.getWidth() - gap * float(options_.size() - 1))
                    / float(options_.size());
    return { inner.getX() + float(index) * (w + gap), inner.getY(),
             w, inner.getHeight() };
}

void OrbitSeg::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = b.getHeight() / 2.0f;
    g.setColour(theme::well());
    g.fillRoundedRectangle(b, radius);
    g.setColour(theme::line());
    g.drawRoundedRectangle(b, radius, 1.0f);

    for (int i = 0; i < options_.size(); ++i) {
        const auto seg = segmentBounds(i);
        const bool on = i == selected_;
        if (on) {
            g.setColour(theme::ember);
            g.fillRoundedRectangle(seg, seg.getHeight() / 2.0f);
        }
        g.setColour(on ? theme::text() : theme::textDim());
        g.setFont(fonts::tracked(fonts::monoSemiBold(8.5f), 0.14f));
        g.drawText(options_[i], seg.toNearestInt(), juce::Justification::centred, false);
    }
}

void OrbitSeg::mouseUp(const juce::MouseEvent& e) {
    for (int i = 0; i < options_.size(); ++i)
        if (segmentBounds(i).expanded(1.0f).contains(e.position))
            return setSelectedIndex(i, juce::sendNotification);
}

// ----------------------------------------------------------------- OrbitMeter

void OrbitMeter::stepPeak(PeakState& st, float level, double nowSeconds, float dt) {
    if (level >= st.peak) {
        st.peak = level;
        st.heldSince = nowSeconds;
    } else if (nowSeconds - st.heldSince > 0.38) {
        st.peak = juce::jmax(level, st.peak - dt * 1.4f);
    }
}

OrbitMeter::OrbitMeter(std::function<float()> getLevel, bool horizontal,
                       juce::Colour fill)
    : getLevel_(std::move(getLevel)), horizontal_(horizontal), fill_(fill) {
    setInterceptsMouseClicks(false, false);
}

void OrbitMeter::refreshNow() {
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastTick_ > 0.0
        ? juce::jlimit(0.001f, 0.1f, float(now - lastTick_)) : 1.0f / 60.0f;
    lastTick_ = now;
    level_ = juce::jlimit(0.0f, 1.0f, getLevel_ != nullptr ? getLevel_() : 0.0f);
    stepPeak(peak_, level_, now, dt);
    repaint();
}

void OrbitMeter::paint(juce::Graphics& g) {
    const auto b = getLocalBounds().toFloat();
    g.setColour(theme::bone50.withAlpha(0.10f));
    g.fillRect(b);

    g.setColour(fill_);
    if (horizontal_)
        g.fillRect(b.withWidth(b.getWidth() * level_));
    else
        g.fillRect(b.withTrimmedTop(b.getHeight() * (1.0f - level_)));

    // Segment dashes every 3px, panel-dark.
    g.setColour(theme::panel.withAlpha(0.55f));
    if (horizontal_)
        for (float x = 2.0f; x < b.getWidth(); x += 3.0f)
            g.fillRect(x, 0.0f, 1.0f, b.getHeight());
    else
        for (float y = 2.0f; y < b.getHeight(); y += 3.0f)
            g.fillRect(0.0f, y, b.getWidth(), 1.0f);

    // Peak-hold line.
    if (peak_.peak > 0.02f) {
        g.setColour(theme::bone50.withAlpha(0.9f));
        if (horizontal_)
            g.fillRect(juce::jmin(b.getWidth() - 1.0f, b.getWidth() * peak_.peak),
                       0.0f, 1.0f, b.getHeight());
        else
            g.fillRect(0.0f, juce::jmax(0.0f, b.getHeight() * (1.0f - peak_.peak)),
                       b.getWidth(), 1.0f);
    }
}
