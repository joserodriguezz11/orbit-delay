#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/DelayLine.h"

using Catch::Approx;
using orbit::dsp::DelayLine;

static std::vector<float> impulseResponse(DelayLine& line, int numSamples) {
    std::vector<float> out;
    out.reserve(static_cast<size_t>(numSamples));
    for (int n = 0; n < numSamples; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    return out;
}

TEST_CASE("DelayLine delays an impulse by the configured whole-sample time") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);
    const auto out = impulseResponse(line, 150);
    CHECK(out[100] == Approx(1.0f));
    for (int n = 0; n < 100; ++n)
        REQUIRE(out[static_cast<size_t>(n)] == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("DelayLine interpolates fractional delay times") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.5f / 48000.0f);
    const auto out = impulseResponse(line, 20);
    CHECK(out[10] == Approx(0.5f).margin(1e-4f));
    CHECK(out[11] == Approx(0.5f).margin(1e-4f));
}

TEST_CASE("DelayLine feedback produces geometrically decaying repeats") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(0.5f);
    const auto out = impulseResponse(line, 45);
    CHECK(out[10] == Approx(1.0f));
    CHECK(out[20] == Approx(0.5f));
    CHECK(out[30] == Approx(0.25f));
    CHECK(out[40] == Approx(0.125f));
}

TEST_CASE("DelayLine clamps feedback and never blows up") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(2.0f);   // must clamp to 0.98
    float maxAbs = 0.0f;
    for (int n = 0; n < 48000; ++n) {
        const float y = line.processSample(n == 0 ? 1.0f : 0.0f);
        REQUIRE(std::isfinite(y));
        maxAbs = std::max(maxAbs, std::abs(y));
    }
    CHECK(maxAbs <= 1.5f);
}

TEST_CASE("DelayLine flushes denormal-range feedback tails to hard zero") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(0.5f);
    std::vector<float> out;
    for (int n = 0; n < 20000; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    for (int n = 19000; n < 20000; ++n)
        REQUIRE(out[static_cast<size_t>(n)] == 0.0f);   // exactly zero, not denormal dust
}
