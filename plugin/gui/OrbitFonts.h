#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Embedded design fonts (Full v2 mockup): Space Mono for values, labels
// and section heads; Space Grotesk for preset names and the wordmark; Syne
// for the pad watermark. Loaded once from OrbitFontBinary so rendering
// matches the mockup on machines without the fonts installed. All faces are
// OFL-licensed. Space Mono ships only Regular (400) and Bold (700); Space
// Grotesk ships only Medium (500) and Bold (700) — the semibold/extrabold
// accessors below deliberately double up on the nearest available weight.
namespace orbit::gui::fonts {

juce::Font mono(float height);          // Space Mono Regular (400)
juce::Font monoMedium(float height);    // Space Mono Regular (400)
juce::Font monoSemiBold(float height);  // Space Mono Bold (700)
juce::Font monoBold(float height);      // Space Mono Bold (700)
juce::Font sans(float height);          // Space Grotesk Medium (500)
juce::Font sansSemiBold(float height);  // Space Grotesk Bold (700)
juce::Font sansExtraBold(float height); // Space Grotesk Bold (700)
juce::Font watermark(float height);     // Syne ExtraBold (800)

// Mockup letter-spacing (e.g. 0.26em section heads) as em-relative tracking.
juce::Font tracked(const juce::Font& base, float emTracking);

} // namespace orbit::gui::fonts
