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
    applyTapTimes();
}

void OrbitEngine::reset() {
    for (auto& tapLines : lines_)
        for (auto& line : tapLines)
            line.reset();
    ducker_.reset();
}

void OrbitEngine::setTap(int index, const TapSettings& settings) {
    if (index < 0 || index >= kNumTaps)
        return;
    taps_[static_cast<size_t>(index)] = settings;
    for (auto& line : lines_[static_cast<size_t>(index)])
        line.setFeedback(settings.feedback);
    applyTapTime(index);
}

void OrbitEngine::setDryWet(float mix01) { mix_ = std::clamp(mix01, 0.0f, 1.0f); }

void OrbitEngine::setDuckAmount(float amount01) { ducker_.setAmount(amount01); }

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

        for (int ch = 0; ch < channels; ++ch) {
            const float dry = channelData[ch][n];
            float wet = 0.0f;
            for (int t = 0; t < kNumTaps; ++t) {
                // Disabled taps keep processing (buffers stay warm -> no clicks
                // on re-enable) but don't contribute to the mix.
                const float tapOut =
                    lines_[static_cast<size_t>(t)][static_cast<size_t>(ch)].processSample(dry);
                if (taps_[static_cast<size_t>(t)].enabled)
                    wet += tapOut;
            }
            channelData[ch][n] = dry * (1.0f - mix_) + wet * mix_ * duckGain;
        }
    }
}

} // namespace orbit
