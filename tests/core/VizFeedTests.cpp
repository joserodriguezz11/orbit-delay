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
    CHECK(def.now == 0u);
    feed.writeLevels({ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 1234u });
    const auto s = feed.readLevels();
    CHECK(s.inRms == 0.1f);
    CHECK(s.inPeak == 0.2f);
    CHECK(s.outRms == 0.3f);
    CHECK(s.outPeak == 0.4f);
    CHECK(s.duckGain == 0.5f);
    CHECK(s.now == 1234u);
}

TEST_CASE("reset() zeroes the level snapshot but leaves queued events poppable") {
    VizFeed feed;
    feed.prepare();
    REQUIRE(feed.pushEvent({ 1, 100, 0.5f }));
    REQUIRE(feed.pushEvent({ 2, 200, 0.75f }));
    feed.writeLevels({ 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 777u });
    feed.reset();
    const auto s = feed.readLevels();
    CHECK(s.inRms == 0.0f);
    CHECK(s.inPeak == 0.0f);
    CHECK(s.outRms == 0.0f);
    CHECK(s.outPeak == 0.0f);
    CHECK(s.duckGain == 1.0f);
    CHECK(s.now == 0u);
    TapFireEvent e;
    REQUIRE(feed.popEvent(e));           // reset is snapshot-only: events survive
    CHECK(e.tapIndex == 1u);
    CHECK(e.timeSamples == 100u);
    REQUIRE(feed.popEvent(e));
    CHECK(e.tapIndex == 2u);
    CHECK(e.timeSamples == 200u);
    REQUIRE_FALSE(feed.popEvent(e));
}

TEST_CASE("re-prepare() keeps queued events: no reallocation, no index reset") {
    VizFeed feed;
    feed.prepare();
    REQUIRE(feed.pushEvent({ 3, 500, 0.9f }));
    feed.writeLevels({ 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 99u });
    feed.prepare();                      // second prepare: snapshot-only reset
    const auto s = feed.readLevels();
    CHECK(s.inRms == 0.0f);
    CHECK(s.duckGain == 1.0f);
    CHECK(s.now == 0u);
    TapFireEvent e;
    REQUIRE(feed.popEvent(e));
    CHECK(e.tapIndex == 3u);
    CHECK(e.timeSamples == 500u);
    REQUIRE_FALSE(feed.popEvent(e));
}

TEST_CASE("reset() is safe against a concurrently polling reader") {
    // One thread plays the audio side (writeLevels / pushEvent / reset), the
    // other polls readLevels() + popEvent() like the UI. A snapshot must be
    // either a writer state (all fields x, now == kNow) or the reset state
    // ({0,0,0,0,1}, now == 0) — any mix is a torn read. popEvent must never
    // produce a garbage event (the old reset() zeroed head_/tail_ under the
    // consumer, underflowing the unsigned indices).
    VizFeed feed;
    feed.prepare();
    constexpr std::uint64_t kNow = 42;
    feed.writeLevels({ 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, kNow });  // seed: reader may run first
    std::atomic<bool> stop { false };
    std::thread audio([&] {
        float x = 0.5f;
        std::uint32_t i = 0;
        while (!stop.load()) {
            feed.writeLevels({ x, x, x, x, x, kNow });
            feed.pushEvent({ i & 3u, kNow, 0.5f });
            feed.reset();
            feed.writeLevels({ x, x, x, x, x, kNow });
            x += 0.25f;
            ++i;
        }
    });
    bool torn = false;
    bool badEvent = false;
    for (int i = 0; i < 200000 && !torn && !badEvent; ++i) {
        const auto s = feed.readLevels();
        const bool writerState = s.inRms == s.inPeak && s.inRms == s.outRms
            && s.inRms == s.outPeak && s.inRms == s.duckGain && s.now == kNow;
        const bool resetState = s.inRms == 0.0f && s.inPeak == 0.0f
            && s.outRms == 0.0f && s.outPeak == 0.0f
            && s.duckGain == 1.0f && s.now == 0u;
        torn = !(writerState || resetState);
        TapFireEvent e;
        if (feed.popEvent(e))
            badEvent = e.tapIndex > 3u || e.timeSamples != kNow;
    }
    stop.store(true);
    audio.join();
    REQUIRE_FALSE(torn);      // assert only after join: a REQUIRE throw must
    REQUIRE_FALSE(badEvent);  // not destroy a joinable thread (SIGABRT)
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
