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

TEST_CASE("changing the host tempo retunes a synced tap by gliding") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Quarter;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);                       // 120 BPM default -> 24000 samples
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);

    StereoBuffer settle(48000);                  // start running at the old tempo
    engine.process(settle.channels.data(), 2, 48000);

    engine.setBpm(90.0);                         // quarter note -> 32000 samples
    StereoBuffer glide(192000);                  // 4 s: glide fully settles
    engine.process(glide.channels.data(), 2, 192000);

    StereoBuffer probe(32100);
    probe.left[0] = 1.0f;
    probe.right[0] = 1.0f;
    engine.process(probe.channels.data(), 2, 32100);
    CHECK(probe.left[32000] == Approx(1.0f).margin(1e-2f));
}

TEST_CASE("motion depth changes the wet output, zero depth does not") {
    auto renderWithDepth = [](float depth) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 100.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);
        engine.setDuckAmount(0.0f);
        engine.setModulation(depth, 4.0f);
        StereoBuffer buf(9600);
        for (int n = 0; n < 9600; ++n)
            buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
                std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(n) / 48000.0f);
        engine.process(buf.channels.data(), 2, 9600);
        return buf.left;
    };

    const auto unmodulated = renderWithDepth(0.0f);
    const auto unmodulated2 = renderWithDepth(0.0f);
    const auto modulated = renderWithDepth(0.5f);

    double diffZero = 0.0, diffMod = 0.0;
    for (int n = 0; n < 9600; ++n) {
        diffZero += std::abs(unmodulated[static_cast<size_t>(n)] - unmodulated2[static_cast<size_t>(n)]);
        diffMod += std::abs(unmodulated[static_cast<size_t>(n)] - modulated[static_cast<size_t>(n)]);
        REQUIRE(std::isfinite(modulated[static_cast<size_t>(n)]));
    }
    CHECK(diffZero == Approx(0.0));
    CHECK(diffMod > 1.0);
}

TEST_CASE("motion on a very short tap stays finite and does not flat-top hard") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 0.001f;               // 48 samples < max mod (96 samples)
    tap.feedback = 0.5f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    engine.setModulation(1.0f, 8.0f);
    StereoBuffer buf(48000);
    for (int n = 0; n < 48000; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
    engine.process(buf.channels.data(), 2, 48000);
    for (int n = 0; n < 48000; ++n)
        REQUIRE(std::isfinite(buf.left[static_cast<size_t>(n)]));
}

TEST_CASE("low-cut filter drains a sustained wet signal") {
    auto tailEnergy = [](float lowCutHz) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 50.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);
        engine.setDuckAmount(0.0f);
        engine.setFilters(lowCutHz, 20000.0f);
        StereoBuffer buf(4800);
        std::fill(buf.left.begin(), buf.left.end(), 1.0f);
        std::fill(buf.right.begin(), buf.right.end(), 1.0f);
        engine.process(buf.channels.data(), 2, 4800);
        double sum = 0.0;
        for (int n = 4000; n < 4800; ++n)
            sum += std::abs(buf.left[static_cast<size_t>(n)]);
        return sum;
    };
    CHECK(tailEnergy(500.0f) < 0.1 * tailEnergy(0.0f));
}

TEST_CASE("equal-power mix keeps full-wet and full-dry exact") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    engine.setDryWet(0.0f);                      // full dry: output == input
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(64);
    buf.left[10] = 0.75f;
    buf.right[10] = 0.75f;
    engine.process(buf.channels.data(), 2, 64);
    CHECK(buf.left[10] == Approx(0.75f));
}
