#include "PresetManager.h"
#include "BinaryData.h"

#include <algorithm>
#include <vector>

namespace orbit {

namespace {
constexpr auto kPresetExtension = ".orbitpreset";
constexpr auto kMetaNodeName    = "PresetMeta";

// Raw text of an embedded resource by namedResourceList index ("" if invalid).
juce::String factoryResourceText(int index) {
    if (index < 0 || index >= BinaryData::namedResourceListSize)
        return {};
    int size = 0;
    const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[index], size);
    return data == nullptr ? juce::String() : juce::String::fromUTF8(data, size);
}

PresetInfo makeUserInfo(const juce::File& file) {
    PresetInfo info;
    info.isFactory = false;
    info.file = file;
    info.name = file.getFileNameWithoutExtension();
    if (const auto xml = juce::parseXML(file)) {
        if (const auto* meta = xml->getChildByName(kMetaNodeName)) {
            info.name        = meta->getStringAttribute("name", info.name);
            info.tags        = meta->getStringAttribute("tags");
            info.author      = meta->getStringAttribute("author");
            info.description = meta->getStringAttribute("description");
        }
    }
    return info;
}

bool tagListContains(const juce::String& tags, const juce::String& tag) {
    auto tokens = juce::StringArray::fromTokens(tags, ",", {});
    tokens.trim();
    return tokens.contains(tag);   // case-sensitive by design
}
} // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             const juce::File& userDirOverride)
    : apvts_(apvts), userDirOverride_(userDirOverride) {
    for (auto* p : apvts_.processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p)) {
            listenedParamIds_.add(withId->paramID);
            apvts_.addParameterListener(withId->paramID, this);
        }

    // Spec §3: fresh instance — both A/B slots hold the construction state,
    // slot A live. copyState() returns independent deep copies.
    slotA_ = apvts_.copyState();
    slotB_ = apvts_.copyState();

    // Factory enumeration: every embedded *.orbitpreset resource, sorted by
    // original filename (the NN_ prefix gives the stable release order).
    struct Entry { juce::String originalFilename; PresetInfo info; };
    std::vector<Entry> entries;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        const juce::String original =
            BinaryData::getNamedResourceOriginalFilename(BinaryData::namedResourceList[i]);
        if (!original.endsWith(kPresetExtension))
            continue;
        PresetInfo info;
        info.isFactory = true;
        info.binaryDataIndex = i;
        info.name = original.dropLastCharacters(
            static_cast<int>(juce::String(kPresetExtension).length()));
        if (const auto xml = juce::parseXML(factoryResourceText(i)))
            if (const auto* meta = xml->getChildByName(kMetaNodeName)) {
                info.name        = meta->getStringAttribute("name", info.name);
                info.tags        = meta->getStringAttribute("tags");
                info.author      = meta->getStringAttribute("author");
                info.description = meta->getStringAttribute("description");
            }
        entries.push_back({ original, std::move(info) });
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                  return a.originalFilename.compareNatural(b.originalFilename) < 0;
              });
    for (auto& e : entries)
        factoryPresets_.add(std::move(e.info));
}

PresetManager::~PresetManager() {
    for (const auto& id : listenedParamIds_)
        apvts_.removeParameterListener(id, this);
}

//==============================================================================
// Enumeration

const juce::Array<PresetInfo>& PresetManager::factoryPresets() const {
    return factoryPresets_;   // built once at construction from BinaryData
}

juce::Array<PresetInfo> PresetManager::userPresets() const {
    juce::Array<PresetInfo> out;
    for (const auto& f : presetDir().findChildFiles(
             juce::File::findFiles, false, juce::String("*") + kPresetExtension))
        out.add(makeUserInfo(f));
    return out;
}

juce::Array<PresetInfo> PresetManager::filterByTag(const juce::String& tag) const {
    juce::Array<PresetInfo> out;
    for (const auto& info : factoryPresets_)
        if (tagListContains(info.tags, tag))
            out.add(info);
    for (const auto& info : userPresets())
        if (tagListContains(info.tags, tag))
            out.add(info);
    return out;
}

//==============================================================================
// Application

bool PresetManager::loadPreset(const PresetInfo& info) {
    if (info.isFactory) {
        const auto xml = factoryResourceText(info.binaryDataIndex);
        return xml.isNotEmpty() && applyPresetXml(xml, info.name);
    }
    if (!info.file.existsAsFile())
        return false;
    return applyPresetXml(info.file.loadFileAsString(), info.name);
}

juce::String PresetManager::presetXmlFor(const PresetInfo& info) const {
    if (info.isFactory)
        return factoryResourceText(info.binaryDataIndex);
    return info.file.existsAsFile() ? info.file.loadFileAsString() : juce::String();
}

juce::String PresetManager::currentPresetName() const { return currentPresetName_; }

bool PresetManager::isModified() const { return modified_; }

//==============================================================================
// User presets

