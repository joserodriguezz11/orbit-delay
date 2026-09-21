#include "OrbitFonts.h"
#include "OrbitFontBinary.h"

namespace orbit::gui::fonts {
namespace {

// Typefaces load once per JUCE runtime. DeletedAtShutdown (not a plain
// function-local static) so the cache is torn down with shutdownJuce_GUI —
// a process-lifetime cache trips the leak detector in the headless test rig,
// which cycles the JUCE runtime per test case.
struct TypefaceCache : private juce::DeletedAtShutdown {
    std::map<const char*, juce::Typeface::Ptr> map;

    JUCE_DECLARE_SINGLETON(TypefaceCache, false)
private:
    TypefaceCache() = default;
    ~TypefaceCache() override { clearSingletonInstance(); }
};

juce::Font fromBinary(const char* data, int size, float height) {
    auto& tf = TypefaceCache::getInstance()->map[data];
    if (tf == nullptr)
        tf = juce::Typeface::createSystemTypefaceFor(data, size_t(size));
    return juce::Font { juce::FontOptions { tf }.withHeight(height) };
}

} // namespace

JUCE_IMPLEMENT_SINGLETON(TypefaceCache)

juce::Font mono(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoRegular_ttf,
                      OrbitFontBinary::SpaceMonoRegular_ttfSize, height);
}
juce::Font monoMedium(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoRegular_ttf,
                      OrbitFontBinary::SpaceMonoRegular_ttfSize, height);
}
juce::Font monoSemiBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoBold_ttf,
                      OrbitFontBinary::SpaceMonoBold_ttfSize, height);
}
juce::Font monoBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceMonoBold_ttf,
                      OrbitFontBinary::SpaceMonoBold_ttfSize, height);
}
juce::Font sans(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskMedium_ttf,
                      OrbitFontBinary::SpaceGroteskMedium_ttfSize, height);
}
juce::Font sansSemiBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskBold_ttf,
                      OrbitFontBinary::SpaceGroteskBold_ttfSize, height);
}
juce::Font sansExtraBold(float height) {
    return fromBinary(OrbitFontBinary::SpaceGroteskBold_ttf,
                      OrbitFontBinary::SpaceGroteskBold_ttfSize, height);
}
juce::Font watermark(float height) {
    return fromBinary(OrbitFontBinary::SyneExtraBold_ttf,
                      OrbitFontBinary::SyneExtraBold_ttfSize, height);
}

juce::Font tracked(const juce::Font& base, float emTracking) {
    return base.withExtraKerningFactor(emTracking);
}

} // namespace orbit::gui::fonts
