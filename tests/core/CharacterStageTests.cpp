#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/CharacterStage.h"

using Catch::Approx;
using orbit::dsp::CharacterStage;

TEST_CASE("Clean mode is a true bypass") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Clean);
    for (int n = 0; n < 100; ++n) {
        const float x = std::sin(0.1f * static_cast<float>(n)) * 1.3f;
        REQUIRE(stage.processSample(x) == x);
    }
}

TEST_CASE("Tape mode saturates softly and stays bounded") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Tape);
    float outAt2 = 0.0f;
    for (int n = 0; n < 100; ++n)
        outAt2 = stage.processSample(2.0f);
    CHECK(outAt2 < 1.2f);
    CHECK(outAt2 > 0.5f);
    stage.reset();
    float outSmall = 0.0f;
    for (int n = 0; n < 100; ++n)
        outSmall = stage.processSample(0.1f);
    CHECK(outSmall == Approx(0.1f).margin(0.02f));   // near-linear at low level
}

TEST_CASE("Tape mode rolls off highs harder than Clean") {
    auto nyquistGain = [](CharacterStage::Mode mode) {
        CharacterStage stage;
        stage.prepare(48000.0);
        stage.setMode(mode);
        float sumAbs = 0.0f;
        for (int n = 0; n < 4800; ++n) {
            const float y = stage.processSample((n % 2 == 0) ? 0.5f : -0.5f);
            if (n >= 4700) sumAbs += std::abs(y);
        }
        return sumAbs;
    };
    CHECK(nyquistGain(CharacterStage::Mode::Tape) < 0.5f * nyquistGain(CharacterStage::Mode::Clean));
}

TEST_CASE("Grit noise is gated: silence in, silence out") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Grit);
    for (int n = 0; n < 48000; ++n)
        REQUIRE(stage.processSample(0.0f) == 0.0f);
}

TEST_CASE("Grit adds a noise floor while signal is present") {
    CharacterStage stage;
    stage.prepare(48000.0);
    stage.setMode(CharacterStage::Mode::Grit);
    // steady input -> settled output would be constant without noise; variance implies noise
    float prev = stage.processSample(0.5f);
    float maxDelta = 0.0f;
    for (int n = 0; n < 4800; ++n) {
        const float y = stage.processSample(0.5f);
        if (n > 4000) maxDelta = std::max(maxDelta, std::abs(y - prev));
        prev = y;
    }
    CHECK(maxDelta > 1e-4f);
    CHECK(maxDelta < 0.05f);
}
