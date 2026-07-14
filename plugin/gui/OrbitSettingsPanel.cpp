#include "OrbitSettingsPanel.h"
#include "OrbitFonts.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

#ifndef JucePlugin_VersionString
#define JucePlugin_VersionString "dev"   // console test rig has no plugin defines
#endif

namespace {
constexpr float kPanelW = 560.0f, kPanelH = 390.0f;

const char* const kHowTo[] = {
    "Drag an orb - x sets tap time, y sets feedback. Flick to throw.",
    "TIME and FEEDBACK knobs ride the selected orb's axes.",
    "SYNC locks a tap to tempo divisions at the host BPM.",
    "CLEAN / TAPE / GRIT re-voice and recolour the field.",
    "FREEZE holds the delay buffer; REV plays a tap backwards.",
    "Click the preset capsule to open the browser; A/B compares edits.",
};
} // namespace

juce::Rectangle<float> OrbitSettingsPanel::panelBounds() const {
    return { (float(theme::kWindowW) - kPanelW) / 2.0f, 70.0f, kPanelW, kPanelH };
}

void OrbitSettingsPanel::mouseUp(const juce::MouseEvent& e) {
    const auto pos = e.position;
    const auto panel = panelBounds();
    const bool onClose_ = !panel.contains(pos)
        || juce::Rectangle<float>(panel.getRight() - 38.0f, panel.getY() + 10.0f,
                                  22.0f, 22.0f).expanded(4.0f).contains(pos);
    if (onClose_ && onClose != nullptr)
        onClose();
}

void OrbitSettingsPanel::paint(juce::Graphics& g) {
    // Scrim + panel chrome — mirrors OrbitPresetBrowser.
    g.fillAll(juce::Colour { 0xff040408 }.withAlpha(0.45f));
    const auto panel = panelBounds();
    g.setColour(juce::Colour { 0xff0c0c13 });
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(theme::bone50.withAlpha(0.16f));
    g.drawRoundedRectangle(panel, 14.0f, 1.0f);

    const auto sectionHead = [&] (const char* title, float y) {
        g.setColour(theme::ember);
        g.fillRect(panel.getX() + 16.0f, y + 5.0f, 4.0f, 4.0f);
        g.setFont(fonts::tracked(fonts::monoSemiBold(9.0f), 0.26f));
        g.setColour(theme::bone50.withAlpha(0.55f));
        g.drawText(title, juce::Rectangle<float>(panel.getX() + 27.0f, y, 200.0f, 14.0f),
                   juce::Justification::centredLeft, false);
    };

    // Close x.
    g.setColour(theme::bone50.withAlpha(0.14f));
    g.drawEllipse(panel.getRight() - 38.0f, panel.getY() + 10.0f, 22.0f, 22.0f, 1.0f);
    g.setColour(theme::bone50.withAlpha(0.6f));
    g.setFont(fonts::mono(11.0f));
    g.drawText(juce::String::fromUTF8("\xc3\x97"),
               juce::Rectangle<float>(panel.getRight() - 38.0f, panel.getY() + 10.0f,
                                      22.0f, 22.0f),
               juce::Justification::centred, false);

    // ABOUT.
    float y = panel.getY() + 12.0f;
    sectionHead("ABOUT", y);
    y += 26.0f;
    g.setFont(fonts::sansSemiBold(14.0f));
    g.setColour(theme::text());
    g.drawText("ORBIT - 4-tap echo",
               juce::Rectangle<float>(panel.getX() + 27.0f, y, 300.0f, 16.0f),
               juce::Justification::centredLeft, false);
    y += 20.0f;
    g.setFont(fonts::mono(9.5f));
    g.setColour(theme::bone50.withAlpha(0.55f));
    g.drawText(juce::String("Version ") + JucePlugin_VersionString
                   + "  -  Synthios Records",
               juce::Rectangle<float>(panel.getX() + 27.0f, y, 400.0f, 12.0f),
               juce::Justification::centredLeft, false);
    y += 30.0f;
    g.setColour(theme::bone50.withAlpha(0.07f));
    g.fillRect(panel.getX(), y, panel.getWidth(), 1.0f);
    y += 14.0f;

    // HOW TO USE.
    sectionHead("HOW TO USE", y);
    y += 28.0f;
    g.setFont(fonts::sans(12.0f));
    for (const auto* line : kHowTo) {
        g.setColour(theme::ember.withAlpha(0.8f));
        g.fillEllipse(panel.getX() + 29.0f, y + 5.0f, 3.0f, 3.0f);
        g.setColour(theme::bone50.withAlpha(0.78f));
        g.drawText(line, juce::Rectangle<float>(panel.getX() + 42.0f, y,
                                                panel.getWidth() - 70.0f, 14.0f),
                   juce::Justification::centredLeft, false);
        y += 26.0f;
    }
}
