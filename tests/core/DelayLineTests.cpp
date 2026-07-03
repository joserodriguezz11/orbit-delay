#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
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

TEST_CASE("DelayLine treats zero delay as a one-sample delay") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(0.0f);
    const auto out = impulseResponse(line, 4);
    CHECK(out[0] == Approx(0.0f).margin(1e-6f));
    CHECK(out[1] == Approx(1.0f));
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

TEST_CASE("DelayLine recovers from a NaN input sample") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(10.0f / 48000.0f);
    line.setFeedback(0.5f);
    line.processSample(std::numeric_limits<float>::quiet_NaN());
    bool allFinite = true;
    for (int n = 0; n < 100; ++n)
        allFinite = allFinite && std::isfinite(line.processSample(0.0f));
    CHECK(allFinite);
}

TEST_CASE("DelayLine keeps sub-sample precision at large delays and high rates") {
    DelayLine line;
    line.prepare(192000.0, 4.0f);
    line.setDelaySeconds(100000.5f / 192000.0f);
    std::vector<float> out;
    for (int n = 0; n < 100050; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    CHECK(out[100000] == Approx(0.5f).margin(1e-3f));
    CHECK(out[100001] == Approx(0.5f).margin(1e-3f));
}

TEST_CASE("DelayLine glides between delay times instead of jumping") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);     // snaps: not running yet
    for (int n = 0; n < 500; ++n)
        line.processSample(0.0f);                 // now running
    line.setDelaySeconds(200.0f / 48000.0f);     // must glide, not jump
    const auto out = impulseResponse(line, 260);
    float early = 0.0f, late = 0.0f;
    for (int n = 0; n < 130; ++n)
        early += std::abs(out[static_cast<size_t>(n)]);
    for (int n = 190; n < 210; ++n)
        late += std::abs(out[static_cast<size_t>(n)]);
    CHECK(early > 0.5f);    // echo still lands near the old time right after the change
    CHECK(late < 0.05f);    // and NOT at the new time yet — no instant jump
}

TEST_CASE("DelayLine modulation offset shifts the read position") {
    DelayLine line;
    line.prepare(48000.0, 1.0f);
    line.setDelaySeconds(100.0f / 48000.0f);
    line.setModulationSamples(10.0f);            // effective delay 110 samples
    std::vector<float> out;
    for (int n = 0; n < 150; ++n)
        out.push_back(line.processSample(n == 0 ? 1.0f : 0.0f));
    CHECK(out[110] == Approx(1.0f));
    CHECK(std::abs(out[100]) < 1e-6f);
}
