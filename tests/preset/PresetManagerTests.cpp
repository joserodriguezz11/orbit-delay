#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "TestProcessor.h"
#include "PresetManager.h"

using Catch::Approx;

struct Fixture {
    juce::ScopedJuceInitialiser_GUI juceInit;
    TestProcessor proc;
    juce::TemporaryFile tmpDirToken;       // unique path base
    juce::File dir;
    std::unique_ptr<orbit::PresetManager> pm;
    Fixture() {
        dir = tmpDirToken.getFile().getSiblingFile("orbit_preset_test_dir");
        dir.createDirectory();
        pm = std::make_unique<orbit::PresetManager>(proc.apvts, dir);
    }
    ~Fixture() { dir.deleteRecursively(); }
    float get(const char* id) { return proc.apvts.getRawParameterValue(id)->load(); }
    void set(const char* id, float v) {
        auto* p = proc.apvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(v));
    }
};

TEST_CASE_METHOD(Fixture, "save then load round-trips every parameter and metadata") {
    set("mix_width", 1.7f);
    set("tap1_feedback", 0.62f);
    set("tap3_pitch", 7.0f);
    REQUIRE(pm->saveUserPreset("My Wide Slap", "Vocals,Utility", "test preset", false));
    // scramble
    set("mix_width", 0.3f);
    set("tap1_feedback", 0.1f);
    set("tap3_pitch", -3.0f);
    const auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    CHECK(users[0].name == "My Wide Slap");
    CHECK(users[0].tags == "Vocals,Utility");
    CHECK(users[0].description == "test preset");
    CHECK_FALSE(users[0].isFactory);
    REQUIRE(pm->loadPreset(users[0]));
    CHECK(get("mix_width") == Approx(1.7f));
    CHECK(get("tap1_feedback") == Approx(0.62f));
    CHECK(get("tap3_pitch") == Approx(7.0f));
    CHECK(pm->currentPresetName() == "My Wide Slap");
}

TEST_CASE_METHOD(Fixture, "corrupt and foreign-product files are rejected without touching state") {
    set("mix_width", 1.5f);
    dir.getChildFile("bad.orbitpreset").replaceWithText("this is not xml <");
    dir.getChildFile("foreign.orbitpreset")
       .replaceWithText("<PARAMS product=\"nebula\" stateVersion=\"1\"/>");
    auto users = pm->userPresets();
    REQUIRE(users.size() == 2);
    for (auto& u : users)
        CHECK_FALSE(pm->loadPreset(u));
    CHECK(get("mix_width") == Approx(1.5f));       // untouched
    CHECK(pm->currentPresetName() == juce::String());
}

TEST_CASE_METHOD(Fixture, "dirty flag lifecycle") {
    REQUIRE(pm->saveUserPreset("Base", "Utility", "", false));
    CHECK_FALSE(pm->isModified());                 // save clears
    REQUIRE(pm->loadPreset(pm->userPresets()[0]));
    CHECK_FALSE(pm->isModified());                 // load clears
    set("tap2_feedback", 0.4f);
    CHECK(pm->isModified());                       // edit sets
}

TEST_CASE_METHOD(Fixture, "tag filtering, overwrite protection, delete and rename") {
    // filterByTag spans factory + user lists by design; this case exercises
    // the user-preset half, so filter the factory entries back out.
    auto userByTag = [this](const juce::String& tag) {
        juce::Array<orbit::PresetInfo> out;
        for (const auto& p : pm->filterByTag(tag))
            if (!p.isFactory)
                out.add(p);
        return out;
    };
    REQUIRE(pm->saveUserPreset("VoxOne", "Vocals", "", false));
    REQUIRE(pm->saveUserPreset("DrumOne", "Drums", "", false));
    CHECK_FALSE(pm->saveUserPreset("VoxOne", "Vocals", "", false));   // exists
    REQUIRE(pm->saveUserPreset("VoxOne", "Vocals", "v2", true));      // overwrite ok
    CHECK(userByTag("Vocals").size() == 1);
    CHECK(userByTag("Drums").size() == 1);
    CHECK(userByTag("Ambient").size() == 0);
    auto vox = userByTag("Vocals")[0];
    REQUIRE(pm->renameUserPreset(vox, "VoxTwo"));
    CHECK(userByTag("Vocals")[0].name == "VoxTwo");
    REQUIRE(pm->deleteUserPreset(userByTag("Vocals")[0]));
    CHECK(pm->userPresets().size() == 1);
}

