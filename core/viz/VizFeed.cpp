#include "viz/VizFeed.h"

namespace orbit::viz {

void VizFeed::prepare() {
    ring_.assign(kEventCapacity, TapFireEvent {});
    reset();
}

void VizFeed::reset() {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    version_.store(0, std::memory_order_relaxed);
    inRms_.store(0.0f, std::memory_order_relaxed);
    inPeak_.store(0.0f, std::memory_order_relaxed);
    outRms_.store(0.0f, std::memory_order_relaxed);
    outPeak_.store(0.0f, std::memory_order_relaxed);
    duckGain_.store(1.0f, std::memory_order_relaxed);
}

bool VizFeed::pushEvent(const TapFireEvent& e) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (head - tail >= kEventCapacity)
        return false;                                  // full: drop, never block
    ring_[static_cast<std::size_t>(head % kEventCapacity)] = e;
    head_.store(head + 1, std::memory_order_release);
    return true;
}

bool VizFeed::popEvent(TapFireEvent& out) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    const auto head = head_.load(std::memory_order_acquire);
    if (tail == head)
        return false;                                  // empty
    out = ring_[static_cast<std::size_t>(tail % kEventCapacity)];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

void VizFeed::writeLevels(const LevelSnapshot& s) {
    const auto v = version_.load(std::memory_order_relaxed);
    // Odd = write in progress. Relaxed store + release FENCE (not a release
    // store): a release store only orders PRIOR accesses before it — the
    // field stores below could otherwise become visible before the odd
    // version on weakly-ordered hardware (ARM), letting a reader return a
    // torn snapshot. (Amended after Task 1 review; canonical seqlock entry.)
    version_.store(v + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    inRms_.store(s.inRms, std::memory_order_relaxed);
    inPeak_.store(s.inPeak, std::memory_order_relaxed);
    outRms_.store(s.outRms, std::memory_order_relaxed);
    outPeak_.store(s.outPeak, std::memory_order_relaxed);
    duckGain_.store(s.duckGain, std::memory_order_relaxed);
    version_.store(v + 2, std::memory_order_release);  // even: published
}

LevelSnapshot VizFeed::readLevels() const {
    LevelSnapshot s;
    for (;;) {
        const auto v1 = version_.load(std::memory_order_acquire);
        if (v1 & 1u)
            continue;                                  // writer mid-flight
        s.inRms = inRms_.load(std::memory_order_relaxed);
        s.inPeak = inPeak_.load(std::memory_order_relaxed);
        s.outRms = outRms_.load(std::memory_order_relaxed);
        s.outPeak = outPeak_.load(std::memory_order_relaxed);
        s.duckGain = duckGain_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (version_.load(std::memory_order_relaxed) == v1)
            return s;                                  // consistent
    }
}

} // namespace orbit::viz
