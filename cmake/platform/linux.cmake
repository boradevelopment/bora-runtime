set(BORA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(BORA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(BORA_VERSION_PATCH ${PROJECT_VERSION_PATCH})

message(NOTICE "BORA ${BORA_VERSION_MAJOR}.${BORA_VERSION_MINOR}.${BORA_VERSION_PATCH} is being built on Linux ${ARCH}")


include_directories(
        src/common
        src/runtime
        ./src/guest/templates ../global/cpp ../global/contribs ./src ./src/runtime/host/common ./src/runtime/host/linux ./src/runtime/host/posix ./contribs ./3rdparty)

collect_sources(BORA_SOURCES_PLATFORM
        contribs/*
        src/tools/*
        src/runtime/guest/*
        src/runtime/host/linux/*
        src/runtime/host/posix/*
        src/supervisor/*
        src/common/*
)

add_compile_definitions(
        TAZABASEDIR=\"${CMAKE_SOURCE_DIR}/contribs/TAZA/code\"
        NOMINMAX
        _WINSOCKAPI_
        GLEW_STATIC
        LIBWASM_STATIC
        SPIRV_SKIA_CONFLICT_HACK
        __POSIX__
        BORA_ENABLE_SUPERVISOR
        BORA_VERSION_MAJOR=${BORA_VERSION_MAJOR}
        BORA_VERSION_MINOR=${BORA_VERSION_MINOR}
        BORA_VERSION_PATCH=${BORA_VERSION_PATCH}
        BORA_VERSION=\"${BORA_VERSION_MAJOR}.${BORA_VERSION_MINOR}.${BORA_VERSION_PATCH}\"
)
if(ENABLE_ASAN)
    add_compile_options("-fsanitize=address" "-fno-omit-frame-pointer")
    add_link_options("-fsanitize=address")
endif()

# Executable
add_executable(BORA
        src/wrapper/mainWrapper.cpp
        src/wrapper/VersionLoader.cpp
        ../global/cpp/contribs/tools/DynamicLibrary.cpp
        ../global/cpp/contribs/tools/AppParam.cpp
        ../global/cpp/contribs/tools/StringUtils.cpp
)

target_compile_definitions(BORA PRIVATE
        TAZABASEDIR="${CMAKE_SOURCE_DIR}/contribs/TAZA/code"
        NOMINMAX _WINSOCKAPI_ WRAPPER SK_VULKAN
)
target_compile_options(BORA PRIVATE -include ${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)
target_link_libraries(BORA PRIVATE "-rdynamic")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.a)
else()
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Release/*.a)
endif()

add_library(BORA_COMPAT SHARED
        src/runtime/__runtime_main.cpp
        ${BORA_SOURCES_PLATFORM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
if(NOT ENABLE_ASAN)
target_link_options(BORA_COMPAT PRIVATE "-Wl,-z,defs")
endif ()

target_compile_options(BORA_COMPAT PRIVATE -include ${CMAKE_SOURCE_DIR}/../global/cpp/contribs/TypeDefinitions.h)
target_link_libraries(BORA_COMPAT PRIVATE skia ${BORA_LIBS} wasmtime.a)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)

bora_check_module(BORA_COMPAT opengl BORA_HAS_OPENGL "OpenGL")
bora_check_module(BORA_COMPAT fontconfig BORA_HAS_FONTCONFIG "FONTCONFIG")
bora_check_module(BORA_COMPAT vulkan BORA_HAS_VULKAN "VULKAN")
bora_check_module(BORA_COMPAT gtk4 BORA_HAS_GTK4 "GTK4")
if(NOT BORA_HAS_GTK4_FOUND)
    bora_check_module(BORA_COMPAT gtk3+-3.0 BORA_HAS_GTK3 "GTK3")
endif()
bora_check_module(BORA_COMPAT gio-2.0 BORA_HAS_GIO "GIO")
bora_check_module(BORA_COMPAT wayland-client BORA_HAS_WAYLAND "Wayland")
bora_check_module(BORA_COMPAT x11 BORA_HAS_X11 "X11")
if(BORA_HAS_OPENGL_PKG_FOUND)
    target_link_libraries(BORA_COMPAT PRIVATE EGL GL)
endif()
if(BORA_HAS_X11_PKG_FOUND)
    target_link_libraries(BORA_COMPAT PRIVATE X11 Xrandr)
endif ()

if(BORA_HAS_WAYLAND_PKG_FOUND)
    target_link_libraries(BORA_COMPAT PRIVATE wayland-cursor wayland-egl)
    find_program(WAYLAND_SCANNER wayland-scanner)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")

    if(WAYLAND_SCANNER AND EXISTS "${XDG_SHELL_XML}")
        add_custom_command(
                OUTPUT "${XDG_SHELL_CLIENT_HEADER}" "${XDG_SHELL_CODE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/artifacts"
                COMMAND ${WAYLAND_SCANNER} client-header "${XDG_SHELL_XML}" "${XDG_SHELL_CLIENT_HEADER}.tmp"
                COMMAND ${WAYLAND_SCANNER} private-code "${XDG_SHELL_XML}" "${XDG_SHELL_CODE}.tmp"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${XDG_SHELL_CLIENT_HEADER}.tmp" "${XDG_SHELL_CLIENT_HEADER}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${XDG_SHELL_CODE}.tmp" "${XDG_SHELL_CODE}"
                DEPENDS "${XDG_SHELL_XML}"
                COMMENT "Generating Wayland xdg-shell protocols"
        )

        target_sources(BORA_COMPAT PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/artifacts/xdg-shell-protocol.c)
        target_include_directories(BORA_COMPAT PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/artifacts)
        message(STATUS "Wayland and xdg-shell bindings generated successfully.")
    else()
        message(WARNING "wayland-scanner or xdg-shell.xml not found. Disabling Wayland backend.")
    endif()
endif()


add_dependencies(BORA_COMPAT wasmtime_lib)
add_dependencies(BORA_COMPAT skia)

set_target_properties(BORA_COMPAT PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}/compatibility"
        OUTPUT_NAME "bora.latest"
)
add_dependencies(BORA BORA_COMPAT)
