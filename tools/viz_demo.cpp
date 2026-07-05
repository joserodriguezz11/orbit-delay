// viz_demo — live terminal readout of the OrbitEngine visualization feed.
//
// Runs the engine offline (48 kHz stereo, 480-sample blocks) with two taps,
// ping-pong and ducking enabled, feeding it a short burst at the start of
// each second. Per block it drains tap-fire events (one printed line per
// fire) and redraws a status line with IN/OUT meters and duck gain.
// Pacing is host-side only (sleep 10 ms per 10 ms block) — the engine
// itself is processed as fast as it is called.
//
// No JUCE. Consumes only OrbitEngine + orbit::viz types.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "OrbitEngine.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 480;      // 10 ms per block
constexpr int kNumChannels = 2;
constexpr int kDurationSeconds = 10;
constexpr int kBlocksPerSecond = 100;
constexpr int kBurstSamples = 240;   // 5 ms burst at the top of each second
constexpr double kBurstFreqHz = 997.0;

constexpr int kMeterWidth = 18;      // chars per level meter
constexpr int kIntensityWidth = 10;  // chars per fire-intensity bar
constexpr int kLineWidth = 78;       // pad printed lines so '\r' redraws cleanly

// RMS drawn as a filled bar, peak as a '|' marker (linear 0..1 scale).
std::string meterBar(float rms, float peak) {
    std::string bar(kMeterWidth, '-');
    const int rmsCells = static_cast<int>(std::lround(
        std::clamp(rms, 0.0f, 1.0f) * static_cast<float>(kMeterWidth)));
    for (int i = 0; i < rmsCells; ++i)
        bar[static_cast<std::size_t>(i)] = '#';
    const int peakCell = std::clamp(
        static_cast<int>(std::lround(std::clamp(peak, 0.0f, 1.0f)
                                     * static_cast<float>(kMeterWidth))) - 1,
        0, kMeterWidth - 1);
    bar[static_cast<std::size_t>(peakCell)] = '|';
    return bar;
}

std::string intensityBar(float intensity01) {
    const int cells = static_cast<int>(std::lround(
        std::clamp(intensity01, 0.0f, 1.0f) * static_cast<float>(kIntensityWidth)));
    std::string bar(kIntensityWidth, ' ');
    for (int i = 0; i < cells; ++i)
        bar[static_cast<std::size_t>(i)] = '#';
    return bar;
}

// Overwrite the status line ('\r'), print a full padded line, keep cursor on
// a fresh line so the next status redraw starts clean.
void printEventLine(const std::string& text) {
    std::printf("\r%-*s\n", kLineWidth, text.c_str());
}

} // namespace

int main() {
    orbit::OrbitEngine engine;
    engine.prepare(kSampleRate, kBlockSize, kNumChannels);

    orbit::TapSettings tap1;
    tap1.enabled = true;
    tap1.sync = orbit::dsp::SyncDivision::Free;
    tap1.timeSeconds = 0.375f;
    tap1.feedback = 0.55f;
    engine.setTap(0, tap1);

    orbit::TapSettings tap2 = tap1;
    tap2.timeSeconds = 0.75f;
    tap2.feedback = 0.35f;
    engine.setTap(1, tap2);

    engine.setPingPong(true);
    engine.setDuckAmount(0.8f);
    engine.setDryWet(0.5f);

    std::vector<float> left(kBlockSize, 0.0f);
    std::vector<float> right(kBlockSize, 0.0f);
    float* channels[kNumChannels] = { left.data(), right.data() };

    std::array<std::uint64_t, orbit::OrbitEngine::kNumTaps> fireCounts {};

    std::printf("viz_demo — OrbitEngine visualization feed (%d s @ %.0f Hz, %d-sample blocks)\n",
                kDurationSeconds, kSampleRate, kBlockSize);
    std::printf("tap 1 = 375 ms fb 0.55 | tap 2 = 750 ms fb 0.35 | ping-pong on | duck 0.8 | mix 0.5\n\n");

    const int totalBlocks = kDurationSeconds * kBlocksPerSecond;
    for (int block = 0; block < totalBlocks; ++block) {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);

        // One burst at the top of each second, amplitude alternating 0.9 / 0.5.
        if (block % kBlocksPerSecond == 0) {
            const int second = block / kBlocksPerSecond;
            const float amp = (second % 2 == 0) ? 0.9f : 0.5f;
            for (int i = 0; i < kBurstSamples; ++i) {
                const double phase = 2.0 * M_PI * kBurstFreqHz
                                     * static_cast<double>(i) / kSampleRate;
                const float sample = amp * static_cast<float>(std::sin(phase));
                left[static_cast<std::size_t>(i)] = sample;
                right[static_cast<std::size_t>(i)] = sample;
            }
        }

        engine.process(channels, kNumChannels, kBlockSize);

        // Drain fire events for this block.
        orbit::viz::VizFeed& feed = engine.vizFeed();
        orbit::viz::TapFireEvent event;
        char line[128];
        while (feed.popEvent(event)) {
            if (event.tapIndex < fireCounts.size())
                ++fireCounts[event.tapIndex];
            const double seconds = static_cast<double>(event.timeSamples) / kSampleRate;
            std::snprintf(line, sizeof(line), "  %7.3f s  tap %u  [%s] %.2f",
                          seconds, event.tapIndex + 1u,
                          intensityBar(event.intensity01).c_str(),
                          event.intensity01);
            printEventLine(line);
        }

        // Redraw the status line in place.
        const orbit::viz::LevelSnapshot levels = feed.readLevels();
        std::snprintf(line, sizeof(line), "  IN  [%s]  OUT [%s]  duck %3.0f%%",
                      meterBar(levels.inRms, levels.inPeak).c_str(),
                      meterBar(levels.outRms, levels.outPeak).c_str(),
                      levels.duckGain * 100.0f);
        std::printf("\r%-*s", kLineWidth, line);
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::printf("\n\nSummary — total fires per tap:\n");
    for (std::size_t tap = 0; tap < fireCounts.size(); ++tap)
        std::printf("  tap %zu: %llu fires\n", tap + 1,
                    static_cast<unsigned long long>(fireCounts[tap]));
    return 0;
}
