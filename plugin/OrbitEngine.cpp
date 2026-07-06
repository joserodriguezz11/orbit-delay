#include "OrbitEngine.h"
#include <algorithm>
#include <cmath>

namespace orbit {

void OrbitEngine::prepare(double sampleRate, int maxBlockSize, int numChannels) {
    (void) maxBlockSize;
    sampleRate_ = sampleRate;
    numChannels_ = std::clamp(numChannels, 1, kMaxChannels);
    for (auto& tapLines : lines_)
        for (auto& line : tapLines)
            line.prepare(sampleRate, kMaxDelaySeconds);
    ducker_.prepare(sampleRate);
    lfo_.prepare(sampleRate);
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        characterStages_[static_cast<size_t>(ch)].prepare(sampleRate);
        characterStages_[static_cast<size_t>(ch)].reset();
        lowCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::HighPass);
        highCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::LowPass);
    }
    vizFeed_.prepare();
    timeSamples_ = 0;
    inMs_ = outMs_ = 0.0f;
    // Same one-pole form the detector uses, at a 50 ms RMS window.
    msCoef_ = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sampleRate)));
    // Detectors before tap-time re-application: prepare() floors each hold,
    // and applyTapTimes() below restores the per-tap hold via
    // setTapDelaySeconds() (ordering contract from Task 2's review).
    for (auto& detector : fireDetectors_)
        detector.prepare(sampleRate);
    applyTapTimes();
    setDryWet(mix_);   // keep gains consistent with mix_ for default-constructed engines
}

void OrbitEngine::reset() {
    for (auto& tapLines : lines_)
        for (auto& line : tapLines)
            line.reset();
    ducker_.reset();
    lfo_.reset();
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        characterStages_[static_cast<size_t>(ch)].reset();
        lowCutFilters_[static_cast<size_t>(ch)].reset();
        highCutFilters_[static_cast<size_t>(ch)].reset();
    }
    vizFeed_.reset();
    for (auto& detector : fireDetectors_)
        detector.reset();
    inMs_ = outMs_ = 0.0f;
}

void OrbitEngine::setTap(int index, const TapSettings& settings) {
    if (index < 0 || index >= kNumTaps)
        return;
    taps_[static_cast<size_t>(index)] = settings;
    for (auto& line : lines_[static_cast<size_t>(index)]) {
        line.setFeedback(settings.feedback);
        line.setReadMode(settings.reverse ? dsp::DelayLine::ReadMode::Reverse
                                          : dsp::DelayLine::ReadMode::Normal);
        line.setPitchSemitones(settings.pitchSemitones);
    }
    // Accepted quirk (animation-only feed): resetting the detector on disable
    // means a re-enable mid-hold re-arms immediately, so a repeat that the
    // hold would normally swallow can fire a second event. Harmless for viz;
    // preferable to stale hold state suppressing the first real fire.
    if (!settings.enabled)
        fireDetectors_[static_cast<size_t>(index)].reset();
    applyTapTime(index);
}

void OrbitEngine::setDryWet(float mix01) {
    mix_ = std::clamp(mix01, 0.0f, 1.0f);
    dryGain_ = std::sqrt(1.0f - mix_);
    wetGain_ = std::sqrt(mix_);
}

void OrbitEngine::setDuckAmount(float amount01) { ducker_.setAmount(amount01); }

void OrbitEngine::setPingPong(bool enabled) { pingPong_ = enabled; }

void OrbitEngine::setWidth(float width01to2) {
    width_ = std::clamp(width01to2, 0.0f, 2.0f);
}

void OrbitEngine::setFreeze(bool enabled) { frozen_ = enabled; }

void OrbitEngine::setCharacterMode(dsp::CharacterStage::Mode mode) {
    for (auto& stage : characterStages_)
        stage.setMode(mode);
}

void OrbitEngine::setModulation(float depth01, float rateHz) {
    const float depth = std::clamp(depth01, 0.0f, 1.0f);
    modDepthSamples_ = depth * kMaxModSeconds * static_cast<float>(sampleRate_);
    lfo_.setRate(rateHz);
}

