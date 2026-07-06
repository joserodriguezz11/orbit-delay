# viz-demo — terminal readout of the visualization feed. No JUCE: links the
# engine exactly like orbit_tests does (orbit_engine, JUCE-free), minus Catch2.
# orbit_engine supplies the plugin/ and core/ include dirs and orbit_core.
add_executable(viz-demo viz_demo.cpp)

set_target_properties(viz-demo PROPERTIES OUTPUT_NAME viz_demo)

target_link_libraries(viz-demo PRIVATE orbit_engine)
