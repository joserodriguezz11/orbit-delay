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

TEST_CASE("preset browser grid fits all 40 factory presets; overflow is unclickable") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitPresetBrowser browser { proc.presetManager() };
    browser.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kWindowH);
    REQUIRE(browser.visibleCount() == 40);

    // Every factory preset's row sits fully inside the panel; the first row
    // past the grid capacity does not (and is therefore neither drawn nor
    // clickable — paint and hit-testing share rowFits).
    for (int rw = 0; rw < 40; ++rw) {
        INFO("row " << rw);
        CHECK(browser.rowFits(rw));
    }
    CHECK_FALSE(browser.rowFits(40));

    // Clicking the centre of the last row loads the 40th preset.
    const auto pos = browser.rowBounds(39).getCentre();
    const auto name = browser.visibleName(39);
    REQUIRE(name.isNotEmpty());
    juce::MouseEvent up { juce::Desktop::getInstance().getMainMouseSource(),
                          pos, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          &browser, &browser, juce::Time::getCurrentTime(), pos,
                          juce::Time::getCurrentTime(), 1, false };
    browser.mouseUp(up);
    CHECK(proc.presetManager().currentPresetName() == name);
}

TEST_CASE("header children and the gear slot do not overlap at base size") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    header.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kHeaderH);

    std::vector<juce::Rectangle<int>> rects;
    for (auto* c : header.getChildren())
        if (c->isVisible() && !c->getBounds().isEmpty())
            rects.push_back(c->getBounds());
    rects.push_back(header.gearBounds());

    for (size_t i = 0; i < rects.size(); ++i)
        for (size_t j = i + 1; j < rects.size(); ++j) {
            INFO("rect " << int(i) << " vs " << int(j));
            CHECK(!rects[i].intersects(rects[j]));
        }
    // Everything must fit inside the header.
    for (const auto& r : rects)
        CHECK(header.getLocalBounds().contains(r));
}

TEST_CASE("A/B slot buttons carry no LookAndFeel text (letters painted once)") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    CHECK(header.slotAButton().getButtonText().isEmpty());
    CHECK(header.slotBButton().getButtonText().isEmpty());
}

TEST_CASE("preset arrows carry no LookAndFeel text (glyphs painted once)") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    CHECK(header.prevButton().getButtonText().isEmpty());
    CHECK(header.nextButton().getButtonText().isEmpty());
}

TEST_CASE("header draws the Orbitum planet mark as a solid bone disc") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    OrbitHeader header { proc };
    header.setBounds(0, 0, orbit::gui::theme::kWindowW, orbit::gui::theme::kHeaderH);

    juce::Image img { juce::Image::ARGB, header.getWidth(), header.getHeight(), true };
    juce::Graphics g { img };
    header.paintEntireComponent(g, true);

    // The planet disc is a filled bone-50 circle — a solid block of near-white
    // pixels in the icon slot. The old ring-outline logo never filled anything
    // bone, so this count proves the new mark is being drawn.
    int bone = 0;
    for (int y = 6; y < 46; ++y)
        for (int x = 10; x < 48; ++x) {
            const auto c = img.getPixelAt(x, y);
            if (c.getRed() > 220 && c.getGreen() > 215 && c.getBlue() > 200)
                ++bone;
        }
    CHECK(bone > 100);
}
