#include <catch2/catch_test_macros.hpp>
#include "gui/OrbitTheme.h"
#include "dsp/CharacterStage.h"

TEST_CASE("modeAccent maps each character mode to a distinct accent colour") {
    using orbit::dsp::CharacterStage;
    const auto clean = orbit::gui::theme::modeAccent(CharacterStage::Mode::Clean);
    const auto tape  = orbit::gui::theme::modeAccent(CharacterStage::Mode::Tape);
    const auto grit  = orbit::gui::theme::modeAccent(CharacterStage::Mode::Grit);
    CHECK(clean != tape);
    CHECK(tape  != grit);
    CHECK(clean == orbit::gui::theme::accentClean);
}

TEST_CASE("ink and bone scales are ordered dark-to-light by luminance") {
    using namespace orbit::gui::theme;
    CHECK(ink900.getPerceivedBrightness() < ink600.getPerceivedBrightness());
    CHECK(bone300.getPerceivedBrightness() < bone50.getPerceivedBrightness());
}
