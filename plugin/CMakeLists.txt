juce_add_plugin(OrbitDelay
    COMPANY_NAME "${ORBIT_COMPANY_NAME}"
    BUNDLE_ID "${ORBIT_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE "${ORBIT_MANUFACTURER_CODE}"
    PLUGIN_CODE "${ORBIT_PLUGIN_CODE}"
    PRODUCT_NAME "${ORBIT_PRODUCT_NAME}"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(OrbitDelay PRIVATE
    PluginProcessor.cpp)

target_compile_definitions(OrbitDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OrbitDelay
    PRIVATE
        orbit_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
