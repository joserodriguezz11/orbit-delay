add_library(orbit_core STATIC
    dsp/DelayLine.cpp
    dsp/TempoSync.cpp
    dsp/Ducker.cpp)
target_include_directories(orbit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(orbit_core PUBLIC cxx_std_17)
