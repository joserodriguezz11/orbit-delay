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
    plugin/OrbitEngineTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/OrbitEngine.cpp)

target_link_libraries(orbit_tests PRIVATE
    orbit_core
    Catch2::Catch2WithMain)

target_include_directories(orbit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)

include(Catch)
catch_discover_tests(orbit_tests)
