#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace orbit::viz {

struct TapFireEvent {
    std::uint32_t tapIndex = 0;
    std::uint64_t timeSamples = 0;   // absolute engine sample count of the threshold crossing
    float intensity01 = 0.0f;        // clamped [0, 1]
};

struct LevelSnapshot {
    float inRms = 0.0f, inPeak = 0.0f;
    float outRms = 0.0f, outPeak = 0.0f;
    float duckGain = 1.0f;           // linear; 1 = no ducking
    std::uint64_t now = 0;           // engine sample counter at snapshot time
                                     // (UI computes event age as now - timeSamples)
};

// Single-producer (audio thread) / single-consumer (UI thread) feed.
// prepare() allocates on first call only (call before the consumer exists);
// re-prepare and reset() are reader-safe — see the .cpp for the contract.
// Caveat: reset() executes the seqlock WRITE sequence (via writeLevels), so it
// is safe against concurrent readers but must never run concurrently with
// another writer (process()/writeLevels) — call it from the audio thread or
// with audio stopped. Two concurrent writers can interleave v+1/v+2 and let a
// reader accept a torn snapshot.
class VizFeed {
public:
    static constexpr std::size_t kEventCapacity = 256;

    void prepare();                          // first call allocates ring + zeroes indices; re-prepare == reset()
    void reset();                            // snapshot-only: re-publishes a zero snapshot; queued events survive

    bool pushEvent(const TapFireEvent& e);   // producer; false = ring full, event dropped
    bool popEvent(TapFireEvent& out);        // consumer; false = empty

    void writeLevels(const LevelSnapshot& s); // producer, once per block
    LevelSnapshot readLevels() const;         // consumer; retries on torn read

private:
    std::vector<TapFireEvent> ring_;
    std::atomic<std::uint64_t> head_ { 0 };  // next write slot (producer-owned)
    std::atomic<std::uint64_t> tail_ { 0 };  // next read slot (consumer-owned)

    // Seqlock: version_ odd = write in progress; fields relaxed-atomic so a
    // racing read is unordered, not UB — version_ acquire/release publishes.
    std::atomic<std::uint32_t> version_ { 0 };
    std::atomic<float> inRms_ { 0.0f }, inPeak_ { 0.0f };
    std::atomic<float> outRms_ { 0.0f }, outPeak_ { 0.0f };
    std::atomic<float> duckGain_ { 1.0f };
    std::atomic<std::uint64_t> now_ { 0 };   // published/read inside the seqlock,
                                             // so the version check prevents a
                                             // torn 64-bit read on 32-bit targets
};

} // namespace orbit::viz
