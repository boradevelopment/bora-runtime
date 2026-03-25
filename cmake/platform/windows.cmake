collect_sources(BORA_SOURCES_PLATFORM
        src/host/*
        src/software/win32/*
)

set(WAMR_BUILD_PLATFORM "windows")
include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

if(NOT DEFINED WAMR_BUILD_TARGET)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WAMR_BUILD_TARGET "X86_64")
    else()
        set(WAMR_BUILD_TARGET "X86_32")
    endif()
endif()

include(${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common ./src/host/templates ../global/cpp ../global/contribs ./src ./src/software/common ./src/software/win32 ./contribs ./3rdparty)

add_compile_definitions(
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
        NOMINMAX
        _WINSOCKAPI_
        GLEW_STATIC
        SPIRV_SKIA_CONFLICT_HACK
)
if(ENABLE_ASAN)
add_link_options("/INFERASANLIBS")
endif()


# Executable
add_executable(BORA
        #${BORA_SOURCES_WRAPPER}
        src/wrapper/mainWrapper.cpp
        src/software/win32/winmeta/bora.rc
        src/software/win32/winmeta/DPI.manifest
#        src/software/win32/rcscle.cc
#        ../global/cpp/contribs/SysImageMgr.cpp ${BORA_TAZA} ${BORA_3RDPARTY}
)

target_compile_definitions(BORA PRIVATE
        TAZABASEDIR="${CMAKE_SOURCE_DIR}/contribs/TAZA/code"
        NOMINMAX _WINSOCKAPI_ WRAPPER SK_DIRECT3D
)
target_compile_options(BORA PRIVATE /FI${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.lib)
else()
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Release/*.lib)
endif()

add_library(BORA_COMPAT SHARED src/library.def ${BORA_SOURCES_PLATFORM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})
target_link_libraries(BORA_COMPAT PRIVATE Vulkan::Vulkan ntdll ${BORA_SKIA_LIBRARY} ${BORA_LIBS} opengl32.lib)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_dependencies(BORA_COMPAT skia_debug)
else()
    add_dependencies(BORA_COMPAT skia_release)
endif()

set_target_properties(BORA_COMPAT PROPERTIES
        OUTPUT_NAME "LATEST"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/compatibility"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/compatibility"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/compatibility"
)
set_target_properties(BORA PROPERTIES
        OUTPUT_NAME "bora"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}"
)

add_dependencies(BORA BORA_COMPAT)
