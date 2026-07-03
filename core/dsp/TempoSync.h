#pragma once
#include <cstddef>

namespace orbit::dsp {

// Order is a contract with the tapN_sync AudioParameterChoice (Task 7).
// Append new divisions at the end; NEVER reorder.
enum class SyncDivision {
    Free = 0,
    Whole,
    Half,
    Quarter,
    Eighth,
    Sixteenth,
    QuarterDotted,
    EighthDotted,
    QuarterTriplet,
    EighthTriplet,
    NumDivisions
};

// Display names for SyncDivision, index-aligned with the enum. The
// static_assert makes the enum<->choice-list contract compile-time.
inline constexpr const char* kSyncDivisionNames[] = {
    "Free", "1/1", "1/2", "1/4", "1/8", "1/16",
    "1/4 D", "1/8 D", "1/4 T", "1/8 T"
};
static_assert(sizeof(kSyncDivisionNames) / sizeof(kSyncDivisionNames[0])
                  == static_cast<std::size_t>(SyncDivision::NumDivisions),
              "kSyncDivisionNames must track SyncDivision exactly");

// Delay time in seconds for a division at the given tempo.
// Free (or a non-positive bpm) returns 0 — caller falls back to free time.
float divisionToSeconds(SyncDivision division, double bpm);

} // namespace orbit::dsp
