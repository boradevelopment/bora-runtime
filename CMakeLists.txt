cmake_minimum_required(VERSION 3.14)
project(BORA VERSION 0.0.1 LANGUAGES C CXX ASM)

option(ENABLE_ASAN "Enable Address Sanitizer (Debug builds affected only)" OFF)
if(ENABLE_ASAN)
    message(STATUS "Address Sanitizer enabled")
    if(win32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} /fsanitize=address")
    elseif(UNIX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} - fsanitize=address")
    endif ()
endif()

# Global Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckPIESupported)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Shared build setting toggle
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

# Unicode on Windows
if(MSVC)
    add_compile_definitions(UNICODE _UNICODE)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

add_compile_definitions(BORA_UI_SUPPORT BORA_USE_SPIRV WASM_ENABLE_DUMP_CALL_STACK=1)

# WAMR Config options

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_LIB_PTHREAD 0)
set(WAMR_BUILD_MINI_LOADER 0)
set(WAMR_BUILD_SIMD 1)
set(WAMR_BUILD_MEMORY64 1)
set(WAMR_BUILD_DEBUG_INTERP 0) # todo: add more implementations
#set(WAMR_ENABLE_INTERP_FRAME_TS 1)
#set(WAMR_ENABLE_DUMP_CALL_STACK 1)


if(WAMR_BUILD_DEBUG_INTERP)
    set(WAMR_BUILD_FAST_INTERP 0)
    set(WAMR_BUILD_MINI_LOADER 0)
    set(WAMR_BUILD_SIMD 0)
endif()

set(WAMR_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contribs/wasm-micro-runtime")

# ───────────────────────────────────────────────
# ░░ Helper Macro Definitions ░░
# ───────────────────────────────────────────────
function(set_default_option VAR VALUE)
    if(NOT DEFINED ${VAR})
        set(${VAR} ${VALUE} PARENT_SCOPE)
    endif()
endfunction()

function(collect_sources VAR)
    file(GLOB_RECURSE TMP CONFIGURE_DEPENDS ${ARGN})
    set(${VAR} ${TMP} PARENT_SCOPE)
endfunction()

# ───────────────────────────────────────────────
# ░░ Dependencies
# ───────────────────────────────────────────────
find_package(Vulkan REQUIRED)

include_directories(
        3rdparty
        ../global/cpp/contribs
        ../global/cpp/contribs/TAZA/code
)

# source files
collect_sources(BORA_SOURCES
        src/mainDynamicLibrary.cpp
        src/host/*
        src/software/common/*
)
collect_sources(BORA_CONTRIBS
        ../global/cpp/contribs/*.cpp ../global/cpp/contribs/*.hpp ../global/cpp/contribs/*.h
)
collect_sources(BORA_TAZA
        ../global/cpp/contribs/TAZA/code/*.cpp ../global/cpp/contribs/TAZA/code/*.hpp ../global/cpp/contribs/TAZA/code/*.h
)
collect_sources(BORA_3RDPARTY
        ../global/cpp/3rdparty/*
)
#collect_sources(BORA_LIBS
#        ../global/libs/*.a ../global/libs/*.lib ../global/libs/*.dlib
#)

#file(GLOB_RECURSE BORA_SOURCES_WRAPPER src/*.cpp src/*.h src/*.hpp src/*.manifest src/*.rc src/*.cc)
#file(GLOB_RECURSE HOST_FILES src/host/*.cpp src/host/*.h src/host/*.hpp src/host/*.cc src/software/common/nGraphics/* src/software/common/nWindow/* src/software/common/nGraphics/* src/software/win32/nGraphics/* src/software/common/nGUI/*)
file(GLOB_RECURSE BORA_SOURCES_WRAPPER src/wrapper/*)
file(GLOB_RECURSE BORA_SOURCES_CONTRIBWRAPPER ../global/cpp/contribs/*.cpp  ../global/cpp/contribs/*.h  ../global/cpp/contribs/*.hpp)
file(GLOB_RECURSE GRAPH_FILES ../global/cpp/contribs/nGraphics/*)
list(REMOVE_ITEM BORA_SOURCES_CONTRIBWRAPPER ${GRAPH_FILES})
list(REMOVE_ITEM BORA_SOURCES ${BORA_SOURCES_WRAPPER})

if (NOT DEFINED WAMR_BUILD_MEMORY64)
    set (WAMR_BUILD_MEMORY64 1)
endif()

set(SKIA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../global/contribs/bskia")
set(SKIA_BUILD_DIR "${CMAKE_SOURCE_DIR}/../global/cached_libs/bskia")
include(${CMAKE_SOURCE_DIR}/../global/cmake/skia.cmake)
# Platform configs
if(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/platform/windows.cmake)
elseif(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/platform/macos.cmake)
elseif(UNIX)
    include(${CMAKE_SOURCE_DIR}/cmake/platform/linux.cmake)
else()
    message(FATAL_ERROR "Unsupported platform!")
endif()
