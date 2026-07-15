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

// ---------------------------------------------------------- v2 assembly

TEST_CASE("editor hosts the pad, rail and strip and wires selection across them") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(base.get());
    REQUIRE(ed != nullptr);

    // Selecting a tap in the strip reaches the pad.
    ed->tapStrip().selectRow(2);
    CHECK(ed->orbPad().selected() == 2);
    // And selecting on the pad reaches the strip.
    ed->orbPad().setSelected(0);        // silent setter
    ed->tapStrip().selectRow(1);
    CHECK(ed->orbPad().selected() == 1);
}

TEST_CASE("dragging an orb writes the tap's time and feedback parameters") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(base.get());
    REQUIRE(ed != nullptr);

    ed->orbPad().beginOrbDrag(0);
    ed->orbPad().dragOrbTo(0.5f, 0.5f);
    ed->orbPad().endOrbDrag();

    // x=0.5 on the log curve is 40*sqrt(22.5) ~= 189.7ms; y=0.5 -> fb 0.475.
    CHECK(proc.apvts.getRawParameterValue("tap1_time")->load()
          == Catch::Approx(189.7f).margin(1.0f));
    CHECK(proc.apvts.getRawParameterValue("tap1_feedback")->load()
          == Catch::Approx(0.475f).margin(0.005f));
}

TEST_CASE("dragging a synced orb snaps onto the nearest real division") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(base.get());
    REQUIRE(ed != nullptr);

    // Put tap 1 in quarter-note sync (120bpm default -> 500ms).
    auto* sync = proc.apvts.getParameter("tap1_sync");
    sync->setValueNotifyingHost(sync->convertTo0to1(float(orbit::dsp::SyncDivision::Quarter)));

    // Drag near the 250ms region: the division flips to the nearest (1/8).
    ed->orbPad().beginOrbDrag(0);
    ed->orbPad().dragOrbTo(orbit::gui::theme::msToX(250.0f), 0.4f);
    ed->orbPad().endOrbDrag();
    CHECK(int(proc.apvts.getRawParameterValue("tap1_sync")->load())
          == int(orbit::dsp::SyncDivision::Eighth));
    // Free time is untouched by synced drags.
    CHECK(proc.apvts.getRawParameterValue("tap1_time")->load()
          == Catch::Approx(350.0f).margin(0.5f));
}

// Hidden: renders the assembled editor to PNG for visual parity checks.
// Run explicitly: orbit_gui_tests "[.snapshot]"
TEST_CASE("render editor snapshot to /tmp", "[.snapshot]") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;

    // Stage a Machine-Hall-ish scene: four live taps spread over the field.
    const auto setP = [&] (const juce::String& id, float v) {
        auto* p = proc.apvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    const float xs[] = { 0.57f, 0.43f, 0.62f, 0.78f };
    const float fb[] = { 0.73f, 0.56f, 0.53f, 0.56f };
    for (int i = 0; i < 4; ++i) {
        setP("tap" + juce::String(i + 1) + "_enabled", 1.0f);
        setP("tap" + juce::String(i + 1) + "_time",
             orbit::gui::theme::msOfX(xs[i]));
        setP("tap" + juce::String(i + 1) + "_feedback", fb[i]);
    }
    setP("mix_drywet", 0.48f);

    std::unique_ptr<juce::AudioProcessorEditor> ed { proc.createEditor() };
    juce::Image img { juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true };
    juce::Graphics g { img };
    ed->paintEntireComponent(g, true);

    juce::PNGImageFormat png;
    juce::File out { "/tmp/orbit-editor-snapshot.png" };
    out.deleteFile();   // FileOutputStream appends — stale bytes poison the PNG
    juce::FileOutputStream stream { out };
    REQUIRE(stream.openedOk());
    REQUIRE(png.writeImageToStream(img, stream));
}

TEST_CASE("orb drags bracket the tap's parameters in a host automation gesture") {
    juce::ScopedJuceInitialiser_GUI juceInit;
    OrbitAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> base { proc.createEditor() };
    auto* ed = dynamic_cast<OrbitEditor*>(base.get());
    REQUIRE(ed != nullptr);

    struct GestureSpy : juce::AudioProcessorParameter::Listener {
        int begins = 0, ends = 0;
        void parameterValueChanged(int, float) override {}
        void parameterGestureChanged(int, bool starting) override {
            (starting ? begins : ends) += 1;
        }
    } spy;
    auto* fb = proc.apvts.getParameter("tap1_feedback");
    fb->addListener(&spy);

    ed->orbPad().beginOrbDrag(0);
    CHECK(spy.begins == 1);
    CHECK(spy.ends == 0);
    ed->orbPad().dragOrbTo(0.5f, 0.5f);
    for (int i = 0; i < 10; ++i)
        ed->orbPad().dragOrbTo(0.5f, 0.5f);   // decelerate under the flick EMA
    ed->orbPad().endOrbDrag();   // slow release: no glide, gesture closes now
    CHECK(spy.ends == 1);

    fb->removeListener(&spy);
}
