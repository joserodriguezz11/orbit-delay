#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/TempoSync.h"

using Catch::Approx;
using orbit::dsp::SyncDivision;
using orbit::dsp::divisionToSeconds;

TEST_CASE("straight divisions at 120 BPM") {
    CHECK(divisionToSeconds(SyncDivision::Whole, 120.0) == Approx(2.0f));
    CHECK(divisionToSeconds(SyncDivision::Half, 120.0) == Approx(1.0f));
    CHECK(divisionToSeconds(SyncDivision::Quarter, 120.0) == Approx(0.5f));
    CHECK(divisionToSeconds(SyncDivision::Eighth, 120.0) == Approx(0.25f));
    CHECK(divisionToSeconds(SyncDivision::Sixteenth, 120.0) == Approx(0.125f));
}

TEST_CASE("dotted divisions are 1.5x straight") {
    CHECK(divisionToSeconds(SyncDivision::QuarterDotted, 120.0) == Approx(0.75f));
    CHECK(divisionToSeconds(SyncDivision::EighthDotted, 120.0) == Approx(0.375f));
}

TEST_CASE("triplet divisions are 2/3 straight") {
    CHECK(divisionToSeconds(SyncDivision::QuarterTriplet, 120.0) == Approx(0.5f * 2.0f / 3.0f));
    CHECK(divisionToSeconds(SyncDivision::EighthTriplet, 120.0) == Approx(0.25f * 2.0f / 3.0f));
}

TEST_CASE("Free and invalid tempi return zero") {
    CHECK(divisionToSeconds(SyncDivision::Free, 120.0) == 0.0f);
    CHECK(divisionToSeconds(SyncDivision::Quarter, 0.0) == 0.0f);
    CHECK(divisionToSeconds(SyncDivision::Quarter, -60.0) == 0.0f);
}
