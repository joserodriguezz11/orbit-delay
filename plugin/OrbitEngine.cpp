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
        lowCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::HighPass);
        highCutFilters_[static_cast<size_t>(ch)].prepare(sampleRate, dsp::OnePole::Mode::LowPass);
    }
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
        lowCutFilters_[static_cast<size_t>(ch)].reset();
        highCutFilters_[static_cast<size_t>(ch)].reset();
    }
}

void OrbitEngine::setTap(int index, const TapSettings& settings) {
    if (index < 0 || index >= kNumTaps)
        return;
    taps_[static_cast<size_t>(index)] = settings;
    for (auto& line : lines_[static_cast<size_t>(index)])
        line.setFeedback(settings.feedback);
    applyTapTime(index);
}

void OrbitEngine::setDryWet(float mix01) {
    mix_ = std::clamp(mix01, 0.0f, 1.0f);
    dryGain_ = std::sqrt(1.0f - mix_);
    wetGain_ = std::sqrt(mix_);
}

void OrbitEngine::setDuckAmount(float amount01) { ducker_.setAmount(amount01); }

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
    const int channels = std::clamp(numChannels, 1, numChannels_);
    for (int n = 0; n < numSamples; ++n) {
        float dryLevel = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            dryLevel += std::abs(channelData[ch][n]);
        dryLevel /= static_cast<float>(channels);
        const float duckGain = ducker_.processGain(dryLevel);

        const float modOffset = lfo_.processSample() * modDepthSamples_;

        for (int ch = 0; ch < channels; ++ch) {
            const float dry = channelData[ch][n];
            float wet = 0.0f;
            for (int t = 0; t < kNumTaps; ++t) {
                // Disabled taps keep processing (buffers stay warm -> no clicks
                // on re-enable) but don't contribute to the mix.
                auto& line = lines_[static_cast<size_t>(t)][static_cast<size_t>(ch)];
                line.setModulationSamples(modOffset * modScale_[static_cast<size_t>(t)]);
                const float tapOut = line.processSample(dry);
                if (taps_[static_cast<size_t>(t)].enabled)
                    wet += tapOut;
            }
            if (lowCutHz_ > 0.0f)
                wet = lowCutFilters_[static_cast<size_t>(ch)].processSample(wet);
            if (highCutHz_ < 20000.0f)
                wet = highCutFilters_[static_cast<size_t>(ch)].processSample(wet);
            channelData[ch][n] = dry * dryGain_ + wet * wetGain_ * duckGain;
        }
    }
}

} // namespace orbit
