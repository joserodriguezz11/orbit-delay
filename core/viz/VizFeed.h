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
};

// Single-producer (audio thread) / single-consumer (UI thread) feed.
// prepare()/reset() are NOT thread-safe: call with audio stopped.
class VizFeed {
public:
    static constexpr std::size_t kEventCapacity = 256;

    void prepare();                          // allocates ring storage, then reset()
    void reset();                            // drops queued events, zeroes snapshot

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
};

} // namespace orbit::viz
