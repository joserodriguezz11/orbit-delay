#include "OrbitPresetBrowser.h"
#include "OrbitFonts.h"
#include "OrbitTheme.h"

namespace theme = orbit::gui::theme;
namespace fonts = orbit::gui::fonts;

namespace {
constexpr float kPanelX = 20.0f, kPanelY = 56.0f, kPanelW = 860.0f, kPanelH = 420.0f;
constexpr float kHeadH = 38.0f, kChipsH = 42.0f, kRowH = 30.0f, kRowGap = 2.0f;
constexpr int kGridCols = 4;   // 4 x 10 rows — the 40-preset factory set fits
} // namespace

OrbitPresetBrowser::OrbitPresetBrowser(orbit::PresetManager& presets)
    : presets_(presets) {
    refresh();
}

void OrbitPresetBrowser::refresh() {
    all_.clear();
    int n = 1;
    for (const auto& p : presets_.factoryPresets())
        all_.add({ p, n++ });
    for (const auto& p : presets_.userPresets())
        all_.add({ p, n++ });
    rebuildVisible();
}

void OrbitPresetBrowser::rebuildVisible() {
    visible_.clear();
    for (const auto& e : all_)
        if (tagFilter_.isEmpty() || e.info.tags.containsIgnoreCase(tagFilter_))
            visible_.add(e);
    repaint();
}

void OrbitPresetBrowser::setTagFilter(const juce::String& tag) {
    tagFilter_ = tag;
    rebuildVisible();
}

int OrbitPresetBrowser::tagCount() const {
    return orbit::PresetManager::tagVocabulary().size() + 1;
}

int OrbitPresetBrowser::visibleCount() const { return visible_.size(); }

juce::String OrbitPresetBrowser::visibleName(int row) const {
    return juce::isPositiveAndBelow(row, visible_.size())
        ? visible_.getReference(row).info.name : juce::String();
}

void OrbitPresetBrowser::pickVisible(int row) {
    if (juce::isPositiveAndBelow(row, visible_.size())) {
        presets_.loadPreset(visible_.getReference(row).info);
        repaint();
    }
}

juce::Rectangle<float> OrbitPresetBrowser::panelBounds() const {
    return { kPanelX, kPanelY, kPanelW, kPanelH };
}

juce::Rectangle<float> OrbitPresetBrowser::chipBounds(int chip) const {
    // ALL + tags flow left-to-right (single row is enough for 7 chips).
    const auto panel = panelBounds();
    float x = panel.getX() + 16.0f;
    const auto& tags = orbit::PresetManager::tagVocabulary();
    for (int i = 0; i <= tags.size(); ++i) {
        const auto label = i == 0 ? juce::String("ALL") : tags[i - 1].toUpperCase();
        const float w = 22.0f + fonts::monoSemiBold(8.5f).getStringWidthFloat(label) * 1.12f;
        if (i == chip)
            return { x, panel.getY() + kHeadH + 10.0f, w, 22.0f };
        x += w + 5.0f;
    }
    return {};
}

juce::Rectangle<float> OrbitPresetBrowser::rowBounds(int visibleRow) const {
    const auto panel = panelBounds();
    const float gridY = panel.getY() + kHeadH + kChipsH;
    const float colW = (panel.getWidth() - 20.0f) / float(kGridCols);
    const int col = visibleRow % kGridCols, r = visibleRow / kGridCols;
    return { panel.getX() + 10.0f + float(col) * colW,
             gridY + float(r) * (kRowH + kRowGap), colW, kRowH };
}

bool OrbitPresetBrowser::rowFits(int visibleRow) const {
    return rowBounds(visibleRow).getBottom() <= panelBounds().getBottom() - 8.0f;
}

void OrbitPresetBrowser::mouseUp(const juce::MouseEvent& e) {
    const auto pos = e.position;
    if (!panelBounds().contains(pos)) {
        if (onClose != nullptr)
            onClose();
        return;
    }
    // Close button.
    const auto panel = panelBounds();
    if (juce::Rectangle<float>(panel.getRight() - 40.0f, panel.getY() + 8.0f,
                               26.0f, 26.0f).contains(pos)) {
        if (onClose != nullptr)
            onClose();
        return;
    }
    // Tag chips.
    const auto& tags = orbit::PresetManager::tagVocabulary();
    for (int i = 0; i <= tags.size(); ++i)
        if (chipBounds(i).contains(pos))
            return setTagFilter(i == 0 ? juce::String() : tags[i - 1]);
    // Preset rows — only rows the grid actually draws are clickable.
    for (int rw = 0; rw < visible_.size() && rowFits(rw); ++rw)
        if (rowBounds(rw).contains(pos))
            return pickVisible(rw);
}