void OrbitEngine::setFilters(float lowCutHz, float highCutHz) {
    // Reset filters when they transition to off so a later re-enable starts
    // from clean state instead of stale energy from the last active pass.
    const bool lowCutTurnsOff  = lowCutHz_ > 0.0f && lowCutHz <= 0.0f;
    const bool highCutTurnsOff = highCutHz_ < 20000.0f && highCutHz >= 20000.0f;
    lowCutHz_ = lowCutHz;
    highCutHz_ = highCutHz;
    for (int ch = 0; ch < kMaxChannels; ++ch) {
        lowCutFilters_[static_cast<size_t>(ch)].setCutoff(lowCutHz);
        highCutFilters_[static_cast<size_t>(ch)].setCutoff(highCutHz);
        if (lowCutTurnsOff)
            lowCutFilters_[static_cast<size_t>(ch)].reset();
        if (highCutTurnsOff)
            highCutFilters_[static_cast<size_t>(ch)].reset();
    }
}

void OrbitEngine::setBpm(double bpm) {
    if (bpm > 0.0 && bpm != bpm_) {
        bpm_ = bpm;
        applyTapTimes();
    }
}

void OrbitEngine::applyTapTime(int index) {
    const auto& tap = taps_[static_cast<size_t>(index)];
    const float seconds = tap.sync == dsp::SyncDivision::Free
        ? tap.timeSeconds
        : dsp::divisionToSeconds(tap.sync, bpm_);
    for (auto& line : lines_[static_cast<size_t>(index)])
        line.setDelaySeconds(seconds);
    // Forward here (not just setTap) so prepare()'s tap-time re-application
    // restores detector holds after the detectors are re-prepared.
    fireDetectors_[static_cast<size_t>(index)].setTapDelaySeconds(seconds);

    // Motion headroom: full depth only when the tap delay leaves room for the
    // max modulation excursion; shorter taps scale depth down proportionally.
    const float delaySamples = seconds * static_cast<float>(sampleRate_);
    const float maxMod = kMaxModSeconds * static_cast<float>(sampleRate_);
    modScale_[static_cast<size_t>(index)] =
        maxMod > 0.0f ? std::min(1.0f, std::max(0.0f, delaySamples - 2.0f) / maxMod) : 0.0f;
}

void OrbitEngine::applyTapTimes() {
    for (int i = 0; i < kNumTaps; ++i)
        applyTapTime(i);
}

