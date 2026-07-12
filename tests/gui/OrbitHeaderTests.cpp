// Header (logo / preset nav / A/B / character seg / FREEZE / meters) and the
// preset browser overlay, bound to the real processor's PresetManager + APVTS.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/OrbitHeader.h"
#include "gui/OrbitPresetBrowser.h"
#include "PluginProcessor.h"

using Catch::Approx;

TEST_CASE("preset prev/next walk the factory list and load presets") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };

    REQUIRE(proc.presetManager().factoryPresets().size() == 40);
    CHECK(proc.presetManager().currentPresetName().isEmpty());

    header.nextPreset();
    const auto first = proc.presetManager().factoryPresets()[0].name;
    CHECK(proc.presetManager().currentPresetName() == first);

    header.nextPreset();
    CHECK(proc.presetManager().currentPresetName()
          == proc.presetManager().factoryPresets()[1].name);

    header.prevPreset();
    CHECK(proc.presetManager().currentPresetName() == first);
}

TEST_CASE("character seg and freeze pill bind to their parameters") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };

    header.characterSeg().setSelectedIndex(2, juce::sendNotification);
    CHECK(int(proc.apvts.getRawParameterValue("character_mode")->load()) == 2);

    header.freezePill().setToggleState(true, juce::sendNotification);
    CHECK(proc.apvts.getRawParameterValue("freeze_active")->load() == Approx(1.0f));
}

TEST_CASE("A/B buttons switch the live preset slot") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };

    CHECK_FALSE(proc.presetManager().isSlotB());
    header.selectSlot(true);   // B
    CHECK(proc.presetManager().isSlotB());
    header.selectSlot(true);   // already live: no toggle
    CHECK(proc.presetManager().isSlotB());
    header.selectSlot(false);  // back to A
    CHECK_FALSE(proc.presetManager().isSlotB());
}

TEST_CASE("preset browser filters by tag and loads on pick") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitPresetBrowser browser { proc.presetManager() };

    // ALL shows the full factory set.
    CHECK(browser.visibleCount() == 40);
    // One chip per vocabulary tag plus ALL.
    CHECK(browser.tagCount() == orbit::PresetManager::tagVocabulary().size() + 1);

    browser.setTagFilter("Rhythm");
    CHECK(browser.visibleCount() > 0);
    CHECK(browser.visibleCount() < 40);

    const auto name = browser.visibleName(0);
    browser.pickVisible(0);
    CHECK(proc.presetManager().currentPresetName() == name);

    browser.setTagFilter({});   // back to ALL
    CHECK(browser.visibleCount() == 40);
}
