// Verifies createEditor() returns the real OrbitEditor shell (not the
// placeholder GenericAudioProcessorEditor): opens at base size, resizable
// within 100-200%, and exposes the single proportional scale() factor.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
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
