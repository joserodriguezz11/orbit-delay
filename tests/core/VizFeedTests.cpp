#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include "viz/VizFeed.h"

using Catch::Approx;
using orbit::viz::LevelSnapshot;
using orbit::viz::TapFireEvent;
using orbit::viz::VizFeed;

TEST_CASE("event ring: FIFO order, wraparound, drop-when-full") {
    VizFeed feed;
    feed.prepare();
    TapFireEvent e;
    REQUIRE_FALSE(feed.popEvent(e));                    // starts empty
    for (std::uint32_t round = 0; round < 3; ++round) { // 3 rounds forces wraparound
        for (std::uint32_t i = 0; i < VizFeed::kEventCapacity; ++i)
            REQUIRE(feed.pushEvent({ i, round, 0.5f }));
        REQUIRE_FALSE(feed.pushEvent({ 999, 999, 1.0f })); // full: dropped
        for (std::uint32_t i = 0; i < VizFeed::kEventCapacity; ++i) {
            REQUIRE(feed.popEvent(e));
            CHECK(e.tapIndex == i);                     // order kept, dropped event absent
            CHECK(e.timeSamples == round);
        }
        REQUIRE_FALSE(feed.popEvent(e));
    }
}

TEST_CASE("event ring: two-thread stress loses nothing below capacity") {
    VizFeed feed;
    feed.prepare();
    constexpr std::uint32_t kTotal = 100000;
    std::atomic<bool> done { false };
    std::vector<std::uint32_t> seen;
    seen.reserve(kTotal);
    std::thread consumer([&] {
        TapFireEvent e;
        while (!done.load() || feed.popEvent(e)) {
            if (e.intensity01 != 0.0f) { seen.push_back(e.tapIndex); e.intensity01 = 0.0f; }
            else if (feed.popEvent(e)) { seen.push_back(e.tapIndex); e.intensity01 = 0.0f; }
        }
    });
    for (std::uint32_t i = 0; i < kTotal; ++i)
        while (!feed.pushEvent({ i, 0, 1.0f }))         // spin on full: consumer drains
            std::this_thread::yield();
    done.store(true);
    consumer.join();
    REQUIRE(seen.size() == kTotal);
    for (std::uint32_t i = 0; i < kTotal; ++i)
        REQUIRE(seen[i] == i);                          // no loss, no dupes, no tears
}

TEST_CASE("level snapshot: write then read round-trips") {
    VizFeed feed;
    feed.prepare();
    LevelSnapshot def = feed.readLevels();
    CHECK(def.duckGain == 1.0f);
    CHECK(def.inRms == 0.0f);
    feed.writeLevels({ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f });
    const auto s = feed.readLevels();
    CHECK(s.inRms == 0.1f);
    CHECK(s.inPeak == 0.2f);
    CHECK(s.outRms == 0.3f);
    CHECK(s.outPeak == 0.4f);
    CHECK(s.duckGain == 0.5f);
}

TEST_CASE("level snapshot: reader never sees a torn write") {
    VizFeed feed;
    feed.prepare();
    // Seed before spawning the writer: the prepare() default {0,0,0,0,1}
    // does NOT satisfy the all-equal invariant, and the reader may run
    // before the writer's first write. (Amended during implementation —
    // the original test failed deterministically on that startup race.)
    feed.writeLevels({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });
    std::atomic<bool> stop { false };
    std::thread writer([&] {
        float x = 0.25f;
        while (!stop.load()) {
            // all five fields carry the same value: any mix = a torn read
            feed.writeLevels({ x, x, x, x, x });
            x += 0.25f;
        }
    });
    bool torn = false;
    for (int i = 0; i < 200000 && !torn; ++i) {
        const auto s = feed.readLevels();
        torn = !(s.inRms == s.inPeak && s.inRms == s.outRms
                 && s.inRms == s.outPeak && s.inRms == s.duckGain);
    }
    stop.store(true);
    writer.join();
    REQUIRE_FALSE(torn);   // assert only after join: a REQUIRE throw must
                           // not destroy a joinable thread (SIGABRT)
}
