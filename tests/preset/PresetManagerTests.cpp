#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>
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
    REQUIRE(pm->saveUserPreset("My Wide Slap", "Wide,Utility", "test preset", false));
    // scramble
    set("mix_width", 0.3f);
    set("tap1_feedback", 0.1f);
    set("tap3_pitch", -3.0f);
    const auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    CHECK(users[0].name == "My Wide Slap");
    CHECK(users[0].tags == "Wide,Utility");
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
    REQUIRE(pm->saveUserPreset("VoxOne", "Wide", "", false));
    REQUIRE(pm->saveUserPreset("DrumOne", "Rhythm", "", false));
    CHECK_FALSE(pm->saveUserPreset("VoxOne", "Wide", "", false));   // exists
    REQUIRE(pm->saveUserPreset("VoxOne", "Wide", "v2", true));      // overwrite ok
    CHECK(userByTag("Wide").size() == 1);
    CHECK(userByTag("Rhythm").size() == 1);
    CHECK(userByTag("Ambient").size() == 0);
    auto vox = userByTag("Wide")[0];
    REQUIRE(pm->renameUserPreset(vox, "VoxTwo"));
    CHECK(userByTag("Wide")[0].name == "VoxTwo");
    REQUIRE(pm->deleteUserPreset(userByTag("Wide")[0]));
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
    REQUIRE(pm->isModified());                     // dirty starting state
    pm->copyAB();                                  // B := A
    CHECK(get("mix_width") == Approx(1.8f));       // live unchanged
    CHECK(pm->isModified());                       // dirty flag untouched (stays dirty)
    CHECK(pm->currentPresetName() == juce::String());   // name untouched
    CHECK_FALSE(pm->isSlotB());                    // active slot untouched
    pm->toggleAB();                                // -> B
    CHECK(get("mix_width") == Approx(1.8f));       // clone applied
}

TEST_CASE_METHOD(Fixture, "copyAB from a clean state leaves flag, name and slot untouched") {
    REQUIRE(pm->saveUserPreset("CleanBase", "Utility", "", false));
    REQUIRE_FALSE(pm->isModified());               // clean starting state
    pm->copyAB();                                  // inactive := live
    CHECK_FALSE(pm->isModified());                 // stays clean
    CHECK(pm->currentPresetName() == "CleanBase"); // name untouched
    CHECK_FALSE(pm->isSlotB());                    // active slot untouched

    pm->toggleAB();                                // -> B (sets modified by contract)
    REQUIRE(pm->isSlotB());
    pm->copyAB();                                  // A := live, from slot B
    CHECK(pm->isSlotB());                          // active slot still untouched
    CHECK(pm->currentPresetName() == "CleanBase");
}

TEST_CASE_METHOD(Fixture, "toggleAB sets modified and keeps preset name") {
    REQUIRE(pm->saveUserPreset("Named", "Utility", "", false));
    REQUIRE(pm->loadPreset(pm->userPresets()[0]));
    pm->toggleAB();
    CHECK(pm->isModified());
    CHECK(pm->currentPresetName() == "Named");
}

TEST_CASE_METHOD(Fixture, "A/B slots never leak into serialized state (tripwire)") {
    // TestProcessor's getStateInformation is a stub owned by another rig, so
    // this asserts the apvts state-equivalence the real processor serializes:
    // the same copyState() + product/stateVersion envelope + writeToStream
    // that OrbitAudioProcessor::getStateInformation performs.
    auto serialize = [this] {
        auto state = proc.apvts.copyState();
        state.setProperty("product", "orbit", nullptr);
        state.setProperty("stateVersion", 1, nullptr);
        juce::MemoryBlock mb;
        juce::MemoryOutputStream stream(mb, false);
        state.writeToStream(stream);
        return mb;
    };

    const float fbDefault = get("tap1_feedback");
    set("mix_width", 1.7f);
    const auto before = serialize();

    // Scribble on both slots without net-changing the live parameter state.
    pm->copyAB();                    // inactive := live
    pm->toggleAB();                  // -> B (holds the values just copied)
    set("tap1_feedback", 0.9f);      // divergent state now lives in slot B
    pm->toggleAB();                  // -> back to A; B keeps the 0.9 edit
    REQUIRE_FALSE(pm->isSlotB());
    REQUIRE(get("tap1_feedback") == Approx(fbDefault));   // live state restored

    const auto after = serialize();
    const auto treeBefore = juce::ValueTree::readFromData(before.getData(), before.getSize());
    const auto treeAfter  = juce::ValueTree::readFromData(after.getData(), after.getSize());
    REQUIRE(treeBefore.isValid());
    REQUIRE(treeAfter.isValid());
    CHECK(treeAfter.isEquivalentTo(treeBefore));   // slot mutations invisible

    // Parameter-only tree: no slot / meta children may ever appear.
    for (int i = 0; i < treeAfter.getNumChildren(); ++i)
        CHECK(treeAfter.getChild(i).hasType("PARAM"));

    // Restore path (mirrors setStateInformation): outcome is independent of
    // whatever the slots hold at restore time.
    set("mix_width", 0.4f);
    proc.apvts.replaceState(juce::ValueTree::readFromData(before.getData(), before.getSize()));
    CHECK(get("mix_width") == Approx(1.7f));
    CHECK(get("tap1_feedback") == Approx(fbDefault));
}

