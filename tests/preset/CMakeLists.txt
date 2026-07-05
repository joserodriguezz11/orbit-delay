# Headless JUCE test rig: a plain executable hosting the real Orbit parameter
# layout + PresetManager, outside juce_add_plugin. JUCE module targets are
# INTERFACE libraries that compile their sources into the consumer and carry
# JUCE_MODULE_AVAILABLE_* / global-settings definitions, so linking them
# directly works for a console test binary.
add_executable(orbit_preset_tests
    PresetManagerTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Parameters.cpp)

target_compile_definitions(orbit_preset_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(orbit_preset_tests PRIVATE
    orbit_core
    OrbitPresetData
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_recommended_config_flags)

target_include_directories(orbit_preset_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)

catch_discover_tests(orbit_preset_tests)
