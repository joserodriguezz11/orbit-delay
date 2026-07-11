#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OrbitTheme.h"

// The Full v2 control set (docs/design/orbit-delay-full-v2.html): every knob,
// pill, switch, segmented control and meter in the editor is one of these.
// Interaction constants (drag span, fine factor, wheel step, spring feel,
// peak-hold times) are the mockup's values — change them there first.

// ---------------------------------------------------------------- OrbitKnob
// Arc knob: -135°..+135° sweep, ticks from 40px up, gradient cap, pointer,
// centred mono value text. Vertical drag (180px = full range, shift = 0.22
// fine), wheel (range/80, shift = one step), double-click resets. A stiff
// spring settles the *displayed* position; the value itself moves instantly.
// Subclasses juce::Slider so APVTS SliderAttachments work unchanged.
class OrbitKnob : public juce::Slider {
public:
    OrbitKnob();

    // Pure gesture math (mockup Knob start()/wheel()), unit-tested directly.
    // dyUp is pixels of upward mouse travel; domDeltaY follows DOM sign
    // conventions (positive = scroll down = decrease).
    static double draggedValue(double startValue, double dyUp, double min,
                               double max, double step, bool fine);
    static double wheeledValue(double current, double domDeltaY, double min,
                               double max, double step, bool fine);

    // Critically-damped-ish display spring (mockup useSpring, feel=2 default).
    class Spring {
    public:
        void snapTo(float p) { pos_ = p; vel_ = 0.0f; }
        void tick(float target, float dt);
        float position() const { return pos_; }
        bool settled(float target) const;
    private:
        float pos_ = 0.0f, vel_ = 0.0f;
    };

    void setKnobDefault(double v) { default_ = v; hasDefault_ = true; }
    void resetToDefault() { if (hasDefault_) setValue(default_, juce::sendNotificationSync); }

    // Position-linked knobs colour their value arc with the tap accent;
    // manual knobs use the bone manual colour (mockup `linked`/`accent`).
    void setLinked(bool linked) { linked_ = linked; repaint(); }
    void setAccent(juce::Colour accent) { accent_ = accent; repaint(); }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void springTick();

    juce::Colour accent_ { orbit::gui::theme::ember };
    bool linked_ = false;
    double default_ = 0.0;
    bool hasDefault_ = false;
    double dragStartValue_ = 0.0;
    Spring spring_;
    bool springInitialised_ = false;
    juce::VBlankAttachment vblank_ { this, [this] { springTick(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitKnob)
};

// ---------------------------------------------------------------- OrbitPill
// Pill toggle (SYNC / REV / FREEZE): outlined when off, accent-filled when
// on, tracked mono label. Subclasses juce::Button so ButtonAttachments work.
class OrbitPill : public juce::Button {
public:
    explicit OrbitPill(const juce::String& label);

    void setAccent(juce::Colour accent) { accent_ = accent; repaint(); }
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour accent_ { orbit::gui::theme::ember };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitPill)
};

// -------------------------------------------------------------- OrbitSwitch
// 40x22 sliding switch (P-PONG). juce::ToggleButton subclass for attachments.
class OrbitSwitch : public juce::ToggleButton {
public:
    static constexpr int kWidth = 40;
    static constexpr int kHeight = 22;

    OrbitSwitch() = default;
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitSwitch)
};

// ----------------------------------------------------------------- OrbitSeg
// Segmented pill row (CLEAN / TAPE / GRIT) in a bordered well. Reports the
// selected index through onChange; wire to a choice parameter externally.
class OrbitSeg : public juce::Component {
public:
    explicit OrbitSeg(juce::StringArray options);

    std::function<void(int)> onChange;

    void setSelectedIndex(int index, juce::NotificationType notify);
    int selectedIndex() const { return selected_; }

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> segmentBounds(int index) const;

    juce::StringArray options_;
    int selected_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitSeg)
};

// --------------------------------------------------------------- OrbitMeter
// Level bar with 380ms peak-hold then 1.4/s decay (mockup Meter): vertical
// (IN/OUT) or horizontal (duck gain-reduction). Pulls its level from a getter
// every vblank; refreshNow() pulls once for headless tests.
class OrbitMeter : public juce::Component {
public:
    struct PeakState {
        float peak = 0.0f;
        double heldSince = 0.0;
    };
    static void stepPeak(PeakState&, float level, double nowSeconds, float dt);

    explicit OrbitMeter(std::function<float()> getLevel, bool horizontal = false,
                        juce::Colour fill = orbit::gui::theme::ember);

    void refreshNow();
    void paint(juce::Graphics&) override;

private:
    std::function<float()> getLevel_;
    bool horizontal_ = false;
    juce::Colour fill_;
    float level_ = 0.0f;
    PeakState peak_;
    double lastTick_ = 0.0;
    juce::VBlankAttachment vblank_ { this, [this] { refreshNow(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitMeter)
};
