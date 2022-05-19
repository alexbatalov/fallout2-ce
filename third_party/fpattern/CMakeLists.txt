include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/Loadmaster/fpattern"
    GIT_TAG "v1.9"
)

FetchContent_GetProperties(fpattern)
if (NOT fpattern_POPULATED)
    FetchContent_Populate(fpattern)
endif()

target_sources(fallout2-ce PUBLIC
    "${fpattern_SOURCE_DIR}/debug.h"
    "${fpattern_SOURCE_DIR}/fpattern.c"
    "${fpattern_SOURCE_DIR}/fpattern.h"
)

target_include_directories(fallout2-ce PUBLIC ${fpattern_SOURCE_DIR})
