#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

// Minimal host for the REAL Orbit parameter layout — no engine, no editor.
struct TestProcessor : juce::AudioProcessor {
    juce::AudioProcessorValueTreeState apvts;
    TestProcessor()
        : apvts(*this, nullptr, "PARAMS", orbit::params::createLayout()) {}
    const juce::String getName() const override { return "OrbitPresetTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
