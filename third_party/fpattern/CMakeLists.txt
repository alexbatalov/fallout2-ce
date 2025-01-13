include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/Loadmaster/fpattern"
    GIT_TAG "v1.9"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fpattern)

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
endif()

add_library(fpattern STATIC
    "${fpattern_SOURCE_DIR}/debug.h"
    "${fpattern_SOURCE_DIR}/fpattern.c"
    "${fpattern_SOURCE_DIR}/fpattern.h"
)

target_include_directories(fpattern PUBLIC "${fpattern_SOURCE_DIR}")

if(NOT WIN32)
    target_compile_definitions(fpattern PRIVATE
        "-Dunix"
    )
endif()

set(FPATTERN_LIBRARY "fpattern" PARENT_SCOPE)
