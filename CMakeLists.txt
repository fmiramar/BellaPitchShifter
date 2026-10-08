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
    src/BelaPhaseVocoder.cpp
)

set(belaphasevocoder_sc_files
    Classes/PitchShiftBella.sc
    Classes/PV_PitchShiftBella.sc
)

set(belaphasevocoder_schelp_files
    HelpSource/Classes/PitchShiftBella.schelp
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
    target_include_directories(BelaPhaseVocoder_scsynth SYSTEM PRIVATE "${SC_PATH}/include/plugin_interface")
endif()

if(TARGET BelaPhaseVocoder_supernova)
    set_target_properties(BelaPhaseVocoder_supernova PROPERTIES OUTPUT_NAME "BelaPhaseVocoder_supernova")
    target_include_directories(BelaPhaseVocoder_supernova SYSTEM PRIVATE "${SC_PATH}/include/plugin_interface")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(bella_pv_state_tests tests/pv_state_tests.cpp)
    target_include_directories(bella_pv_state_tests SYSTEM PRIVATE
        "${SC_PATH}/include/plugin_interface"
        "${SC_PATH}/include/common"
        "${SC_PATH}/common"
    )
    add_test(NAME bella_pv_state COMMAND bella_pv_state_tests)
endif()
