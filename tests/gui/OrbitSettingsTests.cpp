// Settings overlay: gear toggle, close paths, mutual exclusion with the
// preset browser. Mirrors the browser-overlay test idiom.
#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitEditor.h"
#include "gui/OrbitSettingsPanel.h"
#include "PluginProcessor.h"

TEST_CASE("gear toggle shows and hides the settings panel") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> raw { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(raw.get());
    REQUIRE(ed != nullptr);

    CHECK(!ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(!ed->settingsPanel().isVisible());
}

TEST_CASE("settings and preset browser are mutually exclusive") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> raw { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(raw.get());
    REQUIRE(ed != nullptr);

    ed->header().onSettingsToggle();
    REQUIRE(ed->settingsPanel().isVisible());
    ed->header().onBrowserToggle();
    CHECK(ed->presetBrowser().isVisible());
    CHECK(!ed->settingsPanel().isVisible());
    ed->header().onSettingsToggle();
    CHECK(ed->settingsPanel().isVisible());
    CHECK(!ed->presetBrowser().isVisible());
}

TEST_CASE("clicking outside the panel closes settings") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitSettingsPanel panel;
    bool closed = false;
    panel.onClose = [&closed] { closed = true; };
    panel.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kWindowH);

    const auto outside = panel.panelBounds().getBottomRight() + juce::Point<float>(20.0f, 20.0f);
    juce::MouseEvent up { juce::Desktop::getInstance().getMainMouseSource(),
                          outside, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          &panel, &panel, juce::Time::getCurrentTime(), outside,
                          juce::Time::getCurrentTime(), 1, false };
    panel.mouseUp(up);
    CHECK(closed);
}
