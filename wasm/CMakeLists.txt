if(!EMSCRIPTEN)
    message(FATAL_ERROR "This CMakeLists.txt is only for Emscripten")
endif()

add_executable(websynth
    embind.cpp
)

set(compile_and_link_flags
    -fexceptions
    -fwasm-exceptions
)

set_target_properties(websynth
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_compile_options(websynth
    PUBLIC
        ${compile_and_link_flags}
)

target_link_libraries(websynth
    PUBLIC
    ebsynth_core
        embind
        ${compile_and_link_flags}
        -sABORTING_MALLOC=0
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=256mb
        -sSTACK_SIZE=5MB
        -sALLOW_UNIMPLEMENTED_SYSCALLS=1
        -sENVIRONMENT="worker"
        -sERROR_ON_UNDEFINED_SYMBOLS=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=websynth
        -sEXPORTED_FUNCTIONS=_malloc,_free
        -sMODULARIZE=1
        -sNO_EXIT_RUNTIME=1
        -sUSE_ES6_IMPORT_META=1
        -sWASM=1
)
