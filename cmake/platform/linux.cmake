set (WAMR_BUILD_PLATFORM "linux")
include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

if (NOT DEFINED WAMR_BUILD_TARGET)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
        set (WAMR_BUILD_TARGET "AARCH64")
        if (NOT DEFINED WAMR_BUILD_SIMD)
            set (WAMR_BUILD_SIMD 1)
        endif ()
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
        set (WAMR_BUILD_TARGET "RISCV64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Build as X86_64 by default in 64-bit platform
        set (WAMR_BUILD_TARGET "X86_64")
        if (NOT DEFINED WAMR_BUILD_SIMD)
            set (WAMR_BUILD_SIMD 1)
        endif ()
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Build as X86_32 by default in 32-bit platform
        set (WAMR_BUILD_TARGET "X86_32")
    else ()
        message(SEND_ERROR "Unsupported build target platform!")
    endif ()
endif ()

if (WAMR_BUILD_WASI_NN EQUAL 1)
    set (BUILD_SHARED_LIBS ON)
endif ()

check_pie_supported()


set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Wshadow")
# set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wsign-conversion")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat -Wformat-security -Wno-unused")

if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
    if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mindirect-branch-register")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mindirect-branch-register")
        # UNDEFINED BEHAVIOR, refer to https://en.cppreference.com/w/cpp/language/ub
    endif ()
endif ()

include(${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common ./src/host/templates ../global/cpp ./src ./src/software/common ./src/software/linux ./contribs ./3rdparty)

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
target_compile_options(BORA PRIVATE -include${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)


file(GLOB_RECURSE BORA_SOURCES_CONTRIB_GRAPHICS contribs/bskia/src/* 3rdparty/GLEW/**/*.h)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE Debug)
else()
    set(BUILD_TYPE Release)
endif()

message(${BUILD_TYPE})
file(GLOB_RECURSE BORA_SKIA ${CMAKE_SOURCE_DIR}/cached_libs/bskia/${BUILD_TYPE}${ASAN_PREFIX}/*.a)
file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.a)

add_library(BORA_COMPAT SHARED src/library.def ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})

find_package(OpenGL REQUIRED)

target_link_libraries(BORA_COMPAT PRIVATE Vulkan::Vulkan ${BORA_SKIA} ${BORA_LIBS} ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread OpenGL::GL)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)
add_dependencies(BORA_COMPAT skia_all)
set_target_properties(BORA_COMPAT PROPERTIES
        OUTPUT_NAME "LATEST"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/compatibility"
)


add_dependencies(BORA BORA_COMPAT)
