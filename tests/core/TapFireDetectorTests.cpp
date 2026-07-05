#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <vector>
#include "viz/TapFireDetector.h"

using orbit::viz::TapFireDetector;

namespace {
// Runs `input` through a detector prepared at 48 kHz with the given tap
// delay; returns (fireTime, intensity) pairs.
std::vector<std::pair<std::uint64_t, float>>
run(const std::vector<float>& input, float tapDelaySeconds) {
    TapFireDetector det;
    det.prepare(48000.0);
    det.setTapDelaySeconds(tapDelaySeconds);
    std::vector<std::pair<std::uint64_t, float>> fires;
    for (std::size_t n = 0; n < input.size(); ++n) {
        std::uint64_t t = 0;
        float intensity = 0.0f;
        if (det.processSample(input[n], static_cast<std::uint64_t>(n), t, intensity))
            fires.emplace_back(t, intensity);
    }
    return fires;
}
} // namespace

TEST_CASE("silence produces zero fires") {
    CHECK(run(std::vector<float>(96000, 0.0f), 0.25f).empty());
}

TEST_CASE("one echo burst = exactly one fire, timestamped at the crossing") {
    std::vector<float> in(48000, 0.0f);
    for (int n = 10000; n < 10240; ++n) in[static_cast<size_t>(n)] = 0.8f; // 5 ms burst
    const auto fires = run(in, 0.25f);
    REQUIRE(fires.size() == 1);
    CHECK(fires[0].first >= 10000);
    CHECK(fires[0].first < 10100);       // 1 ms attack: crossing within ~2 ms
    CHECK(fires[0].second > 0.7f);       // intensity ~ burst peak
}

TEST_CASE("hold prevents double-fires inside one repeat") {
    std::vector<float> in(48000, 0.0f);
    for (int n = 10000; n < 10240; ++n) in[static_cast<size_t>(n)] = 0.8f;
    for (int n = 10400; n < 10640; ++n) in[static_cast<size_t>(n)] = 0.7f;  // flutter 3ms later
    CHECK(run(in, 0.25f).size() == 1);   // hold (125 ms) swallows the flutter
}

TEST_CASE("successive repeats each fire, louder repeat = higher intensity") {
    std::vector<float> in(96000, 0.0f);
    const int kGap = 24000;              // 500 ms apart, hold = 125 ms
    const float levels[] = { 0.9f, 0.45f, 0.2f };
    for (int r = 0; r < 3; ++r)
        for (int n = 0; n < 240; ++n)
            in[static_cast<size_t>(10000 + r * kGap + n)] = levels[r];
    const auto fires = run(in, 0.25f);
    REQUIRE(fires.size() == 3);
    CHECK(fires[0].second > fires[1].second);
    CHECK(fires[1].second > fires[2].second);
}

TEST_CASE("sub-threshold signal never fires") {
    CHECK(run(std::vector<float>(48000, 0.005f), 0.25f).empty());  // -46 dB < -40 dB
}
