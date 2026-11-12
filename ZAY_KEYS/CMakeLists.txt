cmake_minimum_required(VERSION 3.24)

project(ZAYKEYS VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Runtime estatico en MSVC: el usuario final no necesita instalar VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ZAYKEYS
    COMPANY_NAME            "GOLDMINE AUDIO"
    PRODUCT_NAME            "ZAY KEYS"
    BUNDLE_ID               com.goldmineaudio.zaykeys
    PLUGIN_MANUFACTURER_CODE Gmau
    PLUGIN_CODE             Zayk
    FORMATS                 VST3
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

# SoundFont de fabrica embebido en el binario (no depende de rutas en disco)
juce_add_binary_data(ZayKeysData
    HEADER_NAME "BinaryData.h"
    NAMESPACE   BinaryData
    SOURCES     soundfonts/ZayGrand.sf2)
set_target_properties(ZayKeysData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(ZAYKEYS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SoundFontEngine.cpp
    Source/PresetManager.cpp
    Source/CustomLookAndFeel.cpp
    Source/HardwareControls.cpp
    Source/KeyboardComponent.cpp
    Source/LCDDisplay.cpp
    Source/EffectsProcessor.cpp)

target_include_directories(ZAYKEYS PRIVATE Source third_party)

target_compile_definitions(ZAYKEYS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=1
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ZAYKEYS
    PRIVATE
        ZayKeysData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
