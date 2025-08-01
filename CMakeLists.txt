# This is pretty dogshit.

cmake_minimum_required (VERSION 3.14)

include(CheckPIESupported)

set (BORA_RUNTIME_VERSION "0.0.1")
set(CMAKE_CXX_STANDARD 20)

# set (CMAKE_VERBOSE_MAKEFILE 1)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
project (BORA CXX ASM C)

if (WIN32)
set (WAMR_BUILD_PLATFORM "windows")

# Set WAMR_BUILD_TARGET, currently values supported:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]", "MIPS", "XTENSA"
if (NOT DEFINED WAMR_BUILD_TARGET)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Build as X86_64 by default in 64-bit platform
        set (WAMR_BUILD_TARGET "X86_64")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
        # Build as X86_32 by default in 32-bit platform
        set (WAMR_BUILD_TARGET "X86_32")
    else ()
        message(FATAL_ERROR "Unsupported build target platform!")
    endif ()
endif ()


# Reset default linker flags
set (CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set(WAMR_BUILD_DEBUG_INTERP 1)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

if (NOT DEFINED WAMR_BUILD_INTERP)
    # Enable Interpreter by default
    set (WAMR_BUILD_INTERP 1)
endif ()

if (NOT DEFINED WAMR_BUILD_AOT)
    # Enable AOT by default.
    set (WAMR_BUILD_AOT 1)
endif ()

if (NOT DEFINED WAMR_BUILD_JIT)
    # Disable JIT by default.
    set (WAMR_BUILD_JIT 0)
endif ()

if (NOT DEFINED WAMR_BUILD_LIBC_BUILTIN)
    # Enable libc builtin support by default
    set (WAMR_BUILD_LIBC_BUILTIN 1)
endif ()

if (NOT DEFINED WAMR_BUILD_LIBC_WASI)
    # Enable libc wasi support by default
    set (WAMR_BUILD_LIBC_WASI 1)
endif ()

if (NOT DEFINED WAMR_BUILD_FAST_INTERP)
    # Enable fast interpreter
    set (WAMR_BUILD_FAST_INTERP 1)
endif ()

if (NOT DEFINED WAMR_BUILD_MULTI_MODULE)
    # Enable multiple modules
    set (WAMR_BUILD_MULTI_MODULE 0)
endif ()

if (NOT DEFINED WAMR_BUILD_LIB_PTHREAD)
    # Disable pthread library by default
    set (WAMR_BUILD_LIB_PTHREAD 0)
endif ()

if (NOT DEFINED WAMR_BUILD_MINI_LOADER)
    # Disable wasm mini loader by default
    set (WAMR_BUILD_MINI_LOADER 0)
endif ()

if (NOT DEFINED WAMR_BUILD_SIMD)
    # Enable SIMD by default
    set (WAMR_BUILD_SIMD 1)
endif ()

if (NOT DEFINED WAMR_BUILD_MEMORY64)
    set (WAMR_BUILD_MEMORY64 1)
endif()

#if (NOT DEFINED WAMR_BUILD_DEBUG_INTERP)
#    # Disable Debug feature by default
#    set (WAMR_BUILD_DEBUG_INTERP 0)
#endif ()
#
if (WAMR_BUILD_DEBUG_INTERP EQUAL 1)
    set (WAMR_BUILD_FAST_INTERP 0)
    set (WAMR_BUILD_MINI_LOADER 0)
    set (WAMR_BUILD_SIMD 0)
endif ()

if (WAMR_BUILD_LIBC_WASI EQUAL 1)
    set (CMAKE_C_STANDARD 11)
    if (MSVC)
        add_compile_options(/experimental:c11atomics)
    endif()
else()
    set (CMAKE_C_STANDARD 99)
endif()

set (WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contribs/wasm-micro-runtime)

include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32_LEAN_AND_MEAN")
if (NOT MINGW)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
endif ()

# set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
# set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion -Wsign-conversion")

if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
    if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang" OR MSVC))
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mindirect-branch-register")
    endif ()
endif ()

# The following flags are to enhance security, but it may impact performance,
# we disable them by default.
#if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
#  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftrapv -D_FORTIFY_SOURCE=2")
#endif ()
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong --param ssp-buffer-size=4")
#set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,noexecstack,-z,relro,-z,now")

include (${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common
src/host/templates
)

# Collect source files
file(GLOB_RECURSE BORA_SOURCES src/*.cpp src/*.h src/*.hpp src/*.manifest src/*.rc)
include_directories(
        3rdparty
        ../global/cpp/contribs/TAZA/code
        ../global/cpp/contribs
)

# Collect source files
file(GLOB_RECURSE BORA_SOURCES_CONTRIB_TAZA ../global/cpp/contribs/TAZA/code/*.cpp  ../global/cpp/contribs/TAZA/code/*.h  ../global/cpp/contribs/TAZA/code/*.hpp)
file(GLOB_RECURSE BORA_SOURCES_3RDPARTY 3rdparty/**/*.cpp 3rdparty/**/*.h 3rdparty/**/*.hpp)

file(GLOB_RECURSE BORA_SOURCES_3RDPARTYDLIB ../global/libs/*.dlib)
file(GLOB_RECURSE BORA_SOURCES_3RDPARTYLIB ../global/libs/*.lib)

add_executable (BORA
    ${BORA_SOURCES} ${PLATFORM_SHARED_SOURCE} ${UNCOMMON_SHARED_SOURCE} ${UTILS_SHARED_SOURCE} ${BORA_SOURCES_CONTRIB_TAZA} ${BORA_SOURCES_3RDPARTY}
)

target_compile_options(BORA PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/FI ${CMAKE_CURRENT_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h>
)

target_compile_definitions(BORA PRIVATE
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
)


#set_version_info (BORA)

target_sources(BORA PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(BORA PRIVATE ${BORA_SOURCES_3RDPARTYDLIB} ntdll)
else ()
    target_link_libraries(BORA PRIVATE ${BORA_SOURCES_3RDPARTYLIB} ntdll)
endif()

# Compatibility DLL target
file(GLOB_RECURSE BORA_COMPAT_SOURCES compat_src/*.cpp compat_src/*.h compat_src/*.hpp)

add_library(BORA_COMPAT SHARED
        src/comp.def
        ${BORA_COMPAT_SOURCES}
        ${BORA_SOURCES} ${PLATFORM_SHARED_SOURCE} ${UNCOMMON_SHARED_SOURCE} ${UTILS_SHARED_SOURCE} ${BORA_SOURCES_CONTRIB_TAZA} ${BORA_SOURCES_3RDPARTY}
)

target_compile_options(BORA_COMPAT PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/FI ${CMAKE_CURRENT_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h>
)

# Include the runtime source headers for use in compatibility layer
target_include_directories(BORA_COMPAT PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${WAMR_ROOT_DIR}/product-mini/platforms/common
        src/host/templates
        3rdparty
        contribs/TAZA/code
)


#set_version_info(BORA_COMPAT)

set_target_properties(BORA_COMPAT PROPERTIES OUTPUT_NAME "BORA_COMPAT_${BORA_RUNTIME_VERSION}")


# Export settings or defines if needed
target_compile_definitions(BORA_COMPAT PRIVATE
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
)

target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(BORA_COMPAT PRIVATE ${BORA_SOURCES_3RDPARTYDLIB} ntdll)
else ()
    target_link_libraries(BORA_COMPAT PRIVATE ${BORA_SOURCES_3RDPARTYLIB} ntdll)
endif()

add_dependencies(BORA BORA_COMPAT)

elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (WAMR_BUILD_PLATFORM "linux")

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


    if (NOT DEFINED WAMR_BUILD_INTERP)
        # Enable Interpreter by default
        set (WAMR_BUILD_INTERP 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_AOT)
        # Enable AOT by default.
        set (WAMR_BUILD_AOT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_JIT)
        # Disable JIT by default.
        set (WAMR_BUILD_JIT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_FAST_JIT)
        # Disable Fast JIT by default
        set (WAMR_BUILD_FAST_JIT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIBC_BUILTIN)
        # Enable libc builtin support by default
        set (WAMR_BUILD_LIBC_BUILTIN 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIBC_WASI)
        # Enable libc wasi support by default
        set (WAMR_BUILD_LIBC_WASI 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_FAST_INTERP)
        # Enable fast interpreter
        set (WAMR_BUILD_FAST_INTERP 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_MULTI_MODULE)
        # Disable multiple modules by default
        set (WAMR_BUILD_MULTI_MODULE 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIB_PTHREAD)
        # Disable pthread library by default
        set (WAMR_BUILD_LIB_PTHREAD 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIB_WASI_THREADS)
        # Disable wasi threads library by default
        set (WAMR_BUILD_LIB_WASI_THREADS 0)
    endif()

    if (NOT DEFINED WAMR_BUILD_MINI_LOADER)
        # Disable wasm mini loader by default
        set (WAMR_BUILD_MINI_LOADER 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_SIMD)
        # Enable SIMD by default
        set (WAMR_BUILD_SIMD 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_REF_TYPES)
        # Enable reference types by default
        set (WAMR_BUILD_REF_TYPES 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_DEBUG_INTERP)
        # Disable Debug feature by default
        set (WAMR_BUILD_DEBUG_INTERP 1)
    endif ()

    if (WAMR_BUILD_DEBUG_INTERP EQUAL 1)
        set (WAMR_BUILD_FAST_INTERP 0)
        set (WAMR_BUILD_MINI_LOADER 0)
        set (WAMR_BUILD_SIMD 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_MEMORY64)
        set (WAMR_BUILD_MEMORY64 1)
    endif()

    # if enable wasi-nn, both wasi-nn-backends and iwasm
    # need to use same WAMR (dynamic) libraries
    if (WAMR_BUILD_WASI_NN EQUAL 1)
        set (BUILD_SHARED_LIBS ON)
    endif ()


    set (WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contribs/wasm-micro-runtime)


    include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

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

    include (${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
    include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common
            src/host/templates
    )

    # Collect source files
    file(GLOB_RECURSE BORA_SOURCES src/*.cpp src/*.h src/*.hpp src/*.manifest src/*.rc)
    include_directories(
            3rdparty
            ../global/cpp/contribs/TAZA/code
            ../global/cpp/contribs
    )

    # Collect source files
    file(GLOB_RECURSE BORA_SOURCES_CONTRIB_TAZA ../global/cpp/contribs/TAZA/code/*.cpp ../global/cpp/contribs/TAZA/code/*.h ../global/cpp/contribs/TAZA/code/*.hpp)
    file(GLOB_RECURSE BORA_SOURCES_3RDPARTY 3rdparty/**/*.cpp 3rdparty/**/*.h 3rdparty/**/*.hpp)

    file(GLOB_RECURSE BORA_SOURCES_3RDPARTYDLIB ../global/libs/*.da)
    file(GLOB_RECURSE BORA_SOURCES_3RDPARTYLIB ../global/libs/*.a)

    add_executable (BORA
            ${BORA_SOURCES} ${PLATFORM_SHARED_SOURCE} ${UNCOMMON_SHARED_SOURCE} ${UTILS_SHARED_SOURCE} ${BORA_SOURCES_CONTRIB_TAZA} ${BORA_SOURCES_3RDPARTY}
    )

    target_compile_options(BORA PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h>
    )

    target_compile_definitions(BORA PRIVATE
            TAZABASEDIR=\"${CMAKE_SOURCE_DIR}../global/cpp/contribs/TAZA/code\"
    )


   # set_version_info (BORA)

    target_sources(BORA PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(BORA PRIVATE ${BORA_SOURCES_3RDPARTYDLIB} ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread)
    else ()
        target_link_libraries(BORA PRIVATE ${BORA_SOURCES_3RDPARTYLIB} ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread)
    endif()

    target_link_directories(BORA PRIVATE ../global/libs/__linux__)

    target_link_libraries(BORA PRIVATE
            #${BORA_SOURCES_3RDPARTYDLIB}
            -lbrotlienc -lbrotlidec -lbrotlicommon
    )

    # Compatibility DLL target
    file(GLOB_RECURSE BORA_COMPAT_SOURCES compat_src/*.cpp compat_src/*.h compat_src/*.hpp)

    add_library(BORA_COMPAT SHARED
            src/comp.def
            ${BORA_COMPAT_SOURCES}
            ${BORA_SOURCES} ${PLATFORM_SHARED_SOURCE} ${UNCOMMON_SHARED_SOURCE} ${UTILS_SHARED_SOURCE} ${BORA_SOURCES_CONTRIB_TAZA} ${BORA_SOURCES_3RDPARTY}
    )

#    target_compile_options(BORA_COMPAT PRIVATE
#            $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<COMPILE_LANGUAGE:CXX>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/TypeDefinitions.h>
#    )

    # Include the runtime source headers for use in compatibility layer
    target_include_directories(BORA_COMPAT PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${WAMR_ROOT_DIR}/product-mini/platforms/common
            src/host/templates
            3rdparty
            ../global/cpp/contribs/TAZA/code
    )


    set_target_properties(BORA_COMPAT PROPERTIES OUTPUT_NAME "BORA_COMPAT_${BORA_RUNTIME_VERSION}")


    # Export settings or defines if needed
    target_compile_definitions(BORA_COMPAT PRIVATE
            TAZABASEDIR=\"${CMAKE_SOURCE_DIR}../global/cpp/contribs/TAZA/code\"
    )

    target_sources(BORA_COMPAT PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})


    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(BORA_COMPAT PRIVATE ${BORA_SOURCES_3RDPARTYDLIB} ntdll)
    else ()
        target_link_libraries(BORA_COMPAT PRIVATE ${BORA_SOURCES_3RDPARTYLIB} ntdll)
    endif()

   # add_dependencies(BORA BORA_COMPAT)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    enable_language(OBJCXX)

    set (WAMR_BUILD_PLATFORM "darwin")

    # Reset default linker flags
    set (CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
    set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

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


    if (NOT DEFINED WAMR_BUILD_INTERP)
        # Enable Interpreter by default
        set (WAMR_BUILD_INTERP 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_AOT)
        # Enable AOT by default.
        set (WAMR_BUILD_AOT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_JIT)
        # Disable JIT by default.
        set (WAMR_BUILD_JIT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_FAST_JIT)
        # Disable Fast JIT by default
        set (WAMR_BUILD_FAST_JIT 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIBC_BUILTIN)
        # Enable libc builtin support by default
        set (WAMR_BUILD_LIBC_BUILTIN 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIBC_WASI)
        # Enable libc wasi support by default
        set (WAMR_BUILD_LIBC_WASI 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_FAST_INTERP)
        # Enable fast interpreter
        set (WAMR_BUILD_FAST_INTERP 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_MULTI_MODULE)
        # Disable multiple modules by default
        set (WAMR_BUILD_MULTI_MODULE 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIB_PTHREAD)
        # Disable pthread library by default
        set (WAMR_BUILD_LIB_PTHREAD 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_LIB_WASI_THREADS)
        # Disable wasi threads library by default
        set (WAMR_BUILD_LIB_WASI_THREADS 0)
    endif()

    if (NOT DEFINED WAMR_BUILD_MINI_LOADER)
        # Disable wasm mini loader by default
        set (WAMR_BUILD_MINI_LOADER 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_SIMD)
        # Enable SIMD by default
        set (WAMR_BUILD_SIMD 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_REF_TYPES)
        # Enable reference types by default
        set (WAMR_BUILD_REF_TYPES 1)
    endif ()

    if (NOT DEFINED WAMR_BUILD_DEBUG_INTERP)
        # Disable Debug feature by default
        set (WAMR_BUILD_DEBUG_INTERP 1)
    endif ()

    if (WAMR_BUILD_DEBUG_INTERP EQUAL 1)
        set (WAMR_BUILD_FAST_INTERP 0)
        set (WAMR_BUILD_MINI_LOADER 0)
        set (WAMR_BUILD_SIMD 0)
    endif ()

    if (NOT DEFINED WAMR_BUILD_MEMORY64)
        set (WAMR_BUILD_MEMORY64 1)
    endif()

    # if enable wasi-nn, both wasi-nn-backends and iwasm
    # need to use same WAMR (dynamic) libraries
    if (WAMR_BUILD_WASI_NN EQUAL 1)
        set (BUILD_SHARED_LIBS ON)
    endif ()


    set (WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/contribs/wasm-micro-runtime)
    set (CMAKE_SHARED_LINKER_FLAGS "-Wl,-U,_get_ext_lib_export_apis")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set (CMAKE_MACOSX_RPATH True)

    include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

    include (${WAMR_ROOT_DIR}/core/shared/utils/uncommon/shared_uncommon.cmake)
    include_directories(${WAMR_ROOT_DIR}/product-mini/platforms/common
            src/host/templates
    )

    # Collect source files
    file(GLOB_RECURSE BORA_SOURCES src/*.cpp src/*.h src/*.hpp src/*.manifest src/*.mm)
    include_directories(
            3rdparty
            ../global/cpp/contribs/TAZA/code
            ../global/cpp/contribs
            src
    )

    # Collect source files
    file(GLOB_RECURSE BORA_SOURCES_CONTRIB_TAZA ../global/cpp/contribs/TAZA/code/*.cpp ../global/cpp/contribs/TAZA/code/*.h ../global/cpp/contribs/TAZA/code/*.hpp)
    file(GLOB_RECURSE BORA_SOURCES_3RDPARTY 3rdparty/**/*.cpp 3rdparty/**/*.h 3rdparty/**/*.hpp)

    file(GLOB_RECURSE BORA_SOURCES_3RDPARTYDLIB ../global/libs/*.da)
    file(GLOB_RECURSE BORA_SOURCES_3RDPARTYLIB ../global/libs/*.a)

    file(GLOB_RECURSE BORA_SOURCEMM src/*.mm)

    set_source_files_properties(${BORA_SOURCEMM} PROPERTIES LANGUAGE OBJCXX)


    add_executable (BORA
            ${BORA_SOURCES} ${PLATFORM_SHARED_SOURCE} ${UNCOMMON_SHARED_SOURCE} ${UTILS_SHARED_SOURCE} ${BORA_SOURCES_CONTRIB_TAZA} ${BORA_SOURCES_3RDPARTY}
    )

    target_compile_options(BORA PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h>
    )

    target_compile_definitions(BORA PRIVATE
            TAZABASEDIR=\"${CMAKE_SOURCE_DIR}../global/cpp/contribs/TAZA/code\"
    )



    #set_version_info (BORA)

    target_sources(BORA PRIVATE ${WAMR_RUNTIME_LIB_SOURCE})

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(BORA PRIVATE ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread)
    else ()
        target_link_libraries(BORA PRIVATE ${LLVM_AVAILABLE_LIBS} ${UV_A_LIBS} -lm -ldl -lpthread)
    endif()

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    target_link_directories(BORA PRIVATE ../global/libs/__apple__)
    else()
    target_link_directories(BORA PRIVATE ../global/libs/__apple__x86)
    endif()

    target_link_libraries(BORA PRIVATE
            #${BORA_SOURCES_3RDPARTYDLIB}
            -lbrotlienc -lbrotlidec -lbrotlicommon
            "-framework AppKit"
    )

endif()
