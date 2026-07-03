#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/Lfo.h"

using Catch::Approx;
using orbit::dsp::Lfo;

TEST_CASE("Lfo produces a bounded sine with the requested period") {
    Lfo lfo;
    lfo.prepare(48000.0);
    lfo.setRate(1.0f);                       // 1 Hz -> period 48000 samples
    float atQuarter = 0.0f, atHalf = 0.0f, maxAbs = 0.0f;
    for (int n = 0; n < 48000; ++n) {
        const float v = lfo.processSample();
        if (n == 11999) atQuarter = v;       // ~quarter period -> +1 peak
        if (n == 23999) atHalf = v;          // ~half period -> zero crossing
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    CHECK(atQuarter == Approx(1.0f).margin(1e-2f));
    CHECK(std::abs(atHalf) < 1e-2f);
    CHECK(maxAbs <= 1.0f + 1e-4f);
}

TEST_CASE("Lfo reset restarts the phase") {
    Lfo lfo;
    lfo.prepare(48000.0);
    lfo.setRate(2.0f);
    const float first = lfo.processSample();
    for (int n = 0; n < 1000; ++n)
        lfo.processSample();
    lfo.reset();
    CHECK(lfo.processSample() == Approx(first));
}