TEST_CASE_METHOD(Fixture, "filterByTag spans factory and user presets") {
    const auto factoryOnly = pm->filterByTag("Utility");
    CHECK(factoryOnly.size() >= 4);                // factory set ships >= 4 Utility presets
    int factoryCount = 0;
    for (const auto& p : factoryOnly) {
        CHECK(p.isFactory);
        if (p.isFactory)
            ++factoryCount;
    }
    CHECK(factoryCount >= 4);

    REQUIRE(pm->saveUserPreset("UserUtil", "Utility", "", false));
    const auto combined = pm->filterByTag("Utility");
    CHECK(combined.size() == factoryOnly.size() + 1);   // grows by exactly 1
    int factoryAfter = 0, userAfter = 0;
    for (const auto& p : combined)
        (p.isFactory ? factoryAfter : userAfter)++;
    CHECK(factoryAfter == factoryCount);           // factory entries untouched
    CHECK(userAfter == 1);
}

TEST_CASE_METHOD(Fixture, "filenames are sanitized") {
    REQUIRE(pm->saveUserPreset("A/B: <Test>?", "Utility", "", false));
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.orbitpreset");
    REQUIRE(files.size() == 1);
    CHECK_FALSE(files[0].getFileName().containsAnyOf("/\\:<>?*|\""));
}

TEST_CASE_METHOD(Fixture, "A/B: edits survive a round trip") {
    set("mix_width", 1.9f);                        // edit on A
    pm->toggleAB();                                // -> B (default state)
    CHECK(pm->isSlotB());
    CHECK(get("mix_width") != Approx(1.9f).epsilon(0.001));
    set("tap1_feedback", 0.33f);                   // edit on B
    pm->toggleAB();                                // -> back to A
    CHECK_FALSE(pm->isSlotB());
    CHECK(get("mix_width") == Approx(1.9f));       // A's edit preserved
    pm->toggleAB();                                // -> B again
    CHECK(get("tap1_feedback") == Approx(0.33f));  // B's edit preserved
}

TEST_CASE_METHOD(Fixture, "copyAB clones live over inactive and keeps live untouched") {
    set("mix_width", 1.8f);
    pm->copyAB();                                  // B := A
    CHECK(get("mix_width") == Approx(1.8f));       // live unchanged
    pm->toggleAB();                                // -> B
    CHECK(get("mix_width") == Approx(1.8f));       // clone applied
}

TEST_CASE_METHOD(Fixture, "toggleAB sets modified and keeps preset name") {
    REQUIRE(pm->saveUserPreset("Named", "Utility", "", false));
    REQUIRE(pm->loadPreset(pm->userPresets()[0]));
    pm->toggleAB();
    CHECK(pm->isModified());
    CHECK(pm->currentPresetName() == "Named");
}

//==============================================================================
// Factory preset validation (Task 3). Tree shape confirmed from a real
// saveUserPreset dump: <PARAMS> root with <PARAM id=... value=.../> children
// plus a <PresetMeta> child — the brief's PARAM/id naming matches reality.

TEST_CASE_METHOD(Fixture, "factory set: 40 presets, unique names, valid tags, ordered") {
    const auto& f = pm->factoryPresets();
    REQUIRE(f.size() == 40);
    juce::StringArray names;
    const juce::StringArray vocab { "Vocals", "Drums", "Ambient", "Dub", "Lo-fi", "Utility" };
    for (auto& p : f) {
        CHECK(p.isFactory);
        CHECK(p.name.isNotEmpty());
        names.addIfNotAlreadyThere(p.name);
        for (auto& t : juce::StringArray::fromTokens(p.tags, ",", ""))
            CHECK(vocab.contains(t.trim()));
        CHECK(p.description.isNotEmpty());
    }
    CHECK(names.size() == 40);                     // unique
}

TEST_CASE_METHOD(Fixture, "every factory preset loads and every value is in range") {
    for (auto& p : pm->factoryPresets()) {
        REQUIRE(pm->loadPreset(p));
        for (auto* param : proc.getParameters()) {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param);
            REQUIRE(rp != nullptr);
            const float norm = rp->getValue();
            CHECK(norm >= 0.0f);
            CHECK(norm <= 1.0f);
        }
        CHECK(pm->currentPresetName() == p.name);
        CHECK_FALSE(pm->isModified());
    }
}

TEST_CASE_METHOD(Fixture, "factory presets reference only real parameter IDs") {
    // Loading via replaceState silently drops unknown children; guard by
    // checking each preset XML's parameter ids against the layout.
    juce::StringArray known;
    for (auto* param : proc.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            known.add(wid->paramID);
    for (auto& p : pm->factoryPresets()) {
        const auto xml = pm->presetXmlFor(p);
        auto tree = juce::ValueTree::fromXml(xml);
        REQUIRE(tree.isValid());
        for (int i = 0; i < tree.getNumChildren(); ++i) {
            auto child = tree.getChild(i);
            if (child.hasType("PARAM"))
                CHECK(known.contains(child.getProperty("id").toString()));
        }
    }
}
