cmake_minimum_required(VERSION 3.12)

project(BelaPhaseVocoderSC LANGUAGES CXX)

option(SCSYNTH "Build the scsynth plugin target" ON)
option(SUPERNOVA "Build the supernova plugin target" OFF)
option(STRICT "Enable strict compiler warnings" OFF)
option(NATIVE "Enable -march=native where supported" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/stage" CACHE PATH "Install prefix" FORCE)
endif()

if(NOT SC_PATH)
    set(SC_PATH "../../chow dsp to sc codex/supercollider-3.14.1")
endif()

get_filename_component(SC_PATH_FOR_MODULES "${SC_PATH}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH "${SC_PATH_FOR_MODULES}/tools/cmake_gen")

include(SuperColliderServerPlugin)
sc_do_initial_compiler_config()
sc_check_sc_path("${SC_PATH}")

set(belaphasevocoder_cpp_files
    src/PV_PitchShiftBella.cpp
)

set(belaphasevocoder_sc_files
    Classes/PV_PitchShiftBella.sc
)

set(belaphasevocoder_schelp_files
    HelpSource/Classes/PV_PitchShiftBella.schelp
)

sc_add_server_plugin(
    "BelaPhaseVocoder"
    "BelaPhaseVocoder"
    "${belaphasevocoder_cpp_files}"
    "${belaphasevocoder_sc_files}"
    "${belaphasevocoder_schelp_files}"
)

if(TARGET BelaPhaseVocoder_scsynth)
    set_target_properties(BelaPhaseVocoder_scsynth PROPERTIES OUTPUT_NAME "BelaPhaseVocoder")
endif()

if(TARGET BelaPhaseVocoder_supernova)
    set_target_properties(BelaPhaseVocoder_supernova PROPERTIES OUTPUT_NAME "BelaPhaseVocoder_supernova")
endif()

