#pragma once

#include <cmath>
#include <juce_graphics/juce_graphics.h>
#include "dsp/CharacterStage.h"

// Orbit design tokens — ported verbatim from the approved Full v2 mockup
// (docs/design/orbit-delay-full-v2.html, imported from Claude Design).
// Single source of truth for editor colours, type, layout metrics, character
// field themes, the position→colour mapping and the pad's log time curve.
// Values are a contract with the mockup — do not tweak here without updating
// the mockup (and vice versa).
namespace orbit::gui::theme {

// ------------------------------------------------------------ layout metrics
// Re-based 2026-07-14 (spec: phase3-ui-refinements): 900x550 window — a 52px
// header, the 730x402 orb pad beside a 170px control rail, and a 96px tap
// strip. Deliberate deviation from the Full v2 mockup (README deviations).
// The four constants partition the window exactly (asserted in OrbitThemeTests).
inline constexpr int kWindowW   = 900;
inline constexpr int kWindowH   = 550;
inline constexpr int kHeaderH   = 52;
inline constexpr int kPadW      = 730;
inline constexpr int kPadH      = 402;
inline constexpr int kRailW     = kWindowW - kPadW;   // 170
inline constexpr int kTapStripH = kWindowH - kHeaderH - kPadH;  // 96

// ---------------------------------------------------------------- colours
// Ink scale (dark surfaces, --ink-900 … --ink-400)
inline const juce::Colour ink900 { 0xff08080c };
inline const juce::Colour ink850 { 0xff0c0c12 };
inline const juce::Colour ink800 { 0xff111119 };
inline const juce::Colour ink700 { 0xff181822 };
inline const juce::Colour ink600 { 0xff21212d };
inline const juce::Colour ink500 { 0xff2d2d3a };
inline const juce::Colour ink400 { 0xff3b3b4a };

// Bone scale (light text/surfaces, --bone-50 … --bone-300)
inline const juce::Colour bone50  { 0xfff5f1e8 };
inline const juce::Colour bone100 { 0xffece7da };
inline const juce::Colour bone200 { 0xffdad4c4 };
inline const juce::Colour bone300 { 0xffb8b1a0 };

// Ember ramp (the single red signature accent, --ember-300 … --ember-600)
inline const juce::Colour emberBright { 0xffff5c5c };  // ember-300
inline const juce::Colour ember       { 0xffe30e0e };  // ember-400, primary accent
inline const juce::Colour emberDeep   { 0xffc40a0a };  // ember-500
inline const juce::Colour emberDeeper { 0xff9e0808 };  // ember-600

// Panel chrome (v2 mockup TH + surfaces)
inline const juce::Colour panel { 0xff07070b };  // plugin body background
// Bone-tinted overlay alphas from the mockup TH object.
inline juce::Colour text()     { return bone50; }
inline juce::Colour textDim()  { return bone50.withAlpha(0.55f); }
inline juce::Colour textFaint(){ return bone50.withAlpha(0.38f); }
inline juce::Colour line()     { return bone50.withAlpha(0.09f); }
inline juce::Colour line2()    { return bone50.withAlpha(0.18f); }
inline juce::Colour well()     { return bone50.withAlpha(0.05f); }
inline juce::Colour track()    { return bone50.withAlpha(0.14f); }
inline juce::Colour manual()   { return bone50.withAlpha(0.80f); }

// ---------------------------------------------------------- character themes
// Each character mode retints the whole orb field: background gradient,
// halo glow gain, per-tap hue shift / saturation, scanline opacity and halo
// softness (mockup CHRS table).
struct CharacterTheme {
    juce::Colour bgTop, bgBot;
    float glow;      // halo/glow intensity multiplier
    float hueShift;  // degrees added to every tap hue
    float sat;       // chroma multiplier
    float scan;      // scanline overlay opacity (grit only)
    float soft;      // halo radius multiplier
};

inline const CharacterTheme& characterTheme(orbit::dsp::CharacterStage::Mode mode) {
    using Mode = orbit::dsp::CharacterStage::Mode;
    static const CharacterTheme clean { juce::Colour { 0xff101019 }, juce::Colour { 0xff07070b },
                                        1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
    static const CharacterTheme tape  { juce::Colour { 0xff141009 }, juce::Colour { 0xff0a0705 },
                                        1.3f, 16.0f, 0.85f, 0.0f, 1.6f };
    static const CharacterTheme grit  { juce::Colour { 0xff130709 }, juce::Colour { 0xff070304 },
                                        0.85f, -8.0f, 1.18f, 0.45f, 0.7f };
    switch (mode) {
        case Mode::Tape: return tape;
        case Mode::Grit: return grit;
        case Mode::Clean:
        default:         return clean;
    }
}

// ------------------------------------------------------- position -> colour
// Every tap's colour derives from its pad position via OKLCH (mockup posLCH):
// warm mode sweeps hue red→gold with time (x) only; full mode also swings hue
// with feedback (y). The default ships warm (mockup colorField default).
inline constexpr bool kDefaultWarmField = true;

struct Lch { float l, c, h; };

inline Lch posLch(float x, float y, bool warm) {
    if (warm)
        return { 0.55f + 0.10f * x + 0.06f * y, 0.24f - 0.05f * x, 29.0f + 61.0f * x };
    float h = 29.0f + 61.0f * x - 99.0f * y;
    if (h < 0.0f) h += 360.0f;
    return { 0.55f + 0.14f * x + 0.08f * y, 0.23f - 0.04f * x - 0.03f * y, h };
}

inline Lch tapLch(float x, float y, bool warm, const CharacterTheme& th) {
    const auto l = posLch(x, y, warm);
    return { l.l, l.c * th.sat, l.h + th.hueShift };
}

// OKLCH → sRGB (Björn Ottosson's OKLab reference matrices), clamped to gamut.
inline juce::Colour lchColour(Lch lch, float alpha) {
    const float hr = juce::degreesToRadians(lch.h);
    const float a = lch.c * std::cos(hr), b = lch.c * std::sin(hr);

    const float l_ = lch.l + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = lch.l - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = lch.l - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;

    const float rLin =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    const float gLin = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    const float bLin = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    const auto encode = [] (float c) {
        c = juce::jlimit(0.0f, 1.0f, c);
        return c <= 0.0031308f ? 12.92f * c
                               : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    };
    return juce::Colour::fromFloatRGBA(encode(rLin), encode(gLin), encode(bLin),
                                       juce::jlimit(0.0f, 1.0f, alpha));
}

// --------------------------------------------------------- pad time mapping
// The pad's X axis is the mockup's log curve: 40ms at the left edge, 900ms at
// the right (ms = 40·22.5^x). msToX clamps so parameter values outside the
// pad range (the host can automate 1–2000ms) pin to the pad edges.
inline float msOfX(float x) { return 40.0f * std::pow(22.5f, x); }
inline float msToX(float ms) {
    if (ms <= 0.0f) return 0.0f;
    return juce::jlimit(0.0f, 1.0f, std::log(ms / 40.0f) / std::log(22.5f));
}

// ------------------------------------------------------------- typography
inline constexpr auto fontSans      = "Space Grotesk";   // labels, preset names
inline constexpr auto fontMono      = "Space Mono";      // values, section heads
inline constexpr auto fontWatermark = "Syne";            // pad ORBIT watermark

inline constexpr int fwRegular  = 400;
inline constexpr int fwMedium   = 500;
inline constexpr int fwSemibold = 600;
inline constexpr int fwBold     = 700;

// -------------------------------------------------------- spacing & shape
inline constexpr float rXs   = 4.0f;
inline constexpr float rSm   = 8.0f;
inline constexpr float rMd   = 12.0f;
inline constexpr float rLg   = 18.0f;
inline constexpr float rXl   = 24.0f;
inline constexpr float rPill = 999.0f;

// ------------------------------------------------------------------ motion
inline constexpr int durFastMs = 120;  // --dur-fast
inline constexpr int durBaseMs = 200;  // --dur-base
inline constexpr int durSlowMs = 360;  // --dur-slow

} // namespace orbit::gui::theme
