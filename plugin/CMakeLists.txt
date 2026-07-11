# orbit_engine — the JUCE-free engine core, compiled once and linked by every
# consumer (OrbitDelay plugin, orbit_tests, viz-demo) instead of each target
# recompiling OrbitEngine.cpp. CONTRACT: this target must never link or include
# JUCE — orbit_tests and viz-demo stay JUCE-free through it.
add_library(orbit_engine STATIC OrbitEngine.cpp)
target_include_directories(orbit_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core)
target_link_libraries(orbit_engine PUBLIC orbit_core)

# Factory presets embedded as binary data. Enumeration in PresetManager uses
# BinaryData::namedResourceList + getNamedResourceOriginalFilename, so the
# mangled symbol names never need to be spelled out.
file(GLOB ORBIT_PRESET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/presets/*.orbitpreset")
list(SORT ORBIT_PRESET_SOURCES)
list(LENGTH ORBIT_PRESET_SOURCES ORBIT_PRESET_COUNT)
if(NOT ORBIT_PRESET_COUNT EQUAL 40)
    message(FATAL_ERROR "Expected 40 factory presets, found ${ORBIT_PRESET_COUNT}")
endif()
juce_add_binary_data(OrbitPresetData SOURCES ${ORBIT_PRESET_SOURCES})

# Design fonts embedded as binary data (OFL-licensed: Archivo, IBM Plex Mono,
# Syne). Own header/namespace so the symbols never collide with the preset
# BinaryData target.
file(GLOB ORBIT_FONT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/fonts/*.ttf")
list(SORT ORBIT_FONT_SOURCES)
juce_add_binary_data(OrbitFontData
    HEADER_NAME OrbitFontBinary.h
    NAMESPACE OrbitFontBinary
    SOURCES ${ORBIT_FONT_SOURCES})

juce_add_plugin(OrbitDelay
    COMPANY_NAME "${ORBIT_COMPANY_NAME}"
    BUNDLE_ID "${ORBIT_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE "${ORBIT_MANUFACTURER_CODE}"
    PLUGIN_CODE "${ORBIT_PLUGIN_CODE}"
    PRODUCT_NAME "${ORBIT_PRODUCT_NAME}"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(OrbitDelay PRIVATE
    PluginProcessor.cpp
    Parameters.cpp
    PresetManager.cpp
    gui/OrbPad.cpp
    gui/OrbitControls.cpp
    gui/OrbitRail.cpp
    gui/OrbitTapStrip.cpp
    gui/OrbitEditor.cpp
    gui/OrbitFonts.cpp
    gui/OrbitLookAndFeel.cpp)

target_compile_definitions(OrbitDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OrbitDelay
    PRIVATE
        orbit_core
        orbit_engine
        OrbitPresetData
        OrbitFontData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
