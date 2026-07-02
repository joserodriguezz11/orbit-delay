cmake_minimum_required(VERSION 3.22)
include(cmake/Branding.cmake)
project(OrbitDelay VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(core)
add_subdirectory(plugin)

enable_testing()
add_subdirectory(tests)
