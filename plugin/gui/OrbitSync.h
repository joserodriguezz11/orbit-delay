#pragma once

#include <cmath>
#include "dsp/TempoSync.h"

namespace orbit::gui {

// Nearest non-Free sync division to a time in ms at the given tempo — the
// single snapping rule shared by pad drags (OrbitEditor::writeOrb) and the
// strip's SYNC pill (OrbitTapStrip::setSyncEnabled).
inline int nearestSyncDivision(float ms, double bpm) {
    using orbit::dsp::SyncDivision;
    int best = int(SyncDivision::Quarter);
    float bestD = 1.0e9f;
    for (int d = 1; d < int(SyncDivision::NumDivisions); ++d) {
        const float dms = orbit::dsp::divisionToSeconds(SyncDivision(d), bpm) * 1000.0f;
        const float diff = std::abs(dms - ms);
        if (diff < bestD) {
            bestD = diff;
            best = d;
        }
    }
    return best;
}

} // namespace orbit::gui
