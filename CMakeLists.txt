cmake_minimum_required(VERSION 3.15)

project(StepSequencer VERSION 1.0.0)

# Add JUCE as a subdirectory (assuming you have JUCE in a folder called JUCE)
add_subdirectory(ext/juce)
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

#
# Create the plugin
juce_add_plugin(BetterChordStacks
    COMPANY_NAME "YourCompany"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Yoco
    PLUGIN_CODE Bcs1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Better Chord Stacks")
# Add source files
juce_generate_juce_header(BetterChordStacks)

target_sources(BetterChordStacks
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp)


# Link libraries
target_link_libraries(BetterChordStacks
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)