#pragma once

#include <array>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "OrbitControls.h"
#include "OrbitTheme.h"
#include "dsp/CharacterStage.h"
#include "viz/VizFeed.h"

// Interaction physics for the orb field (mockup OrbPad state machine),
// kept as pure functions so the flick/bounce/heat behaviour is unit-testable
// without a component or a clock.
namespace orbpad {

struct TapPos { float x, y; };  // normalized, y-up (1 = top of the field)

// Nearest orb within 30 design px of the pointer (kPadW x kPadH field), else -1.
// Disabled taps are still grabbable, so all four positions participate.
int hitTest(const std::array<TapPos, 4>& taps, float nx, float ny);

// Wall-hit bitmask returned by stepGlide.
inline constexpr int kWallNone   = 0;
inline constexpr int kWallLeft   = 1;
inline constexpr int kWallRight  = 2;
inline constexpr int kWallTop    = 4;
inline constexpr int kWallBottom = 8;

// A thrown orb: integrates position, damps exp(-2.3 dt), reflects off the
// field walls at 0.55 restitution, deactivates below speed 0.02.
struct Glide {
    float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
    bool active = false;
};
int stepGlide(Glide&, float dt);

// A release launches a glide only above the mockup's 0.25 speed threshold.
bool shouldGlide(float vx, float vy);

// Drag heat: attacks at rate 9, releases at 2.2, snaps to zero under 0.004.
float stepHeat(float heat, float target01, float dt);

// Scale-label suppression: a ruler label hides while a riding knob (56px,
// with its ~26px scrim/label halo) overlaps it, and returns when clear.
bool scaleLabelVisible(juce::Rectangle<float> label,
                       juce::Rectangle<float> timeKnob,
                       juce::Rectangle<float> fbKnob);

// Watermark fit: the height that renders a string measured at baseHeight
// into availableWidth — scales down proportionally, never up.
float watermarkHeight(float baseHeight, float stringWidthAtBase, float availableWidth);

// Viz envelope: exponential approach with separate attack (rising) and
// release (falling) rates; snaps onto the target within 0.0005 so the
// repaint gate can settle instead of chasing an asymptote.
float stepEnvelope(float current, float target, float attackRate,
                   float releaseRate, float dt);

// Ducking pushes an orb's *painted* position radially away from pad centre —
// up to 7 design px at full duck. Paint-only: hit-testing and parameter truth
// stay on the real position. Zero at pad centre (nothing to normalise).
juce::Point<float> duckOffset(float px, float py, float duck01);

// Freeze ice-over: lerps a tap colour toward a cold blue-white — chroma
// collapses to a quarter, hue takes the short way round to 250°, lightness
// lifts a touch. mix 0 = identity, 1 = fully iced.
orbit::gui::theme::Lch frozenLch(orbit::gui::theme::Lch c, float mix);

// Fire-pulse decay (rate 4.5), paused while frozen: the buffer is held,
// so is its light.
float stepFire(float env, bool frozen, float dt);

} // namespace orbpad

// The orb field (kPadW x kPadH): character-themed background, orbit rings, watermark,
// position-coloured tap halos and orbs, crosshair + etched rulers, drag trails
// with heat bloom, flick-to-throw with bounce flashes and spark bursts, and
// (beyond the static mockup) halo pulses from the engine's real TapFireEvents.
// The pad owns no parameters: drags emit onOrbMove(tap, x, y) and the editor
// writes the APVTS; setTap() flows the truth back in.
class OrbPad : public juce::Component {
public:
    struct TapView {
        bool on = false;
        float x = 0.3f, y = 0.3f;   // normalized, y-up
        bool synced = false;
        bool reversed = false;
    };

    OrbPad();

    std::function<void(int, float, float)> onOrbMove;
    std::function<void(int)> onSelect;
    // Gesture bracket around a tap's writes: (tap, true) when a drag, glide
    // or riding-knob turn starts writing it, (tap, false) when it stops. The
    // editor maps these onto begin/endChangeGesture so hosts see orb moves
    // as automation gestures. A flick holds the gesture open until the glide
    // settles.
    std::function<void(int, bool)> onOrbGesture;