void OrbitEngine::process(float* const* channelData, int numChannels, int numSamples) {
    // Empty block: return before the block-end writeLevels so it cannot
    // overwrite the last good snapshot with zeroed peaks / a stale duck gain,
    // or publish a spurious "now". (Guard, not a DSP change.)
    if (numSamples <= 0)
        return;
    const int channels = std::clamp(numChannels, 1, numChannels_);
    float inBlockPeak = 0.0f, outBlockPeak = 0.0f;   // peaks reset every block
    float lastDuckGain = 1.0f;
    for (int n = 0; n < numSamples; ++n) {
        float dryLevel = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            dryLevel += std::abs(channelData[ch][n]);
        dryLevel /= static_cast<float>(channels);
        const float duckGain = ducker_.processGain(dryLevel);

        const float modOffset = lfo_.processSample() * modDepthSamples_;

        const bool cross = pingPong_ && channels >= 2;   // ping-pong is stereo only
        float dry[kMaxChannels] {};
        float wet[kMaxChannels] {};
        for (int ch = 0; ch < channels; ++ch)
            dry[ch] = channelData[ch][n];

        // Viz input stats (observe only): mean of squares -> 50 ms one-pole
        // RMS accumulator; max |dry| -> block peak.
        lastDuckGain = duckGain;
        float inSq = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            inSq += dry[ch] * dry[ch];
            inBlockPeak = std::max(inBlockPeak, std::abs(dry[ch]));
        }
        inSq /= static_cast<float>(channels);
        inMs_ += (inSq - inMs_) * msCoef_;

        for (int t = 0; t < kNumTaps; ++t) {
            auto& tapLines = lines_[static_cast<size_t>(t)];
            // Read every channel of this tap BEFORE any write so feedback can
            // cross channels (ping-pong) without consuming this sample's write.
            float outs[kMaxChannels] {};
            for (int ch = 0; ch < channels; ++ch) {
                auto& line = tapLines[static_cast<size_t>(ch)];
                line.setModulationSamples(modOffset * modScale_[static_cast<size_t>(t)]);
                outs[ch] = line.read();
            }
            for (int ch = 0; ch < channels; ++ch) {
                auto& line = tapLines[static_cast<size_t>(ch)];
                if (frozen_) {
                    // Freeze: recirculate the captured audio at unity — no new
                    // input enters the line, and ping-pong's cross-write is
                    // bypassed so each line simply loops its own read forever.
                    //
                    // Design notes (accepted behavior):
                    // (1) While frozen, glide and Motion still move the read
                    //     positions, so tap-time/BPM/Motion changes re-pitch or
                    //     smear the frozen loop. This is bounded — all read
                    //     paths are convex combinations of buffer contents.
                    // (2) With ping-pong on, disabled warm-keeping taps also
                    //     cross their feedback (intentional; default path
                    //     unaffected).
                    line.writeAndAdvance(outs[ch]);
                } else {
                    // Ping-pong: each channel feeds back the OTHER channel's output
                    // so the echo bounces L -> R -> L. Off (or mono): own output —
                    // identical topology to the pre-split processSample path.
                    const float fbSource = cross ? outs[1 - ch] : outs[ch];
                    line.writeAndAdvance(dry[ch] + fbSource * line.feedback());
                }
            }
            // Disabled taps keep processing (buffers stay warm -> no clicks
            // on re-enable) but don't contribute to the mix, and feed the
            // fire detector 0 (below) so warm-keeping audio can't trigger
            // viz events. Fused loop: wet accumulation order and the
            // tapLevel max-reduction are unchanged (bit-identical).
            float tapLevel = 0.0f;
            if (taps_[static_cast<size_t>(t)].enabled) {
                for (int ch = 0; ch < channels; ++ch) {
                    wet[ch] += outs[ch];
                    tapLevel = std::max(tapLevel, std::abs(outs[ch]));
                }
            }

            // Viz tap-fire detection (observe only).
            std::uint64_t fireTime = 0;
            float intensity = 0.0f;
            if (fireDetectors_[static_cast<size_t>(t)].processSample(
                    tapLevel, timeSamples_, fireTime, intensity))
                vizFeed_.pushEvent({ static_cast<std::uint32_t>(t), fireTime, intensity });
        }

        for (int ch = 0; ch < channels; ++ch) {
            // Wet chain order (binding): tap sum -> character -> filters ->
            // width -> duck x wetGain. Clean mode is a true bypass, so the
            // default output stays bit-identical.
            wet[ch] = characterStages_[static_cast<size_t>(ch)].processSample(wet[ch]);
            if (lowCutHz_ > 0.0f)
                wet[ch] = lowCutFilters_[static_cast<size_t>(ch)].processSample(wet[ch]);
            if (highCutHz_ < 20000.0f)
                wet[ch] = highCutFilters_[static_cast<size_t>(ch)].processSample(wet[ch]);
        }

        // Stereo width on the wet pair (mid/side). Width 1 is a bit-exact
        // passthrough, so it is skipped entirely; mono is left untouched.
        if (channels >= 2 && width_ != 1.0f) {
            const float mid = (wet[0] + wet[1]) * 0.5f;
            const float side = (wet[0] - wet[1]) * 0.5f * width_;
            wet[0] = mid + side;
            wet[1] = mid - side;
        }

        for (int ch = 0; ch < channels; ++ch)
            channelData[ch][n] = dry[ch] * dryGain_ + wet[ch] * wetGain_ * duckGain;

        // Viz output stats: read the final samples back AFTER they are written.
        float outSq = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            const float out = channelData[ch][n];
            outSq += out * out;
            outBlockPeak = std::max(outBlockPeak, std::abs(out));
        }
        outSq /= static_cast<float>(channels);
        outMs_ += (outSq - outMs_) * msCoef_;
        ++timeSamples_;
    }

    // timeSamples_ (post-loop) rides along as "now" so the UI can compute
    // event age. It is rebased to 0 only by prepare(); reset() leaves it
    // running, so "now" is monotonic across reset() like event timestamps.
    vizFeed_.writeLevels({ std::sqrt(inMs_), inBlockPeak,
                           std::sqrt(outMs_), outBlockPeak, lastDuckGain,
                           timeSamples_ });
}

} // namespace orbit
