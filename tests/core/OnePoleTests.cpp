#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/OnePole.h"

using Catch::Approx;
using orbit::dsp::OnePole;

TEST_CASE("OnePole low-pass passes DC") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::LowPass);
    f.setCutoff(1000.0f);
    float y = 0.0f;
    for (int n = 0; n < 48000; ++n)
        y = f.processSample(1.0f);
    CHECK(y == Approx(1.0f).margin(1e-3f));
}

TEST_CASE("OnePole high-pass rejects DC") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::HighPass);
    f.setCutoff(100.0f);
    float y = 1.0f;
    for (int n = 0; n < 48000; ++n)
        y = f.processSample(1.0f);
    CHECK(std::abs(y) < 0.01f);
}

TEST_CASE("OnePole low-pass attenuates Nyquist-rate alternation") {
    OnePole f;
    f.prepare(48000.0, OnePole::Mode::LowPass);
    f.setCutoff(1000.0f);
    float sumAbs = 0.0f;
    for (int n = 0; n < 4800; ++n) {
        const float x = (n % 2 == 0) ? 1.0f : -1.0f;
        const float y = f.processSample(x);
        if (n >= 4700) sumAbs += std::abs(y);
    }
    CHECK(sumAbs / 100.0f < 0.2f);
}
