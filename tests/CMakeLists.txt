FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(orbit_tests
    core/SanityTests.cpp
    core/DelayLineTests.cpp
    core/TempoSyncTests.cpp
    core/DuckerTests.cpp
    core/OnePoleTests.cpp
    core/LfoTests.cpp
    core/CharacterStageTests.cpp
    core/VizFeedTests.cpp
    core/TapFireDetectorTests.cpp
    plugin/OrbitEngineTests.cpp)

# orbit_engine (JUCE-free) provides OrbitEngine.cpp, the plugin/ and core/
# include dirs, and orbit_core transitively.
target_link_libraries(orbit_tests PRIVATE
    orbit_engine
    Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(orbit_tests)

add_subdirectory(preset)
add_subdirectory(gui)
