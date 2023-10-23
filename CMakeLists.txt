cmake_minimum_required(VERSION 3.27)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Same-directory build detected. Remove CMakeCache.txt and the CMakeFiles/ directory. Then `mkdir build && cd buil && cmake ..`")
endif()

set(EBSYNTH_VERSION 0.0.0)
if($ENV{EBSYNTH_VERSION})
    set(EBSYNTH_VERSION $ENV{EBSYNTH_VERSION})
endif()

project(
	"ebsynth"
	VERSION ${EBSYNTH_VERSION}
	LANGUAGES C CXX
)

#
# Check dependencies
#

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(OpenMP)
set(EBSYNTH_ENABLE_OPENMP OFF CACHE BOOL "Whether to enable OpenMP")
if(OPENMP_FOUND)
    set(EBSYNTH_ENABLE_OPENMP ON)
endif()

include(CheckLanguage)
check_language(CUDA)
set(EBSYNTH_ENABLE_CUDA OFF CACHE BOOL "Whether to enable CUDA")
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA OPTIONAL)
    set(EBSYNTH_ENABLE_CUDA ON)
endif()

find_package(Halide)
set(EBSYNTH_ENABLE_HALIDE OFF CACHE BOOL "Whether to enable Halide")
if(Halide_FOUND)
    set(EBSYNTH_ENABLE_HALIDE ON)
endif()

set(EBSYNTH_ENABLE_WASM_TARGETS OFF CACHE BOOL "Whether to enable WebAssembly targets")
if(EMSCRIPTEN)
    set(EBSYNTH_ENABLE_WASM_TARGETS ON)
endif()

#
# User options
#

option(EBSYNTH_ENABLE_TESTING "Whether to enable test modules" OFF)
option(EBSYNTH_ENABLE_BENCHMARKS "Whether to enable benchmark modules" OFF)

#
# Validate options
#

if(EBSYNTH_ENABLE_WASM_TARGETS AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "WebAssembly targets are only supported with emscripten!")
endif()

if(EBSYNTH_ENABLE_TESTING)
    message(WARNING "Tests are not yet supported!")
endif()

if(EBSYNTH_ENABLE_BENCHMARKS)
    message(WARNING "Benchmarks are not yet supported!")
endif()

message(STATUS "***** ebsynth: Configuration *****")
message(STATUS "EBSYNTH_VERSION......................................... ${CMAKE_PROJECT_VERSION}")
message(STATUS "EBSYNTH_ENABLE_CUDA..................................... ${EBSYNTH_ENABLE_CUDA}")
message(STATUS "EBSYNTH_ENABLE_HALIDE................................... ${EBSYNTH_ENABLE_HALIDE}")
message(STATUS "EBSYNTH_ENABLE_OPENMP................................... ${EBSYNTH_ENABLE_OPENMP}")
message(STATUS "EBSYNTH_ENABLE_WASM_TARGETS............................. ${EBSYNTH_ENABLE_WASM_TARGETS}")
message(STATUS "EBSYNTH_ENABLE_TESTING.................................. ${EBSYNTH_ENABLE_TESTING}")

#
# Generate our targets
#

# Core library

add_library(ebsynth_core OBJECT
    src/ebsynth_cpu.cpp
    src/ebsynth.cpp
)

if(EBSYNTH_ENABLE_CUDA)
    target_sources(ebsynth_core
        PRIVATE
            src/ebsynth_cuda.cu
    )
else()
    target_sources(ebsynth_core
        PRIVATE
            src/ebsynth_nocuda.cpp
    )
endif()

target_compile_definitions(ebsynth_core
    PUBLIC
        EBSYNTH_ENABLE_CUDA=$<BOOL:${EBSYNTH_ENABLE_CUDA}>
        EBSYNTH_ENABLE_OPENMP=$<BOOL:${EBSYNTH_ENABLE_OPENMP}>
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

target_compile_options(ebsynth_core
    PUBLIC
        ${OpenMP_CXX_FLAGS}
)

target_link_options(ebsynth_core
    PUBLIC
        ${OpenMP_CXX_FLAGS}
)

# Ebsynth CLI program

add_executable(ebsynth
    src/main.cpp
)

target_link_libraries(ebsynth
    PRIVATE
        ebsynth_core
)

add_custom_command(TARGET ebsynth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ebsynth> ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# Ebsynth Wasm module

if(EBSYNTH_ENABLE_WASM_TARGETS)
    add_subdirectory(wasm)
endif()
