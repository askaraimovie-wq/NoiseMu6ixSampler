cmake_minimum_required(VERSION 3.15)
project(NoiseMu6ixSampler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NoiseMu6ixSampler
    COMPANY_NAME "Noise Mu6ix Studio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Nm6s
    PLUGIN_CODE Nm6p
    FORMATS VST3 Standalone
    PRODUCT_NAME "Noise Mu6ix Sampler"
)

juce_generate_juce_header(NoiseMu6ixSampler)

target_sources(NoiseMu6ixSampler PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_definitions(NoiseMu6ixSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NoiseMu6ixSampler PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
