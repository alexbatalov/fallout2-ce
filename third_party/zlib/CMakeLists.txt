# Do not build examples.
set(ZLIB_BUILD_EXAMPLES OFF)

include(FetchContent)

FetchContent_Declare(zlib
    GIT_REPOSITORY "https://github.com/madler/zlib"
    GIT_TAG "v1.3.1"
)

FetchContent_GetProperties(zlib)
if (NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
endif()

add_subdirectory("${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}" EXCLUDE_FROM_ALL)

set(ZLIB_LIBRARIES "zlibstatic" PARENT_SCOPE)
set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}" PARENT_SCOPE)
