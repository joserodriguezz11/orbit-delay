// Verifies OrbitLookAndFeel actually lays down ink when asked to draw a
// rotary slider and a toggle button. Each test renders into an image created
// fully transparent (juce::Image::ARGB with clearImage=true) and then asserts
// at least one pixel became non-transparent — a check that FAILS if the draw
// method is a no-op.
//
// NOTE — deliberate deviations from the task brief's Step-1 example test,
// per controller instruction:
//   1. The example passed `*(juce::Slider*)nullptr` (undefined behavior).
//      A real juce::Slider is constructed on the stack instead, as the
//      brief's own footnote permits.
//   2. The example asserted `getPixelAt(40,40).getAlpha() >= 0`, which is a
//      tautology (getAlpha() returns uint8). The brief's stated intent —
//      "a non-transparent pixel proves it drew" — is implemented for real:
//      scan the whole image for any pixel with alpha > 0. A single fixed
//      coordinate is unreliable (the arc hugs the edge; the centre holds
//      text), so the scan covers the full image.
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitLookAndFeel.h"

namespace {

// True if any pixel in the image is non-transparent (alpha > 0).
bool imageHasInk(const juce::Image& img) {
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() > 0)
                return true;
    return false;
}

} // namespace

TEST_CASE("OrbitLookAndFeel::drawRotarySlider lays down non-transparent pixels") {
    juce::ScopedJuceInitialiser_GUI juceInit; // fonts/graphics need a live JUCE runtime
    OrbitLookAndFeel laf;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag,
                          juce::Slider::NoTextBox };
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5, juce::dontSendNotification);

    juce::Image img { juce::Image::ARGB, 80, 80, true }; // cleared: fully transparent
    REQUIRE_FALSE(imageHasInk(img)); // sanity: canvas starts blank

    juce::Graphics g { img };
    laf.drawRotarySlider(g, 0, 0, 80, 80, 0.5f,
                         0.0f, juce::MathConstants<float>::twoPi * 0.8f, slider);

    // Fails if drawRotarySlider is a no-op: the image would still be blank.
    CHECK(imageHasInk(img));
}

TEST_CASE("OrbitLookAndFeel::drawToggleButton lays down non-transparent pixels") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitLookAndFeel laf;
    juce::ToggleButton button { "Sync" };
    button.setSize(120, 32); // drawToggleButton reads the button's local bounds

    SECTION("off state") {
        juce::Image img { juce::Image::ARGB, 120, 32, true };
        REQUIRE_FALSE(imageHasInk(img));
        juce::Graphics g { img };
        laf.drawToggleButton(g, button, false, false);
        CHECK(imageHasInk(img));
    }

    SECTION("on state") {
        button.setToggleState(true, juce::dontSendNotification);
        juce::Image img { juce::Image::ARGB, 120, 32, true };
        REQUIRE_FALSE(imageHasInk(img));
        juce::Graphics g { img };
        laf.drawToggleButton(g, button, false, false);
        CHECK(imageHasInk(img));
    }
}
