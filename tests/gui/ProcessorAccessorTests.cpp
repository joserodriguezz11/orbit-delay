// Verifies the REAL OrbitAudioProcessor exposes the engine's VizFeed so the
// editor (later waves) can poll animation data. Runs in the headless console
// rig — the target links the real processor sources (see CMakeLists.txt).
#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"

TEST_CASE("processor exposes the engine VizFeed with a sane default snapshot") {
    // Same guard as tests/preset: the real processor's PresetManager touches
    // JUCE singletons (Timer/MessageManager) that need a live JUCE runtime.
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    const auto snap = proc.vizFeed().readLevels();
    CHECK(snap.duckGain == 1.0f);   // no ducking at rest
    CHECK(snap.inRms == 0.0f);
    CHECK(snap.inPeak == 0.0f);
    CHECK(snap.outRms == 0.0f);
    CHECK(snap.outPeak == 0.0f);
}
