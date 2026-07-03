#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include "OrbitEngine.h"

using Catch::Approx;
using orbit::OrbitEngine;
using orbit::TapSettings;
namespace dsp = orbit::dsp;

namespace {
struct StereoBuffer {
    explicit StereoBuffer(int numSamples)
        : left(static_cast<size_t>(numSamples), 0.0f),
          right(static_cast<size_t>(numSamples), 0.0f) {
        channels[0] = left.data();
        channels[1] = right.data();
    }
    std::vector<float> left, right;
    std::array<float*, 2> channels {};
};
} // namespace

TEST_CASE("single enabled tap echoes an impulse at the tap time") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 100.0f / 48000.0f;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);     // wet only
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(256);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 256);

    CHECK(buf.left[100] == Approx(1.0f));
    CHECK(buf.right[100] == Approx(1.0f));
    CHECK(std::abs(buf.left[0]) < 1e-6f);   // dry removed at mix = 1
    CHECK(std::abs(buf.left[50]) < 1e-6f);
}

TEST_CASE("all taps disabled yields silence at full wet") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(256);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 256);

    for (int n = 0; n < 256; ++n) {
        REQUIRE(std::abs(buf.left[static_cast<size_t>(n)]) < 1e-6f);
        REQUIRE(std::abs(buf.right[static_cast<size_t>(n)]) < 1e-6f);
    }
}

TEST_CASE("tempo-synced tap lands on the musical grid") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setBpm(120.0);                    // quarter note = 0.5 s = 24000 samples
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Quarter;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer buf(24100);
    buf.left[0] = 1.0f;
    buf.right[0] = 1.0f;
    engine.process(buf.channels.data(), 2, 24100);

    CHECK(buf.left[24000] == Approx(1.0f));
}

TEST_CASE("ducking reduces wet energy while the dry signal is loud") {
    auto makeEngine = [](float duckAmount) {
        auto engine = std::make_unique<OrbitEngine>();
        engine->prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 50.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine->setTap(0, tap);
        engine->setDryWet(0.5f);
        engine->setDuckAmount(duckAmount);
        return engine;
    };

    auto energyWithDuck = [&](float duckAmount) {
        auto engine = makeEngine(duckAmount);
        StereoBuffer buf(4800);
        std::fill(buf.left.begin(), buf.left.end(), 1.0f);
        std::fill(buf.right.begin(), buf.right.end(), 1.0f);
        engine->process(buf.channels.data(), 2, 4800);
        double sum = 0.0;
        for (float v : buf.left) sum += std::abs(v);
        return sum;
    };

    CHECK(energyWithDuck(1.0f) < energyWithDuck(0.0f) * 0.9);
}
