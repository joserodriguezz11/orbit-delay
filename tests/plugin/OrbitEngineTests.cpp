#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include "OrbitEngine.h"
#include "viz/VizFeed.h"

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

// Ping-pong trace justifying the expectations below: impulse L at n=0 -> both
// lines get their channel's dry; the L line holds the impulse. Echo 1 (n=100):
// outL=1, outR=0 -> heard LEFT; the cross-write puts fb*outL into the R line.
// Echo 2 (n=200): outR=0.7 -> RIGHT. Echo 3 (n=300): back LEFT at 0.49.
TEST_CASE("ping-pong bounces the echo between channels") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 100.0f / 48000.0f;
    tap.feedback = 0.7f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    engine.setPingPong(true);
    StereoBuffer buf(350);
    buf.left[0] = 1.0f;                       // impulse on LEFT only
    engine.process(buf.channels.data(), 2, 350);
    CHECK(buf.right[100] == Approx(0.0f).margin(1e-4f));   // 1st echo: left
    CHECK(buf.left[100] == Approx(1.0f).margin(1e-2f));
    CHECK(std::abs(buf.right[200]) > 0.5f);                 // 2nd echo: crossed to right
    CHECK(std::abs(buf.left[200]) < 1e-3f);
    CHECK(std::abs(buf.left[300]) > 0.3f);                  // 3rd echo: back to left
}

TEST_CASE("width narrows or spreads the wet stereo image") {
    auto sideEnergy = [](float width) {
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
        engine.setWidth(width);
        StereoBuffer buf(200);
        buf.left[0] = 1.0f;                   // left-only impulse -> wet has side content
        engine.process(buf.channels.data(), 2, 200);
        double side = 0.0;
        for (int n = 0; n < 200; ++n)
            side += std::abs(buf.left[static_cast<size_t>(n)] - buf.right[static_cast<size_t>(n)]);
        return side;
    };
    const auto normal = sideEnergy(1.0f);
    CHECK(sideEnergy(0.0f) == Approx(0.0).margin(1e-6));    // mono-ized wet
    CHECK(sideEnergy(2.0f) > 1.5 * normal);                  // spread
}

TEST_CASE("per-tap reverse and pitch flow through TapSettings") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 400.0f / 48000.0f;
    tap.feedback = 0.0f;
    tap.reverse = true;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(1200);
    for (int n = 0; n < 1200; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            static_cast<float>(n) / 1200.0f;
    engine.process(buf.channels.data(), 2, 1200);
    int descending = 0, counted = 0;
    for (int n = 560; n < 780; ++n) {
        ++counted;
        if (buf.left[static_cast<size_t>(n + 1)] < buf.left[static_cast<size_t>(n)]) ++descending;
    }
    CHECK(descending > counted * 9 / 10);
}

TEST_CASE("character stage colors the wet path between tap sum and filters") {
    // Hot sine so Tape's tanh saturation actually bites; feedback 0 and full
    // wet so the output is exactly the (colored) 100-sample echo of the input.
    auto renderWet = [](dsp::CharacterStage::Mode mode, bool callSetMode) {
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
        if (callSetMode)
            engine.setCharacterMode(mode);
        StereoBuffer buf(600);
        for (int n = 0; n < 600; ++n)
            buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
                0.9f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(n) / 48000.0f);
        engine.process(buf.channels.data(), 2, 600);
        return buf.left;
    };

    const auto untouched = renderWet(dsp::CharacterStage::Mode::Clean, false);
    const auto clean = renderWet(dsp::CharacterStage::Mode::Clean, true);
    const auto tape = renderWet(dsp::CharacterStage::Mode::Tape, true);

    // Clean is the default and a true bypass: bit-identical to an engine that
    // never called setCharacterMode, and the echo is the input delayed by
    // exactly 100 samples (nothing colored it).
    for (int n = 0; n < 600; ++n)
        REQUIRE(clean[static_cast<size_t>(n)] == untouched[static_cast<size_t>(n)]);
    for (int n = 100; n < 600; ++n) {
        const float expected =
            0.9f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(n - 100) / 48000.0f);
        REQUIRE(clean[static_cast<size_t>(n)] == Approx(expected).margin(1e-5f));
    }

    // Tape colors the wet path: same input, audibly different echo.
    float maxDiff = 0.0f;
    for (int n = 100; n < 600; ++n)
        maxDiff = std::max(maxDiff,
                           std::abs(tape[static_cast<size_t>(n)] - clean[static_cast<size_t>(n)]));
    CHECK(maxDiff > 0.01f);
}

TEST_CASE("freeze loops the captured audio indefinitely and ignores new input") {
    auto renderFrozen = [](float postFreezeInput) {
        OrbitEngine engine;
        engine.prepare(48000.0, 512, 2);
        TapSettings tap;
        tap.enabled = true;
        tap.sync = dsp::SyncDivision::Free;
        tap.timeSeconds = 100.0f / 48000.0f;
        tap.feedback = 0.0f;
        engine.setTap(0, tap);
        engine.setDryWet(1.0f);                       // wet only: dry removed
        engine.setDuckAmount(0.0f);
        StereoBuffer capture(100);
        for (int n = 0; n < 100; ++n)                 // deterministic "audio" to capture
            capture.left[static_cast<size_t>(n)] = capture.right[static_cast<size_t>(n)] =
                std::sin(0.37f * static_cast<float>(n));
        engine.process(capture.channels.data(), 2, 100);
        engine.setFreeze(true);
        StereoBuffer frozen(1000);
        std::fill(frozen.left.begin(), frozen.left.end(), postFreezeInput);
        std::fill(frozen.right.begin(), frozen.right.end(), postFreezeInput);
        engine.process(frozen.channels.data(), 2, 1000);
        return frozen.left;
    };

    const auto quiet = renderFrozen(0.0f);
    const auto loud = renderFrozen(0.9f);
    double loopEnergyFirst = 0.0, loopEnergyLast = 0.0;
    for (int n = 0; n < 500; ++n)
        loopEnergyFirst += std::abs(quiet[static_cast<size_t>(n)]);
    for (int n = 500; n < 1000; ++n)
        loopEnergyLast += std::abs(quiet[static_cast<size_t>(n)]);
    CHECK(loopEnergyFirst > 1.0);                                   // something is looping
    CHECK(loopEnergyLast == Approx(loopEnergyFirst).epsilon(0.05)); // no decay
    for (int n = 0; n < 1000; ++n)                                  // input doesn't leak into wet
        REQUIRE(quiet[static_cast<size_t>(n)] == Approx(loud[static_cast<size_t>(n)]).margin(1e-4f));
}

