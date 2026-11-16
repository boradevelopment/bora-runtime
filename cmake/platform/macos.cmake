
macro(bora_check_module TARGET_NAME MODULE_NAME DEF_NAME FRIENDLY_NAME)
    find_package(${MODULE_NAME} REQUIRED)
    if(${MODULE_NAME}_FOUND)
        target_compile_definitions(${TARGET_NAME} PRIVATE "${DEF_NAME}=1")
        target_include_directories(${TARGET_NAME} PRIVATE ${${MODULE_NAME}_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${${MODULE_NAME}_LIBRARIES})
        message(STATUS "${FRIENDLY_NAME} was found and has been imported into BORA")
        set(${MODULE_NAME}_FOUND TRUE)
    else()
        set(${MODULE_NAME}_FOUND FALSE)
        message(WARNING "${FRIENDLY_NAME} was not found, it will not be available in compilation.")
    endif()
endmacro()

set(BORA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(BORA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(BORA_VERSION_PATCH ${PROJECT_VERSION_PATCH})

message(NOTICE "BORA ${BORA_VERSION_MAJOR}.${BORA_VERSION_MINOR}.${BORA_VERSION_PATCH} is being built on Darwin MacOS ${ARCH}")

include_directories(
        src/common
        src/runtime
        ./src/guest/templates ../global/cpp ../global/contribs ./src ./src/runtime/host/common ./src/runtime/host/darwin ./src/runtime/host/posix ./contribs ./3rdparty)

collect_sources(BORA_SOURCES_PLATFORM
        contribs/*
        src/tools/*
        src/runtime/guest/*
        src/runtime/host/darwin/*
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
        __DARWIN__
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
target_link_libraries(BORA PRIVATE "-rdynamic")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Debug/*.a)
else()
    file(GLOB_RECURSE BORA_LIBS ${CMAKE_SOURCE_DIR}/libs/Release/*.a)
endif()

collect_sources(BORA_CONTRIBS_MM
        ../global/cpp/contribs/*.mm
)

add_library(BORA_COMPAT SHARED
        src/runtime/__runtime_main.cpp
        ${BORA_SOURCES_PLATFORM} ${BORA_CONTRIBS_MM} ${BORA_SOURCES} ${BORA_CONTRIBS} ${BORA_TAZA} ${BORA_3RDPARTY})
if(NOT ENABLE_ASAN)
    target_link_options(BORA_COMPAT PRIVATE "-Wl,-z,defs")
endif ()

target_link_libraries(BORA_COMPAT PRIVATE skia ${BORA_LIBS} wasmtime.a)
target_precompile_headers(BORA_COMPAT PRIVATE
        ../global/cpp/contribs/TypeDefinitions.h
)

bora_check_module(BORA_COMPAT OpenGL BORA_HAS_OPENGL "OpenGL")
bora_check_module(BORA_COMPAT Vulkan BORA_HAS_VULKAN "VULKAN")

if(BORA_HAS_OPENGL_PKG_FOUND)
    target_link_libraries(BORA_COMPAT PRIVATE EGL GL)
endif()

add_dependencies(BORA_COMPAT wasmtime_lib)
add_dependencies(BORA_COMPAT skia)

set_target_properties(BORA_COMPAT PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${SYSTEM_NAME_LOWER}/${ARCH}/compatibility"
        OUTPUT_NAME "bora.latest"
)
add_dependencies(BORA BORA_COMPAT)
