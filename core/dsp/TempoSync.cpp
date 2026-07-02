#include "dsp/TempoSync.h"

namespace orbit::dsp {

float divisionToSeconds(SyncDivision division, double bpm) {
    if (bpm <= 0.0 || division == SyncDivision::Free)
        return 0.0f;

    const float beat = static_cast<float>(60.0 / bpm);   // one quarter note
    switch (division) {
        case SyncDivision::Whole:          return 4.0f * beat;
        case SyncDivision::Half:           return 2.0f * beat;
        case SyncDivision::Quarter:        return beat;
        case SyncDivision::Eighth:         return 0.5f * beat;
        case SyncDivision::Sixteenth:      return 0.25f * beat;
        case SyncDivision::QuarterDotted:  return 1.5f * beat;
        case SyncDivision::EighthDotted:   return 0.75f * beat;
        case SyncDivision::QuarterTriplet: return beat * 2.0f / 3.0f;
        case SyncDivision::EighthTriplet:  return 0.5f * beat * 2.0f / 3.0f;
        default:                           return 0.0f;
    }
}

} // namespace orbit::dsp
