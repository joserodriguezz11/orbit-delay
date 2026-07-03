#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include "dsp/Ducker.h"

using Catch::Approx;
using orbit::dsp::Ducker;

TEST_CASE("Ducker leaves gain at 1.0 when amount is zero") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(0.0f);
    float gain = 1.0f;
    for (int n = 0; n < 4800; ++n)
        gain = d.processGain(1.0f);
    CHECK(gain == Approx(1.0f));
}

TEST_CASE("Ducker strongly reduces gain on sustained loud input at full amount") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(1.0f);
    float gain = 1.0f;
    for (int n = 0; n < 4800; ++n)   // 100 ms of loud dry signal
        gain = d.processGain(1.0f);
    CHECK(gain < 0.3f);
}

TEST_CASE("Ducker recovers toward 1.0 after the input goes silent") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(1.0f);
    for (int n = 0; n < 4800; ++n)
        d.processGain(1.0f);
    float gain = 0.0f;
    for (int n = 0; n < 48000; ++n)  // 1 s of silence
        gain = d.processGain(0.0f);
    CHECK(gain > 0.9f);
}

TEST_CASE("Ducker recovers from a NaN sidechain sample") {
    Ducker d;
    d.prepare(48000.0);
    d.setAmount(1.0f);
    d.processGain(std::numeric_limits<float>::quiet_NaN());
    float gain = 0.0f;
    for (int n = 0; n < 48000; ++n)
        gain = d.processGain(0.0f);
    CHECK(gain > 0.9f);
}
