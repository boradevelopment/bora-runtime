include(ExternalProject)

set(SKIA_SOURCE_DIR "${CMAKE_SOURCE_DIR}/contribs/bskia")
set(SKIA_BUILD_DIR "${CMAKE_SOURCE_DIR}/cached_libs/bskia")

# Compiler flags
if (MSVC)
    if(ENABLE_ASAN)
    set(ASAN_CODE "/fsanitize$address")
    endif ()
endif()

if(ENABLE_ASAN)
    set(ASAN_PREFIX "ASAN")
endif()

if(WIN32)
    set(SKIA_SUFFIX ".bat")
else ()
    set(SKIA_SUFFIX ".sh")
endif ()

# ----------------------
# DEBUG BUILD
# ----------------------
ExternalProject_Add(
        skia_debug
        SOURCE_DIR ${SKIA_SOURCE_DIR}
        BINARY_DIR ${SKIA_BUILD_DIR}/Debug

        CONFIGURE_COMMAND
        cmd /C
        "cd /D ${SKIA_SOURCE_DIR} && build_all${SKIA_SUFFIX} ${SKIA_BUILD_DIR} DEBUG ${ASAN_CODE} ${ASAN_PREFIX}"

        INSTALL_COMMAND ""
        BUILD_COMMAND ""

        USES_TERMINAL_CONFIGURE true
)

ExternalProject_Add(
        skia_release
        SOURCE_DIR ${SKIA_SOURCE_DIR}
        BINARY_DIR ${SKIA_BUILD_DIR}/Release

        CONFIGURE_COMMAND
        cmd /C
        "cd /D ${SKIA_SOURCE_DIR} && build_all${SKIA_SUFFIX} ${SKIA_BUILD_DIR} RELEASE ${ASAN_CODE} ${ASAN_PREFIX}"

        INSTALL_COMMAND ""
        BUILD_COMMAND ""

        USES_TERMINAL_CONFIGURE true
)


add_custom_target(skia_all
        DEPENDS skia_debug skia_release
)