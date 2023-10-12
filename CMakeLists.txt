
cmake_minimum_required(VERSION 3.17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Same-directory build detected. Remove CMakeCache.txt and the CMakeFiles/ directory. Then `mkdir build && cd buil && cmake ..`")
endif()

project(
	"ebsynth"
	VERSION $ENV{EBSYNTH_VERSION}
	LANGUAGES C CXX
)

#
# User options
#

option(EBSYNTH_ENABLE_WASM_TARGETS "Whether to enable WebAssembly targets" OFF)
option(EBSYNTH_ENABLE_TESTING "Whether to enable test modules" OFF)
option(EBSYNTH_ENABLE_BENCHMARKS "Whether to enable benchmark modules" OFF)

#
# Validate options
#

if(EMSCRIPTEN)
    message("Using emscripten--enabling wasm targets!")
    set(EBSYNTH_ENABLE_WASM_TARGETS ON)
else()
    if(EBSYNTH_ENABLE_WASM_TARGETS)
        message(FATAL_ERROR "WebAssembly targets are only supported with emscripten!")
    endif()
endif()

if(EBSYNTH_ENABLE_TESTING)
    message(FATAL_ERROR "Tests are not yet supported!")
endif()

if(EBSYNTH_ENABLE_BENCHMARKS)
    message(FATAL_ERROR "Benchmarks are not yet supported!")
endif()

message(STATUS "***** ebsynth: Configuration *****")
message(STATUS "EBSYNTH_VERSION......................................... ${CMAKE_PROJECT_VERSION}")
message(STATUS "EBSYNTH_ENABLE_WASM_TARGETS............................. ${EBSYNTH_ENABLE_WASM_TARGETS}")
message(STATUS "EBSYNTH_ENABLE_TESTING.................................. ${EBSYNTH_ENABLE_TESTING}")

# Search for dependencies

add_library(ebsynth_core OBJECT
    src/ebsynth_cpu.cpp
    src/ebsynth.cpp
    src/ebsynth_nocuda.cpp
)

set_target_properties(ebsynth_core PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(ebsynth_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(EBSYNTH_ENABLE_WASM_TARGETS)
    add_subdirectory(wasm)
endif()
