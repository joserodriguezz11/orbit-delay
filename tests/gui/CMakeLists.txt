# Headless JUCE GUI test rig: a plain executable hosting the design-token
# header (plugin/gui/OrbitTheme.h) + the real Orbit parameter layout, outside
# juce_add_plugin. Mirrors tests/preset — JUCE module targets are INTERFACE
# libraries that compile their sources into the consumer, so linking them
# directly works for a console test binary.
add_executable(orbit_gui_tests
    OrbitThemeTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Parameters.cpp)

target_compile_definitions(orbit_gui_tests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(orbit_gui_tests PRIVATE
    orbit_core
    Catch2::Catch2WithMain
    juce::juce_audio_processors
    juce::juce_recommended_config_flags)

target_include_directories(orbit_gui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)

catch_discover_tests(orbit_gui_tests)
