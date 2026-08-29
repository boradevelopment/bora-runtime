


include_directories(./src/host/templates ../global/cpp ../global/contribs ./src ./src/software/common ./src/software/linux ./contribs ./3rdparty)

collect_sources(BORA_SOURCES_PLATFORM
        src/tools/*
        src/host/*
        src/software/linux/*
)

add_compile_definitions(
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
        NOMINMAX
        _WINSOCKAPI_
        GLEW_STATIC
        LIBWASM_STATIC
        SPIRV_SKIA_CONFLICT_HACK
)
if(ENABLE_ASAN)
    add_compile_options("-fsanitize=address" "-fno-omit-frame-pointer")
    add_link_options("-fsanitize=address")
endif()

# Executable
add_executable(BORA
        ${BORA_SOURCES_WRAPPER}
        src/wrapper/mainWrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/artifacts/resource_wrapper.rc
)

target_compile_definitions(BORA PRIVATE
        TAZABASEDIR="${CMAKE_SOURCE_DIR}/contribs/TAZA/code"
        NOMINMAX _WINSOCKAPI_ WRAPPER SK_VULKAN
)
target_compile_options(BORA PRIVATE -I${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.a)
else()
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Release/*.a)
endif()

add_library(BORA_COMPAT SHARED ${BORA_SOURCES_PLATFORM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_link_libraries(BORA_COMPAT PRIVATE ${BORA_SKIA_LIBRARY} ${BORA_LIBS} wasmtime.a)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)

bora_check_module(BORA_COMPAT opengl BORA_HAS_OPENGL "OpenGL")
bora_check_module(BORA_COMPAT vulkan BORA_HAS_VULKAN "VULKAN")
bora_check_module(BORA_COMPAT gtk4 BORA_HAS_GTK4 "GTK4")
if(NOT GTK4_FOUND)
    bora_check_module(BORA_COMPAT gtk3+-3.0 BORA_HAS_GTK3 "GTK4")
endif()
bora_check_module(BORA_COMPAT gio-2.0 BORA_HAS_GIO "GIO")
bora_check_module(BORA_COMPAT wayland-client BORA_HAS_WAYLAND "Wayland")
if(BORA_HAS_WAYLAND_PKG_FOUND)
    find_program(WAYLAND_SCANNER wayland-scanner)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")

    if(WAYLAND_SCANNER AND EXISTS "${XDG_SHELL_XML}")
        execute_process(
                COMMAND ${WAYLAND_SCANNER} client-header
                "${XDG_SHELL_XML}"
                "${CMAKE_CURRENT_BINARY_DIR}/artifacts/xdg-shell-client-protocol.h"
        )
        execute_process(
                COMMAND ${WAYLAND_SCANNER} private-code
                "${XDG_SHELL_XML}"
                "${CMAKE_CURRENT_BINARY_DIR}/artifacts/xdg-shell-protocol.c"
        )

        target_sources(BORA_COMPAT PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/artifacts/xdg-shell-protocol.c)
        target_include_directories(BORA_COMPAT PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/artifacts)
        message(STATUS "BORA: Wayland and xdg-shell bindings generated successfully.")
    else()
        message(WARNING "BORA: wayland-scanner or xdg-shell.xml not found. Disabling Wayland backend.")
    endif()
endif()

bora_check_module(BORA_COMPAT x11 BORA_HAS_X11 "X11")

add_dependencies(BORA_COMPAT wasmtime_lib)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_dependencies(BORA_COMPAT skia_debug)
else()
    add_dependencies(BORA_COMPAT skia_release)
endif()

#
#if(DEBUGGER)
#    set_target_properties(BORA_COMPAT PROPERTIES OUTPUT_NAME "LATESTD")
#    set_target_properties(BORA PROPERTIES OUTPUT_NAME "borad")
#else()
#    set_target_properties(BORA_COMPAT PROPERTIES OUTPUT_NAME "LATEST")
#    set_target_properties(BORA PROPERTIES OUTPUT_NAME "bora")
#endif()

set_target_properties(BORA_COMPAT PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}/compatibility"
)
set_target_properties(BORA PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}"
)
add_dependencies(BORA BORA_COMPAT)
