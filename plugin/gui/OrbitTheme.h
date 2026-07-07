#pragma once

#include <juce_graphics/juce_graphics.h>
#include "dsp/CharacterStage.h"

// Orbit design tokens — ported verbatim from the approved Phase-3 UI mockup
// (docs/superpowers/specs/phase3-ui-design/2026-07-06-orbit-phase3-ui-design.html).
// Single source of truth for editor colours, type, spacing and motion.
// Values are a contract with the mockup CSS custom properties — do not tweak
// here without updating the mockup (and vice versa).
namespace orbit::gui::theme {

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

// Ember ramp (primary accent, --ember-300 … --ember-600)
inline const juce::Colour emberBright { 0xffff5c5c };  // ember-300
inline const juce::Colour ember       { 0xffe30e0e };  // ember-400, primary accent
inline const juce::Colour emberDeep   { 0xffc40a0a };  // ember-500
inline const juce::Colour emberDeeper { 0xff9e0808 };  // ember-600

// Per-mode accents (mockup mode-switch: Clean=voltage-cyan, Tape=amber,
// Grit=ember-red — active pill uses acc:'#E30E0E').
inline const juce::Colour accentClean { 0xff5bc8e6 };
inline const juce::Colour accentTape  { 0xfff2b33a };
inline const juce::Colour accentGrit  { 0xffe30e0e };

// ------------------------------------------------------------ mode accent
enum class Accent { Clean, Tape, Grit };

inline juce::Colour modeAccent(orbit::dsp::CharacterStage::Mode mode) {
    switch (mode) {
        case orbit::dsp::CharacterStage::Mode::Clean: return accentClean;
        case orbit::dsp::CharacterStage::Mode::Tape:  return accentTape;
        case orbit::dsp::CharacterStage::Mode::Grit:  return accentGrit;
        default:                                      break;
    }
    return ember;
}

// ------------------------------------------------------------- typography
inline constexpr auto fontSans    = "Schibsted Grotesk";  // --font-sans
inline constexpr auto fontDisplay = "Syne";               // --font-display
inline constexpr auto fontMono    = "Space Mono";         // --font-mono

inline constexpr float fsCaption   = 12.0f;  // --fs-caption
inline constexpr float fsMonoSm    = 12.0f;  // --fs-mono-sm
inline constexpr float fsLabel     = 13.0f;  // --fs-label
inline constexpr float fsBodySm    = 14.0f;  // --fs-body-sm
inline constexpr float fsBodyMd    = 16.0f;  // --fs-body-md
inline constexpr float fsHeadingSm = 18.0f;  // --fs-heading-sm
inline constexpr float fsHeadingMd = 22.0f;  // --fs-heading-md
inline constexpr float fsHeadingLg = 28.0f;  // --fs-heading-lg

inline constexpr int fwRegular  = 400;  // --fw-regular
inline constexpr int fwMedium   = 500;  // --fw-medium
inline constexpr int fwSemibold = 600;  // --fw-semibold
inline constexpr int fwBold     = 700;  // --fw-bold

// -------------------------------------------------------- spacing & shape
inline constexpr int gutter     = 24;  // --gutter
inline constexpr int controlHsm = 32;  // --control-h-sm
inline constexpr int controlHmd = 40;  // --control-h-md
inline constexpr int controlHlg = 52;  // --control-h-lg

inline constexpr float rXs   = 4.0f;    // --r-xs
inline constexpr float rSm   = 8.0f;    // --r-sm
inline constexpr float rMd   = 12.0f;   // --r-md
inline constexpr float rLg   = 18.0f;   // --r-lg
inline constexpr float rXl   = 24.0f;   // --r-xl
inline constexpr float rPill = 999.0f;  // --r-pill

// ------------------------------------------------------------------ motion
inline constexpr int durFastMs = 120;  // --dur-fast
inline constexpr int durBaseMs = 200;  // --dur-base
inline constexpr int durSlowMs = 360;  // --dur-slow

} // namespace orbit::gui::theme
