collect_sources(BORA_SOURCES_PLATFORM
        src/host/*
        src/software/darwin/*
)
enable_language(OBJCXX)

file(GLOB_RECURSE BORA_DARWIN_SOURCE src/software/darwin/*)
file(GLOB_RECURSE BORA_CONTRIBS_DARWIN
        ../global/cpp/contribs/*.mm
)

set_source_files_properties(${BORA_DARWIN_SOURCE} PROPERTIES LANGUAGE OBJCXX)

set(WAMR_BUILD_PLATFORM "darwin")
include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
find_package(OpenGL REQUIRED)
# Set WAMR_BUILD_TARGET, currently values supported:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]",
# "MIPS", "XTENSA", "RISCV64[sub]", "RISCV32[sub]"
if (NOT DEFINED WAMR_BUILD_TARGET)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
        set (WAMR_BUILD_TARGET "AARCH64")
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
        set (WAMR_BUILD_TARGET "RISCV64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Build as X86_64 by default in 64-bit platform
        set (WAMR_BUILD_TARGET "X86_64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Build as X86_32 by default in 32-bit platform
        set (WAMR_BUILD_TARGET "X86_32")
    else ()
        message(SEND_ERROR "Unsupported build target platform!")
    endif ()
endif ()

set (WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contribs/wasm-micro-runtime)
set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-U,_get_ext_lib_export_apis")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
set (CMAKE_MACOSX_RPATH True)

include(${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common ./src/host/templates ../global/cpp ../global/contribs ./src ./src/software/common ./src/software/darwin ./contribs ./3rdparty)

add_compile_definitions(
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
        NOMINMAX
        _WINSOCKAPI_
        GLEW_STATIC
        SPIRV_SKIA_CONFLICT_HACK
)

if(ENABLE_ASAN)
    add_compile_options("-fsanitize=address")
    add_link_options("-fsanitize=address")
endif()


# Executable
add_executable(BORA
    ${BORA_SOURCES_WRAPPER}
)

target_compile_definitions(BORA PRIVATE
        TAZABASEDIR="${CMAKE_SOURCE_DIR}/contribs/TAZA/code"
        NOMINMAX _WINSOCKAPI_ WRAPPER SK_DIRECT3D
)

add_compile_options(
  -include ${CMAKE_CURRENT_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.a)
else()
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Release/*.a)
endif()


add_library(BORA_COMPAT SHARED ${BORA_CONTRIBS_DARWIN} ${BORA_SOURCES_PLATFORM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})
target_link_libraries(BORA_COMPAT PRIVATE Vulkan::Vulkan OpenGL::GL ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread ${BORA_SKIA_LIBRARY} ${BORA_LIBS}
        -lbrotlienc -lbrotlidec -lbrotlicommon
        "-framework AppKit"
)

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
