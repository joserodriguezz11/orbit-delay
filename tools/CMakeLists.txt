# viz-demo — terminal readout of the visualization feed. No JUCE: links the
# engine exactly like orbit_tests does (orbit_core + OrbitEngine.cpp), minus Catch2.
add_executable(viz-demo
    viz_demo.cpp
    ${CMAKE_SOURCE_DIR}/plugin/OrbitEngine.cpp)

set_target_properties(viz-demo PROPERTIES OUTPUT_NAME viz_demo)

target_link_libraries(viz-demo PRIVATE orbit_core)

target_include_directories(viz-demo PRIVATE
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/plugin)
