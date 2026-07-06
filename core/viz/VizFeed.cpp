#include "viz/VizFeed.h"

namespace orbit::viz {

void VizFeed::prepare() {
    // Allocate the ring exactly once: ring_.assign() on a re-prepare would
    // reallocate storage out from under a reader indexing ring_ (a data
    // race). Indices and version are zeroed only alongside that first
    // allocation, when no reader can exist yet. A re-prepare does no
    // reallocation and no index reset — it just re-publishes the zero
    // snapshot via the reader-safe reset(), so queued events survive.
    if (ring_.size() != kEventCapacity) {
        ring_.assign(kEventCapacity, TapFireEvent {});
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        version_.store(0, std::memory_order_relaxed);
    }
    reset();
}

void VizFeed::reset() {
    // Snapshot-only, reader-safe: re-publish an all-zero snapshot (duckGain 1)
    // through the exact writeLevels() seqlock sequence, so a concurrently
    // polling reader can never see a torn snapshot. Deliberately does NOT:
    // - touch head_/tail_ — they are producer/consumer-owned; zeroing them
    //   under a live consumer underflows popEvent's unsigned indices.
    //   Leftover events stay poppable, consistent with the consumer contract
    //   ("timestamps monotonic across reset()").
    // - rewind version_ to 0 — that breaks the monotonicity the readers'
    //   retry loop relies on.
    // The published now = 0 is a one-block transient: the next process()
    // block republishes the engine's true sample counter.
    writeLevels(LevelSnapshot {});
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
    now_.store(s.now, std::memory_order_relaxed);
    version_.store(v + 2, std::memory_order_release);  // even: published
}

LevelSnapshot VizFeed::readLevels() const {
    // Busy-spin retry: acceptable at UI poll rate (~30 Hz reads racing one
    // sub-microsecond write per audio block — a retry is rare and cheap).
    // Would need backoff (yield/pause) only if this ever moved to a hot path.
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
        s.now = now_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (version_.load(std::memory_order_relaxed) == v1)
            return s;                                  // consistent
    }
}

} // namespace orbit::viz
