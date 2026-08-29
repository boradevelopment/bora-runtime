include_directories(./src/host/templates ../global/cpp ../global/contribs ./src ./src/software/common ./src/software/win32 ./contribs ./3rdparty)

collect_sources(BORA_SOURCES_PLATFORM
        src/tools/*
        src/host/*
        src/software/win32/*
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
    if(MSVC)
        add_link_options("/INFERASANLIBS")
    else()
        add_compile_options("-fsanitize=address" "-fno-omit-frame-pointer")
        add_link_options("-fsanitize=address")
    endif()
endif()

# Resources
set(BORA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(BORA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(BORA_VERSION_PATCH ${PROJECT_VERSION_PATCH})

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/win32/resource_template_dll.rc
        ${CMAKE_CURRENT_BINARY_DIR}/artifacts/resource_dll.rc
        @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/win32/resource_template_wrapper.rc
        ${CMAKE_CURRENT_BINARY_DIR}/artifacts/resource_wrapper.rc
        @ONLY
)

# Executable
add_executable(BORA
            ${BORA_SOURCES_WRAPPER}
        src/wrapper/mainWrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/artifacts/resource_wrapper.rc
        src/software/win32/winmeta/DPI.manifest
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
#
add_library(BORA_COMPAT SHARED ${CMAKE_CURRENT_BINARY_DIR}/artifacts/resource_dll.rc src/library.def ${BORA_SOURCES_PLATFORM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
target_link_libraries(BORA_COMPAT PRIVATE Vulkan::Vulkan ntdll ${BORA_SKIA_LIBRARY} ${BORA_LIBS} opengl32.lib wasmtime.lib userenv.lib bcrypt.lib ws2_32.lib ntdll.lib)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)

add_dependencies(BORA_COMPAT wasmtime_lib)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_dependencies(BORA_COMPAT skia_debug)
else()
    add_dependencies(BORA_COMPAT skia_release)
endif()

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
