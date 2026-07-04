#pragma once
#include <array>
#include <juce_audio_utils/juce_audio_utils.h>
#include "OrbitEngine.h"
#include "Parameters.h"

class OrbitAudioProcessor : public juce::AudioProcessor {
public:
    OrbitAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return orbit::OrbitEngine::kMaxDelaySeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateEngineFromParameters();

    orbit::OrbitEngine engine_;

    struct TapParamPointers {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* time = nullptr;
        std::atomic<float>* sync = nullptr;
        std::atomic<float>* feedback = nullptr;
        std::atomic<float>* reverse = nullptr;
        std::atomic<float>* pitch = nullptr;
    };
    std::array<TapParamPointers, orbit::OrbitEngine::kNumTaps> tapParams_ {};
    std::atomic<float>* dryWetParam_ = nullptr;
    std::atomic<float>* duckParam_ = nullptr;
    std::atomic<float>* widthParam_ = nullptr;
    std::atomic<float>* pingPongParam_ = nullptr;
    std::atomic<float>* characterModeParam_ = nullptr;
    std::atomic<float>* modDepthParam_ = nullptr;
    std::atomic<float>* modRateParam_ = nullptr;
    std::atomic<float>* lowCutParam_ = nullptr;
    std::atomic<float>* highCutParam_ = nullptr;
    std::atomic<float>* freezeParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbitAudioProcessor)
};