    void setTap(int i, const TapView& tap);
    void setSelected(int i);              // silent (editor-driven)
    int selected() const { return sel_; }
    void setCharacter(orbit::dsp::CharacterStage::Mode mode);
    void setFreeze(bool frozen);
    void setWarmField(bool warm);
    // Sync grid: normalized-x gridline positions with their division names,
    // shown while the selected tap is synced (and driving the stepped TIME
    // knob). The editor derives these from real TempoSync divisions at host
    // tempo; empty hides the grid.
    struct SyncGridEntry { float x; juce::String label; };
    void setSyncGrid(std::vector<SyncGridEntry> grid);
    // Optional: real echo fires pulse the matching orb's halo.
    void setEventSource(orbit::viz::VizFeed* feed) { feed_ = feed; }

    // Drag pipeline — the mouse overrides drive exactly these, so tests can
    // exercise selection/move semantics without synthesizing MouseEvents.
    void beginOrbDrag(int i);
    void dragOrbTo(float nx, float ny);   // already grab-compensated
    void endOrbDrag();

    // Riding axis knobs (mockup kTime/kFb): TIME follows the selected orb
    // along the bottom edge, FEEDBACK along the left. Turning them emits
    // onOrbMove on the selected tap — the same write path as a drag.
    OrbitKnob& timeKnob() { return timeKnob_; }
    OrbitKnob& fbKnob() { return fbKnob_; }

    void resized() override;

    // One animation step (trail/heat/glide/sparks). The VBlank attachment
    // drives it live; public so headless tests can step the glide (mirrors
    // OrbitMeter::refreshNow).
    void animationTick();

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    struct TrailPoint { float x, y; double t; float sp; };
    struct Spark { float x, y, vx, vy; double t0, life; orbit::gui::theme::Lch lch; };
    struct Flash { int wall; double t0; };

    juce::Point<float> toNorm(juce::Point<float> p) const;  // pixels -> (x, y-up)
    const orbit::gui::theme::CharacterTheme& th() const;
    bool animating() const;
    void burstSparks(float nx, float ny, orbit::gui::theme::Lch col);
    void configureTimeKnob();
    void syncKnobsFromTap();
    void updateKnobPositions();
    int nearestGridIndex(float x) const;

    std::array<TapView, 4> taps_ {};
    int sel_ = 0;
    orbit::dsp::CharacterStage::Mode mode_ = orbit::dsp::CharacterStage::Mode::Clean;
    bool freeze_ = false;
    bool warm_ = orbit::gui::theme::kDefaultWarmField;
    std::vector<SyncGridEntry> syncGrid_;
    orbit::viz::VizFeed* feed_ = nullptr;

    OrbitKnob timeKnob_, fbKnob_;
    bool timeKnobStepped_ = false;   // current mode of the TIME knob
    bool knobGuard_ = false;         // suppress write-back while mirroring
    int knobGestureTap_ = -1;        // tap a riding-knob gesture opened on

    // Drag state.
    int dragI_ = -1, hoverI_ = -1;
    juce::Point<float> grabOffset_;
    float velX_ = 0.0f, velY_ = 0.0f;     // EMA of pointer velocity (norm/s)
    float lastX_ = 0.0f, lastY_ = 0.0f;
    double lastMoveT_ = 0.0;
    float speedEma_ = 0.0f;
    float heat_ = 0.0f;

    orbpad::Glide glide_;
    int glideTap_ = 0;   // the thrown tap — the glide steers it, not sel_
    std::vector<TrailPoint> trail_;
    std::vector<Spark> sparks_;
    std::vector<Flash> flashes_;
    std::array<float, 4> fireEnv_ {};     // real TapFireEvent pulses
    double lastTick_ = 0.0;

    juce::VBlankAttachment vblank_ { this, [this] { animationTick(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbPad)
};
