include(FetchContent)

FetchContent_Declare(zlib
    GIT_REPOSITORY "https://github.com/madler/zlib"
    GIT_TAG "v1.2.11"
)

FetchContent_GetProperties(zlib)
if (NOT zlib_POPULATED)
    FetchContent_Populate(zlib)
endif()

add_subdirectory(${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} EXCLUDE_FROM_ALL)
target_include_directories(fallout2-ce PUBLIC ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
target_link_libraries(fallout2-ce zlibstatic)
