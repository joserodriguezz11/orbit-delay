#pragma once

namespace orbit::dsp {

// Shared pi constant for all DSP code. The literal is byte-identical to the
// values previously inlined in Lfo.cpp / OnePole.cpp (and to M_PI on this
// platform), so adopting it changes no DSP output bit.
inline constexpr double kPi = 3.14159265358979323846;

} // namespace orbit::dsp
