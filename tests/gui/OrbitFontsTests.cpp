// Verifies the embedded design fonts load as real typefaces (not system
// fallbacks): the Full v2 mockup renders values/labels in IBM Plex Mono,
// preset names in Archivo, and the pad watermark in Syne. Loading from
// OrbitFontBinary means the plugin looks right on machines without the
// fonts installed.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbitFonts.h"

TEST_CASE("embedded fonts resolve to the design families") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    using namespace orbit::gui;

    CHECK(fonts::mono(12.0f).getTypefaceName().contains("IBM Plex Mono"));
    CHECK(fonts::monoSemiBold(12.0f).getTypefaceName().contains("IBM Plex Mono"));
    CHECK(fonts::sans(12.0f).getTypefaceName().contains("Archivo"));
    CHECK(fonts::sansSemiBold(12.0f).getTypefaceName().contains("Archivo"));
    CHECK(fonts::watermark(190.0f).getTypefaceName().contains("Syne"));
}

TEST_CASE("tracked() applies em-relative letter spacing") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    using namespace orbit::gui;
    const auto base = fonts::mono(10.0f);
    const auto wide = fonts::tracked(base, 0.26f);
    CHECK(wide.getExtraKerningFactor() == Catch::Approx(0.26f));
    // Height untouched.
    CHECK(wide.getHeight() == Catch::Approx(base.getHeight()));
}
