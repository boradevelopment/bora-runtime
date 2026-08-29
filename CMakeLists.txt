cmake_minimum_required(VERSION 3.14)
project(BORA VERSION 0.6.1 LANGUAGES C CXX)
include(FindPkgConfig)
macro(bora_check_module TARGET_NAME MODULE_PKG DEF_NAME FRIENDLY_NAME)
    pkg_check_modules(${DEF_NAME}_PKG QUIET ${MODULE_PKG})
    if(${DEF_NAME}_PKG_FOUND)
        target_compile_definitions(${TARGET_NAME} PRIVATE "${DEF_NAME}=1")
        target_include_directories(${TARGET_NAME} PRIVATE ${${DEF_NAME}_PKG_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${${DEF_NAME}_PKG_LIBRARIES})
        message(STATUS "BORA: ${FRIENDLY_NAME} found")
        set(${DEF_NAME}_FOUND TRUE)
    else()
        set(${DEF_NAME}_FOUND FALSE)
    endif()
endmacro()

set(BORA_WRAPPER_VERSION_MAJOR 1)
set(BORA_WRAPPER_VERSION_MINOR 0)
set(BORA_WRAPPER_VERSION_PATCH 0)

option(ENABLE_ASAN "Enable Address Sanitizer (Debug builds affected only)" OFF)
option(DEBUGGER "Is this a debugger varient that can debug BORA Applications?" OFF)
if(DEBUGGER)
    message(STATUS "Debugging Interpretation Enabled")
endif()
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

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE Debug)
else()
    set(BUILD_TYPE Release)
endif()


if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
    set(ARCH "x86")
else()
    set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME_LOWER)

set(SKIA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../global/contribs/bskia")
set(WASMTIME_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../global/contribs/bora-wasmtime")
set(SKIA_BUILD_DIR "${CMAKE_SOURCE_DIR}/../global/cached_libs/${SYSTEM_NAME_LOWER}/${ARCH}/bskia")
set(WASMTIME_BUILD_DIR "${CMAKE_SOURCE_DIR}/../global/cached_libs/${SYSTEM_NAME_LOWER}/${ARCH}/bora-wasmtime")

include(${CMAKE_SOURCE_DIR}/../global/cmake/skia.cmake)
include(${CMAKE_SOURCE_DIR}/../global/cmake/wasmtime.cmake)

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
