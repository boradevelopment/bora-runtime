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
include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common ./src/host/templates ../global/cpp ./src ./src/software/common ./src/software/win32 ./contribs ./3rdparty)

add_compile_definitions(
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
        NOMINMAX
        _WINSOCKAPI_
        GLEW_STATIC
)
if(ENABLE_ASAN)
add_link_options("/INFERASANLIBS")
endif()

# Executable
add_executable(BORA
        ${BORA_SOURCES_WRAPPER}
        ${BORA_SOURCES_CONTRIBWRAPPER} ${BORA_TAZA} ${BORA_3RDPARTY}
)
target_compile_definitions(BORA PRIVATE
        TAZABASEDIR="${CMAKE_SOURCE_DIR}/contribs/TAZA/code"
        NOMINMAX _WINSOCKAPI_ WRAPPER
)
target_compile_options(BORA PRIVATE /FI${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)


file(GLOB_RECURSE BORA_SOURCES_CONTRIB_GRAPHICS contribs/bskia/src/* 3rdparty/GLEW/**/*.h)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE Debug)
else()
    set(BUILD_TYPE Release)
endif()

message(${BUILD_TYPE})
file(GLOB_RECURSE BORA_SKIA ${CMAKE_SOURCE_DIR}/cached_libs/bskia/${BUILD_TYPE}${ASAN_PREFIX}/*.lib)
file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.lib)

add_library(BORA_COMPAT SHARED src/library.def ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})

target_link_libraries(BORA_COMPAT PRIVATE Vulkan::Vulkan ntdll ${BORA_SKIA} ${BORA_LIBS} opengl32.lib)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)
add_dependencies(BORA_COMPAT skia_all)
set_target_properties(BORA_COMPAT PROPERTIES
        OUTPUT_NAME "LATEST"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/compatibility"
)


add_dependencies(BORA BORA_COMPAT)
