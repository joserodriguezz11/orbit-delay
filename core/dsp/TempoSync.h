#pragma once

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

// Delay time in seconds for a division at the given tempo.
// Free (or a non-positive bpm) returns 0 — caller falls back to free time.
float divisionToSeconds(SyncDivision division, double bpm);

} // namespace orbit::dsp
