# Headless JUCE GUI test rig: a plain executable hosting the design-token
# header (plugin/gui/OrbitTheme.h) + the REAL OrbitAudioProcessor, outside
# juce_add_plugin. Mirrors tests/preset — JUCE module targets are INTERFACE
# libraries that compile their sources into the consumer, so linking them
# directly works for a console test binary.
#
# The real processor is compiled in (not just Parameters/PresetManager) so
# processor-level GUI seams — vizFeed(), createEditor() in later waves — are
# testable here. That pulls in the plugin's full link set: orbit_engine,
# OrbitPresetData (factory presets for PresetManager), juce_audio_utils
# (GenericAudioProcessorEditor in PluginProcessor.h).
add_executable(orbit_gui_tests
    OrbitThemeTests.cpp
    OrbitFontsTests.cpp
    OrbitControlsTests.cpp
    ProcessorAccessorTests.cpp
    OrbitEditorTests.cpp
    OrbitLookAndFeelTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gui/OrbitControls.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gui/OrbitEditor.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gui/OrbitFonts.cpp
    ${CMAKE_SOURCE_DIR}/plugin/gui/OrbitLookAndFeel.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Parameters.cpp
    ${CMAKE_SOURCE_DIR}/plugin/PresetManager.cpp)

target_compile_definitions(orbit_gui_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # PluginProcessor.h::getName() uses JucePlugin_Name, which only exists
    # inside juce_add_plugin targets — provide it for the console rig.
    JucePlugin_Name="Orbit")

target_link_libraries(orbit_gui_tests PRIVATE
    orbit_core
    orbit_engine
    OrbitPresetData
    OrbitFontData
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

target_include_directories(orbit_gui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)

catch_discover_tests(orbit_gui_tests)