bool PresetManager::saveUserPreset(const juce::String& name, const juce::String& tags,
                                   const juce::String& description, bool overwrite) {
    const auto dir = presetDir();
    dir.createDirectory();   // created on demand
    const auto file = dir.getChildFile(sanitizeName(name) + kPresetExtension);
    if (file.existsAsFile() && !overwrite)
        return false;

    const auto state = stateWithMeta(name, tags, description);
    const auto xml = state.createXml();
    if (xml == nullptr || !file.replaceWithText(xml->toString(juce::XmlElement::TextFormat())))
        return false;

    currentPresetName_ = name;
    modified_ = false;   // save clears the dirty flag
    return true;
}

bool PresetManager::deleteUserPreset(const PresetInfo& info) {
    if (info.isFactory || !info.file.existsAsFile())
        return false;
    return info.file.deleteFile();
}

bool PresetManager::renameUserPreset(const PresetInfo& info, const juce::String& newName) {
    if (info.isFactory || !info.file.existsAsFile())
        return false;
    const auto target = presetDir().getChildFile(sanitizeName(newName) + kPresetExtension);
    if (target.existsAsFile())
        return false;   // never clobber an existing preset via rename

    auto xml = juce::parseXML(info.file);
    if (xml == nullptr)
        return false;
    if (auto* meta = xml->getChildByName(kMetaNodeName))
        meta->setAttribute("name", newName);
    else
        xml->createNewChildElement(kMetaNodeName)->setAttribute("name", newName);

    if (!target.replaceWithText(xml->toString(juce::XmlElement::TextFormat())))
        return false;
    info.file.deleteFile();
    if (currentPresetName_ == info.name)
        currentPresetName_ = newName;
    return true;
}

//==============================================================================
// A/B compare (spec §3). Slots hold parameter-only trees — copyState() never
// contains PresetMeta (that node exists only inside saved preset files).

void PresetManager::toggleAB() {
    // Capture live state into the slot being left, so edits are never lost.
    (slotBActive_ ? slotB_ : slotA_) = apvts_.copyState();
    slotBActive_ = !slotBActive_;

    // Apply the other slot via the same suppress-dirty replaceState path the
    // load path uses. createCopy() keeps the slot independent of the live tree.
    suppressDirty_ = true;
    apvts_.replaceState((slotBActive_ ? slotB_ : slotA_).createCopy());
    suppressDirty_ = false;

    modified_ = true;   // the applied slot's state counts as an edit (spec §3)
    // currentPresetName_ intentionally unchanged.
}

void PresetManager::copyAB() {
    // inactive := live; live state and modified flag untouched.
    (slotBActive_ ? slotA_ : slotB_) = apvts_.copyState();
}

bool PresetManager::isSlotB() const { return slotBActive_; }

//==============================================================================

juce::File PresetManager::userPresetDirectory() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Synthios")
                   .getChildFile("Orbit")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

//==============================================================================
// Private

void PresetManager::parameterChanged(const juce::String&, float) {
    if (!suppressDirty_)
        modified_ = true;
}

juce::ValueTree PresetManager::stateWithMeta(const juce::String& name,
                                             const juce::String& tags,
                                             const juce::String& description) const {
    // Same versioned envelope as OrbitAudioProcessor::getStateInformation
    // (family-wide product/stateVersion rules), plus a PresetMeta child.
    auto state = apvts_.copyState();
    state.setProperty("product", "orbit", nullptr);
    state.setProperty("stateVersion", 1, nullptr);

    juce::ValueTree meta(kMetaNodeName);
    meta.setProperty("name", name, nullptr);
    meta.setProperty("tags", tags, nullptr);
    meta.setProperty("description", description, nullptr);
    state.appendChild(meta, nullptr);
    return state;
}

bool PresetManager::applyPresetXml(const juce::String& xml, const juce::String& presetName) {
    // Mirrors OrbitAudioProcessor::setStateInformation semantics, plus
    // PresetMeta stripping (the APVTS tree must stay parameter-only).
    const auto element = juce::parseXML(xml);
    if (element == nullptr)
        return false;
    auto tree = juce::ValueTree::fromXml(*element);
    if (!tree.isValid())
        return false;
    if (tree.hasProperty("product") && tree.getProperty("product").toString() != "orbit")
        return false;   // refuse presets from a different family product
    if (!tree.hasType(apvts_.state.getType()))
        return false;   // not an Orbit parameter tree
    // Migration switch point: v1 is current. When stateVersion 2 exists,
    // transform older trees here before replaceState.
    const int loadedVersion = static_cast<int>(tree.getProperty("stateVersion", 1));
    juce::ignoreUnused(loadedVersion);

    tree.removeChild(tree.getChildWithName(kMetaNodeName), nullptr);

    suppressDirty_ = true;
    apvts_.replaceState(tree);
    suppressDirty_ = false;

    currentPresetName_ = presetName;
    modified_ = false;   // load clears the dirty flag
    return true;
}

juce::File PresetManager::presetDir() const {
    return userDirOverride_ == juce::File() ? userPresetDirectory() : userDirOverride_;
}

juce::String PresetManager::sanitizeName(const juce::String& name) {
    juce::String out;
    for (int i = 0; i < name.length(); ++i) {
        const auto c = name[i];
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(c)
                       || c == ' ' || c == '-' || c == '_';
        out << juce::String::charToString(keep ? c : juce::juce_wchar('_'));
    }
    out = out.trim();
    return out.isEmpty() ? juce::String("Preset") : out;
}

} // namespace orbit
