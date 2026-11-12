cmake_minimum_required(VERSION 3.22)
project(MIKEGORDON VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# ---- Pure C++ DSP engine tests + demo renderer (no JUCE)
add_executable(mg_engine_tests tests/test_engine.cpp)
target_include_directories(mg_engine_tests PRIVATE src/engine tests)
add_test(NAME engine_every_control_textbook_math COMMAND mg_engine_tests)
add_executable(mg_render_demos tests/render_demos.cpp)
target_include_directories(mg_render_demos PRIVATE src/engine tests)

option(MG_ENGINE_ONLY "Build dependency-free engine tests only" OFF)
if(MG_ENGINE_ONLY)
  return()
endif()

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MikeGordon
  VERSION 1.0.0
  COMPANY_NAME koobacorp
  BUNDLE_ID com.koobacorp.mikegordon
  PLUGIN_MANUFACTURER_CODE Koob
  PLUGIN_CODE MkGd
  FORMATS VST3
  PRODUCT_NAME "MIKE GORDON"
  DESCRIPTION "MIKE GORDON vibe and character box"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Modulation)
target_sources(MikeGordon PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
target_include_directories(MikeGordon PUBLIC src/engine src/plugin)
target_compile_definitions(MikeGordon PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0 JUCE_USE_CAMERA=0 JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(MikeGordon PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_gui_basics
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
target_compile_options(MikeGordon PUBLIC $<$<CONFIG:Release>:-O3>)
set_target_properties(MikeGordon PROPERTIES VISIBILITY_INLINES_HIDDEN ON CXX_VISIBILITY_PRESET hidden)
if(APPLE)
  target_link_options(MikeGordon PUBLIC -Wl,-dead_strip)
endif()

option(MG_BUILD_TOOLS "Build JUCE plugin tests and UI screenshots" ON)
if(MG_BUILD_TOOLS)
  juce_add_console_app(mg_plugin_tests PRODUCT_NAME mg_plugin_tests)
  target_sources(mg_plugin_tests PRIVATE tests/plugin_tests.cpp)
  target_include_directories(mg_plugin_tests PRIVATE src/engine src/plugin tests $<TARGET_PROPERTY:MikeGordon,INCLUDE_DIRECTORIES>)
  target_compile_definitions(mg_plugin_tests PRIVATE $<TARGET_PROPERTY:MikeGordon,COMPILE_DEFINITIONS>)
  target_link_libraries(mg_plugin_tests PRIVATE MikeGordon juce::juce_recommended_config_flags)
  add_test(NAME plugin_params_buses_state_presets_editor COMMAND mg_plugin_tests)
endif()
