cmake_minimum_required(VERSION 3.22)
project(GhoulboxVST3 VERSION 0.1.0 LANGUAGES C CXX)

# cmake -B build -DGHOULBOX_ROOT=<path to the ghoulbox repo, after ./build.sh>
set(GHOULBOX_ROOT "" CACHE PATH "ghoulbox-fm1-dungeon-synth checkout")
if(NOT GHOULBOX_ROOT)
  message(FATAL_ERROR "Set -DGHOULBOX_ROOT=<path to the Ghoulbox repo>")
endif()
if(NOT EXISTS "${GHOULBOX_ROOT}/build/gen/felucca_tables.h")
  message(FATAL_ERROR "Run the repo's ./build.sh once first (it creates build/gen).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

# Same version define web/emu/build.sh derives from firmware/src/felucca.c
file(STRINGS "${GHOULBOX_ROOT}/firmware/src/felucca.c" _ver REGEX "^#define FELUCCA_VERSION")
string(REGEX REPLACE ".*\"([^\"]*)\".*" "\\1" GB_FELUCCA_VERSION "${_ver}")
if(NOT GB_FELUCCA_VERSION)
  set(GB_FELUCCA_VERSION "WEB")
endif()

# ---- the firmware: ONE translation unit = web/emu/felucca_web.c, included by gb_host.c.
# Built as a SHARED library: every plugin instance loads its own copy (see Source/GbFirmware.h),
# so instances don't share the firmware's globals.
add_library(ghoulbox_fw SHARED Source/gb_host.c)
target_include_directories(ghoulbox_fw PUBLIC
  Source
  ${GHOULBOX_ROOT}/web/emu          # so gb_host.c can #include "felucca_web.c"
  ${GHOULBOX_ROOT}/build/gen
  ${GHOULBOX_ROOT}/firmware/src)
target_compile_definitions(ghoulbox_fw PRIVATE "FELUCCA_VERSION=\"${GB_FELUCCA_VERSION}\"" GBX_BUILD_DLL=1)
if(MSVC)
  # Needs clang-cl (GNU C extensions):
  #   cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ...
  target_compile_options(ghoulbox_fw PRIVATE /clang:-std=gnu99 /clang:-ffp-contract=off /w /O2)
else()
  target_compile_options(ghoulbox_fw PRIVATE -std=gnu99 -ffp-contract=off -w -O2)
  target_link_libraries(ghoulbox_fw PUBLIC m)
endif()
set_target_properties(ghoulbox_fw PROPERTIES POSITION_INDEPENDENT_CODE ON PREFIX "" OUTPUT_NAME ghoulbox_fw)

# ---- the plugin
juce_add_plugin(Ghoulbox
  PRODUCT_NAME "Ghoulbox"
  COMPANY_NAME "Ghoulbox"
  PLUGIN_MANUFACTURER_CODE Gblx
  PLUGIN_CODE Gb01
  FORMATS VST3
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(Ghoulbox PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/GbFirmware.cpp)
target_compile_definitions(Ghoulbox PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Ghoulbox
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# The plugin does not link the firmware; it loads it at run time. Build it first and put it inside the
# bundle next to the plugin binary (before the link step, so JUCE's copy-to-VST3-folder includes it).
add_dependencies(Ghoulbox ghoulbox_fw)
if(TARGET Ghoulbox_VST3)
  add_dependencies(Ghoulbox_VST3 ghoulbox_fw)
  add_custom_command(TARGET Ghoulbox_VST3 PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ghoulbox_VST3>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ghoulbox_fw>" "$<TARGET_FILE_DIR:Ghoulbox_VST3>")
endif()
