// Verifies createEditor() returns the real OrbitEditor shell (not the
// placeholder GenericAudioProcessorEditor): opens at base size, resizable
// within 100-200%, and exposes the single proportional scale() factor.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "gui/OrbitEditor.h"
#include "PluginProcessor.h"

using Catch::Approx;

TEST_CASE("OrbitEditor opens at base size and is resizable within 100-200%") {
    // Same guard as the other gui tests: the real processor + JUCE editor
    // need a live JUCE runtime (MessageManager) even headless.
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed { proc.createEditor() };
    REQUIRE(ed != nullptr);
    CHECK(dynamic_cast<juce::GenericAudioProcessorEditor*>(ed.get()) == nullptr); // real editor
    CHECK(ed->getWidth()  == OrbitEditor::kBaseW);
    CHECK(ed->getHeight() == OrbitEditor::kBaseH);
    CHECK(ed->isResizable());
    ed->setSize(OrbitEditor::kBaseW * 2, OrbitEditor::kBaseH * 2);   // 200%
    CHECK(dynamic_cast<OrbitEditor*>(ed.get())->scale() == Approx(2.0));
}

TEST_CASE("OrbitEditor's installed constrainer clamps resizes to 100-200% at fixed aspect") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed { proc.createEditor() };
    REQUIRE(ed != nullptr);

    // setSize() bypasses the constrainer entirely (Component::setSize calls
    // setBounds directly), so drive the resize the way a host drag does:
    // through the constrainer installed via setConstrainer(). getConstrainer()
    // returns the editor's default, limitless constrainer if OrbitEditor never
    // installed its own — every section below would then fail.
    auto* constrainer = ed->getConstrainer();
    REQUIRE(constrainer != nullptr);

    constexpr double kAspect = double(OrbitEditor::kBaseW) / double(OrbitEditor::kBaseH);
    auto aspectOf = [&ed] { return ed->getWidth() / double(ed->getHeight()); };

    SECTION("oversize request is clamped down to 200% keeping the aspect") {
        constrainer->setBoundsForComponent(ed.get(), { 0, 0, 5000, 3000 },
                                           false, false, false, false);
        CHECK(ed->getWidth()  <= OrbitEditor::kBaseW * 2);
        CHECK(ed->getHeight() <= OrbitEditor::kBaseH * 2);
        CHECK(std::abs(aspectOf() - kAspect) < 0.02);
        CHECK(dynamic_cast<OrbitEditor*>(ed.get())->scale() == Approx(2.0));
    }

    SECTION("undersize request is clamped up to 100% keeping the aspect") {
        constrainer->setBoundsForComponent(ed.get(), { 0, 0, 100, 100 },
                                           false, false, false, false);
        CHECK(ed->getWidth()  >= OrbitEditor::kBaseW);
        CHECK(ed->getHeight() >= OrbitEditor::kBaseH);
        CHECK(std::abs(aspectOf() - kAspect) < 0.02);
    }

    SECTION("in-range but off-aspect request is corrected to the fixed aspect") {
        // 2000x800 sits inside the size limits, so only setFixedAspectRatio()
        // can reject it. The size limits alone would leave this bounds intact
        // (and clamp the over/undersize cases above to exactly-on-aspect
        // corners), so this is the section that fails if the aspect lock is
        // missing while the min/max sizes happen to be right.
        constrainer->setBoundsForComponent(ed.get(), { 0, 0, 2000, 800 },
                                           false, false, false, false);
        CHECK(std::abs(aspectOf() - kAspect) < 0.02);
        CHECK(ed->getWidth()  >= OrbitEditor::kBaseW);
        CHECK(ed->getWidth()  <= OrbitEditor::kBaseW * 2);
        CHECK(ed->getHeight() >= OrbitEditor::kBaseH);
        CHECK(ed->getHeight() <= OrbitEditor::kBaseH * 2);
    }
}
