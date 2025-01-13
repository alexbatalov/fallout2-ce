# Do not build examples.
set(ZLIB_BUILD_EXAMPLES OFF)

include(FetchContent)

FetchContent_Declare(zlib
    GIT_REPOSITORY "https://github.com/madler/zlib"
    GIT_TAG "v1.3.1"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(zlib)

set(ZLIB_LIBRARIES "zlibstatic" PARENT_SCOPE)
