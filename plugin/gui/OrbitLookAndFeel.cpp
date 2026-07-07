#include "OrbitLookAndFeel.h"
#include "OrbitTheme.h"

namespace theme = orbit::gui::theme;

namespace {

juce::Font monoFont(float height) {
    return juce::Font { juce::FontOptions { theme::fontMono, height,
                                            juce::Font::plain } };
}

juce::Font sansFont(float height) {
    return juce::Font { juce::FontOptions { theme::fontSans, height,
                                            juce::Font::plain } };
}

} // namespace

OrbitLookAndFeel::OrbitLookAndFeel() : accent_(theme::ember) {
    setColour(juce::Slider::textBoxTextColourId, theme::bone100);
    setColour(juce::Label::textColourId, theme::bone200);
    setColour(juce::ToggleButton::textColourId, theme::bone200);
}

void OrbitLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                        int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider) {
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const float lineW  = juce::jmax(2.0f, radius * 0.08f); // thin arc
    const float arcR   = radius - lineW / 2.0f;
    const auto centre  = bounds.getCentre();
    const float toAngle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Track: full sweep, thin arc in ink600.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(theme::ink600);
    g.strokePath(track, juce::PathStrokeType { lineW,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded });

    // Value arc: start -> current position, in the active accent.
    if (toAngle > rotaryStartAngle) {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f,
                            rotaryStartAngle, toAngle, true);
        g.setColour(accent_);
        g.strokePath(value, juce::PathStrokeType { lineW,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded });
    }

    // Centred mono value readout.
    const float fontH = juce::jlimit(theme::fsMonoSm, theme::fsHeadingMd,
                                     radius * 0.34f);
    g.setColour(theme::bone100);
    g.setFont(monoFont(fontH));
    g.drawText(slider.getTextFromValue(slider.getValue()),
               bounds.reduced(lineW), juce::Justification::centred, false);
}

void OrbitLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown) {
    const auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    // Pill switch track on the left, sized to the button height.
    const float trackH = juce::jmin(bounds.getHeight() * 0.7f, 22.0f);
    const float trackW = trackH * 1.8f;
    const juce::Rectangle<float> track {
        bounds.getX(), bounds.getCentreY() - trackH / 2.0f, trackW, trackH };

    auto trackColour = on ? accent_ : theme::ink600;
    if (shouldDrawButtonAsDown)
        trackColour = trackColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        trackColour = trackColour.brighter(0.15f);

    g.setColour(trackColour);
    g.fillRoundedRectangle(track, trackH / 2.0f);

    // Thumb: bone circle, left when off, right when on.
    const float thumbD = trackH - 4.0f;
    const float thumbX = on ? track.getRight() - thumbD - 2.0f
                            : track.getX() + 2.0f;
    g.setColour(on ? theme::bone50 : theme::bone300);
    g.fillEllipse(thumbX, track.getCentreY() - thumbD / 2.0f, thumbD, thumbD);

    // Label to the right of the switch.
    const auto text = button.getButtonText();
    if (text.isNotEmpty()) {
        g.setColour(on ? theme::bone100 : theme::bone300);
        g.setFont(sansFont(theme::fsLabel));
        auto textArea = bounds.withTrimmedLeft(trackW + 8.0f);
        g.drawText(text, textArea, juce::Justification::centredLeft, false);
    }
}
