add_library(orbit_core STATIC
    dsp/DelayLine.cpp
    dsp/TempoSync.cpp
    dsp/Ducker.cpp
    dsp/OnePole.cpp
    dsp/Lfo.cpp
    dsp/CharacterStage.cpp)
target_include_directories(orbit_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(orbit_core PUBLIC cxx_std_17)
