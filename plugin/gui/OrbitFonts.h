#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Embedded design fonts (Full v2 mockup): IBM Plex Mono for values, labels
// and section heads; Archivo for preset names and the wordmark; Syne for the
// pad watermark. Loaded once from OrbitFontBinary so rendering matches the
// mockup on machines without the fonts installed. All faces are OFL-licensed.
namespace orbit::gui::fonts {

juce::Font mono(float height);          // IBM Plex Mono Regular (400)
juce::Font monoMedium(float height);    // IBM Plex Mono Medium (500)
juce::Font monoSemiBold(float height);  // IBM Plex Mono SemiBold (600)
juce::Font monoBold(float height);      // IBM Plex Mono Bold (700)
juce::Font sans(float height);          // Archivo Medium (500)
juce::Font sansSemiBold(float height);  // Archivo SemiBold (600)
juce::Font sansExtraBold(float height); // Archivo ExtraBold (800)
juce::Font watermark(float height);     // Syne ExtraBold (800)

// Mockup letter-spacing (e.g. 0.26em section heads) as em-relative tracking.
juce::Font tracked(const juce::Font& base, float emTracking);

} // namespace orbit::gui::fonts