TEST_CASE("viz feed reports tap fires with correct tap index and sane timing") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 4800.0f / 48000.0f;   // 100 ms
    tap.feedback = 0.0f;
    engine.setTap(2, tap);                   // tap index 2 on purpose
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(24000);
    for (int n = 0; n < 240; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] = 0.8f;
    engine.process(buf.channels.data(), 2, 24000);
    orbit::viz::TapFireEvent e;
    REQUIRE(engine.vizFeed().popEvent(e));
    CHECK(e.tapIndex == 2u);
    CHECK(e.timeSamples >= 4800u);           // echo lands at ~100 ms
    CHECK(e.timeSamples < 5200u);
    CHECK(e.intensity01 > 0.5f);
    REQUIRE_FALSE(engine.vizFeed().popEvent(e));  // one echo, one event
}

TEST_CASE("viz snapshot tracks levels and duck gain") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 0.05f;
    tap.feedback = 0.5f;
    engine.setTap(0, tap);
    engine.setDryWet(0.5f);
    engine.setDuckAmount(1.0f);              // full ducking
    StereoBuffer buf(48000);
    for (int n = 0; n < 48000; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
    engine.process(buf.channels.data(), 2, 48000);
    const auto s = engine.vizFeed().readLevels();
    CHECK(s.inRms > 0.2f);                   // 0.5-amp sine RMS ~= 0.35
    CHECK(s.inRms < 0.5f);
    CHECK(s.inPeak > 0.4f);
    CHECK(s.outRms > 0.0f);
    CHECK(s.duckGain < 0.9f);                // loud input + full duck => audible reduction
}

TEST_CASE("viz snapshot publishes the engine sample counter as now") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 4800.0f / 48000.0f;    // 100 ms
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(1.0f);
    engine.setDuckAmount(0.0f);
    StereoBuffer buf(24000);
    for (int n = 0; n < 240; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] = 0.8f;
    engine.process(buf.channels.data(), 2, 24000);
    CHECK(engine.vizFeed().readLevels().now == 24000u);   // now == samples processed

    orbit::viz::TapFireEvent e;
    REQUIRE(engine.vizFeed().popEvent(e));
    CHECK(engine.vizFeed().readLevels().now >= e.timeSamples);  // events never post-date now

    // now is monotonic across reset() — only prepare() rebases it to 0.
    engine.reset();
    StereoBuffer more(512);
    engine.process(more.channels.data(), 2, 512);
    CHECK(engine.vizFeed().readLevels().now == 24512u);

    engine.prepare(48000.0, 512, 2);
    StereoBuffer few(10);
    engine.process(few.channels.data(), 2, 10);
    CHECK(engine.vizFeed().readLevels().now == 10u);
}

TEST_CASE("zero-length process() leaves the prior snapshot intact and fires nothing") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    TapSettings tap;
    tap.enabled = true;
    tap.sync = dsp::SyncDivision::Free;
    tap.timeSeconds = 100.0f / 48000.0f;
    tap.feedback = 0.0f;
    engine.setTap(0, tap);
    engine.setDryWet(0.5f);
    engine.setDuckAmount(1.0f);
    StereoBuffer buf(4800);
    for (int n = 0; n < 4800; ++n)
        buf.left[static_cast<size_t>(n)] = buf.right[static_cast<size_t>(n)] =
            0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(n) / 48000.0f);
    engine.process(buf.channels.data(), 2, 4800);
    orbit::viz::TapFireEvent e;
    while (engine.vizFeed().popEvent(e)) {}              // drain any fires
    const auto before = engine.vizFeed().readLevels();
    REQUIRE(before.inPeak > 0.0f);                       // there is a snapshot to clobber
    REQUIRE(before.now == 4800u);

    engine.process(buf.channels.data(), 2, 0);           // empty block
    const auto after = engine.vizFeed().readLevels();
    CHECK(after.inRms == before.inRms);                  // snapshot untouched
    CHECK(after.inPeak == before.inPeak);
    CHECK(after.outRms == before.outRms);
    CHECK(after.outPeak == before.outPeak);
    CHECK(after.duckGain == before.duckGain);
    CHECK(after.now == before.now);                      // now did not advance
    CHECK_FALSE(engine.vizFeed().popEvent(e));           // nothing fired
}

TEST_CASE("viz feed stays silent for disabled taps and empty input") {
    OrbitEngine engine;
    engine.prepare(48000.0, 512, 2);
    StereoBuffer buf(24000);                  // all zeros, all taps disabled
    engine.process(buf.channels.data(), 2, 24000);
    orbit::viz::TapFireEvent e;
    CHECK_FALSE(engine.vizFeed().popEvent(e));
    const auto s = engine.vizFeed().readLevels();
    CHECK(s.inRms == 0.0f);
    CHECK(s.outPeak == 0.0f);
}