void OrbitPresetBrowser::paint(juce::Graphics& g) {
    // Scrim over the whole editor.
    g.fillAll(juce::Colour { 0xff040408 }.withAlpha(0.45f));

    const auto panel = panelBounds();
    g.setColour(juce::Colour { 0xff0c0c13 });
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(theme::bone50.withAlpha(0.16f));
    g.drawRoundedRectangle(panel, 14.0f, 1.0f);

    // Head: dot + PRESETS + count + ×.
    g.setColour(theme::ember);
    g.fillRect(panel.getX() + 16.0f, panel.getY() + 17.0f, 4.0f, 4.0f);
    g.setFont(fonts::tracked(fonts::monoSemiBold(9.0f), 0.26f));
    g.setColour(theme::bone50.withAlpha(0.55f));
    g.drawText("PRESETS",
               juce::Rectangle<float>(panel.getX() + 27.0f, panel.getY() + 12.0f, 90.0f, 14.0f),
               juce::Justification::centredLeft, false);
    g.setFont(fonts::monoSemiBold(9.0f));
    g.setColour(theme::bone50.withAlpha(0.35f));
    g.drawText(juce::String(visible_.size()) + " / " + juce::String(all_.size()),
               juce::Rectangle<float>(panel.getX() + 110.0f, panel.getY() + 12.0f, 70.0f, 14.0f),
               juce::Justification::centredLeft, false);
    g.setColour(theme::bone50.withAlpha(0.14f));
    g.drawEllipse(panel.getRight() - 38.0f, panel.getY() + 10.0f, 22.0f, 22.0f, 1.0f);
    g.setColour(theme::bone50.withAlpha(0.6f));
    g.setFont(fonts::mono(11.0f));
    g.drawText(juce::String::fromUTF8("\xc3\x97"),
               juce::Rectangle<float>(panel.getRight() - 38.0f, panel.getY() + 10.0f, 22.0f, 22.0f),
               juce::Justification::centred, false);
    g.setColour(theme::bone50.withAlpha(0.07f));
    g.fillRect(panel.getX(), panel.getY() + kHeadH, panel.getWidth(), 1.0f);

    // Tag chips.
    const auto& tags = orbit::PresetManager::tagVocabulary();
    for (int i = 0; i <= tags.size(); ++i) {
        const auto b = chipBounds(i);
        const auto label = i == 0 ? juce::String("ALL") : tags[i - 1].toUpperCase();
        const bool on = i == 0 ? tagFilter_.isEmpty() : tagFilter_ == tags[i - 1];
        if (on) {
            g.setColour(theme::ember);
            g.fillRoundedRectangle(b, b.getHeight() / 2.0f);
        }
        g.setColour(on ? theme::ember : theme::bone50.withAlpha(0.14f));
        g.drawRoundedRectangle(b, b.getHeight() / 2.0f, 1.0f);
        g.setColour(on ? theme::text() : theme::bone50.withAlpha(0.55f));
        g.setFont(fonts::tracked(fonts::monoSemiBold(8.5f), 0.12f));
        g.drawText(label, b, juce::Justification::centred, false);
    }

    // Preset grid (kGridCols columns; 40 factory presets fit exactly).
    const auto current = presets_.currentPresetName();
    int drawn = 0;
    for (int rw = 0; rw < visible_.size(); ++rw) {
        if (!rowFits(rw))
            break;   // no scrolling in v1 — overflow is counted below
        ++drawn;
        const auto b = rowBounds(rw);
        const auto& e = visible_.getReference(rw);
        if (e.info.name == current) {
            g.setColour(theme::ember.withAlpha(0.14f));
            g.fillRoundedRectangle(b, 8.0f);
        }
        g.setFont(fonts::mono(9.0f));
        g.setColour(theme::bone50.withAlpha(0.38f));
        g.drawText(juce::String(e.number).paddedLeft('0', 2),
                   b.withX(b.getX() + 10.0f).withWidth(20.0f),
                   juce::Justification::centredLeft, false);
        g.setFont(fonts::sans(12.5f));
        g.setColour(theme::text());
        g.drawText(e.info.name, b.withX(b.getX() + 34.0f).withWidth(b.getWidth() - 80.0f),
                   juce::Justification::centredLeft, false);
        g.setFont(fonts::tracked(fonts::mono(8.0f), 0.1f));
        g.setColour(theme::bone50.withAlpha(0.35f));
        g.drawText(e.info.tags.upToFirstOccurrenceOf(",", false, false)
                       .trim().toUpperCase().substring(0, 3),
                   b.withX(b.getRight() - 40.0f).withWidth(30.0f),
                   juce::Justification::centredRight, false);
    }

    // Grid overflow (user presets past capacity): say so instead of hiding it.
    if (drawn < visible_.size()) {
        g.setFont(fonts::tracked(fonts::monoSemiBold(8.0f), 0.12f));
        g.setColour(theme::bone50.withAlpha(0.4f));
        g.drawText("+" + juce::String(visible_.size() - drawn) + " MORE (USE PRESET ARROWS)",
                   juce::Rectangle<float>(panel.getX(), panel.getBottom() - 20.0f,
                                          panel.getWidth() - 14.0f, 12.0f),
                   juce::Justification::centredRight, false);
    }
}