//==============================================================================
// User-preset file lifecycle hardening

TEST_CASE_METHOD(Fixture, "rename success leaves exactly one file and updates bookkeeping") {
    REQUIRE(pm->saveUserPreset("OldName", "Utility", "", false));
    auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    REQUIRE(pm->renameUserPreset(users[0], "NewName"));
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.orbitpreset");
    REQUIRE(files.size() == 1);                    // old file gone, no duplicate
    CHECK(files[0].getFileNameWithoutExtension() == "NewName");
    CHECK(pm->currentPresetName() == "NewName");   // current-name follows rename
}

#if JUCE_MAC
TEST_CASE_METHOD(Fixture, "rename rolls back when the old file cannot be deleted") {
    REQUIRE(pm->saveUserPreset("LockedSrc", "Utility", "", false));
    auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    const auto srcPath = users[0].file.getFullPathName();
    // uchg makes the source undeletable (unlink -> EPERM) but still readable.
    REQUIRE(std::system(("chflags uchg \"" + srcPath + "\"").toRawUTF8()) == 0);
    const bool renamed = pm->renameUserPreset(users[0], "Renamed");
    std::system(("chflags nouchg \"" + srcPath + "\"").toRawUTF8());   // allow cleanup
    CHECK_FALSE(renamed);
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.orbitpreset");
    REQUIRE(files.size() == 1);                    // zero net new files (target rolled back)
    CHECK(files[0].getFileNameWithoutExtension() == "LockedSrc");
    CHECK(pm->currentPresetName() == "LockedSrc"); // bookkeeping untouched on failure
}
#endif

TEST_CASE_METHOD(Fixture, "empty save name falls back to 'Preset' consistently") {
    REQUIRE(pm->saveUserPreset("   ", "Utility", "", false));
    CHECK(pm->currentPresetName() == "Preset");
    const auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    CHECK(users[0].name == "Preset");              // meta name matches, not ""
    CHECK(users[0].file.getFileNameWithoutExtension() == "Preset");
    CHECK_FALSE(pm->isModified());                 // save still clears the flag
}

TEST_CASE("tagVocabulary is the fixed six-tag list, in order") {
    const auto& vocab = orbit::PresetManager::tagVocabulary();
    REQUIRE(vocab.size() == 6);
    CHECK(vocab[0] == "Ambient");
    CHECK(vocab[1] == "Rhythm");
    CHECK(vocab[2] == "Dub");
    CHECK(vocab[3] == "Tape");
    CHECK(vocab[4] == "Wide");
    CHECK(vocab[5] == "Utility");
    CHECK(&vocab == &orbit::PresetManager::tagVocabulary());   // stable single instance
}

TEST_CASE_METHOD(Fixture, "meta-less file with NN_ prefix gets a cleaned fallback name") {
    dir.getChildFile("07_Dub_Tape Echo.orbitpreset")
       .replaceWithText("<PARAMS product=\"orbit\" stateVersion=\"1\"/>");
    const auto users = pm->userPresets();
    REQUIRE(users.size() == 1);
    CHECK(users[0].name == "Dub Tape Echo");       // "07_" stripped, de-underscored
}

//==============================================================================
// Factory preset validation (Task 3). Tree shape confirmed from a real
// saveUserPreset dump: <PARAMS> root with <PARAM id=... value=.../> children
// plus a <PresetMeta> child — the brief's PARAM/id naming matches reality.

TEST_CASE_METHOD(Fixture, "factory set: 40 presets, unique names, valid tags, ordered") {
    const auto& f = pm->factoryPresets();
    REQUIRE(f.size() == 40);
    juce::StringArray names;
    const auto& vocab = orbit::PresetManager::tagVocabulary();   // single source of truth
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

TEST_CASE("user preset dir migrates Synthios/Orbit to Synthios/Orbitum once") {
    juce::TemporaryFile scratch;
    const auto root = scratch.getFile().getSiblingFile("orbitum-migration-test");
    root.createDirectory();

    // Legacy layout with a saved preset in it.
    const auto legacy = root.getChildFile("Synthios").getChildFile("Orbit")
                            .getChildFile("Presets");
    legacy.createDirectory();
    legacy.getChildFile("01_Test.orbitpreset").replaceWithText("x");

    const auto dir = orbit::PresetManager::resolveUserPresetDirectory(root);
    CHECK(dir == root.getChildFile("Synthios").getChildFile("Orbitum")
                     .getChildFile("Presets"));
    CHECK(dir.getChildFile("01_Test.orbitpreset").existsAsFile());
    CHECK(!legacy.exists());   // moved, not copied

    // Second resolve is a no-op on the migrated tree.
    const auto again = orbit::PresetManager::resolveUserPresetDirectory(root);
    CHECK(again == dir);
    CHECK(dir.getChildFile("01_Test.orbitpreset").existsAsFile());

    root.deleteRecursively();
}
