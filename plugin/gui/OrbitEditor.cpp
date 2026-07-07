#include "OrbitEditor.h"
#include "OrbitTheme.h"

OrbitEditor::OrbitEditor(OrbitAudioProcessor& proc)
    : juce::AudioProcessorEditor(proc) {
    setResizable(true, true);
    // Fixed-aspect proportional resize, 100%-200% of base. Our own member
    // constrainer (not the editor's default one) so the aspect ratio and
    // limits live in one object that survives for the editor's lifetime.
    constrainer_.setFixedAspectRatio(double(kBaseW) / double(kBaseH));
    constrainer_.setSizeLimits(kBaseW, kBaseH, kBaseW * 2, kBaseH * 2);
    setConstrainer(&constrainer_);
    setSize(kBaseW, kBaseH);
}

void OrbitEditor::paint(juce::Graphics& g) {
    g.fillAll(orbit::gui::theme::ink900);
}

void OrbitEditor::resized() {
    // Intentionally empty — child layout arrives in Task 5.
}
