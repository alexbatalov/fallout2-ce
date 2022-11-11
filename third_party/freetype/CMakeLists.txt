SET(BUILD_SHARED_LIBS FALSE)
SET(FT_DISABLE_HARFBUZZ ON)
SET(FT_DISABLE_PNG ON)
SET(FT_DISABLE_BROTLI ON)
SET(FT_DISABLE_BZIP2 ON)
SET(FT_DISABLE_ZLIB ON)
SET(FT_CONFIG_OPTION_USE_ZLIB OFF)

include(FetchContent)

FetchContent_Declare(freetype
    GIT_REPOSITORY "https://github.com/freetype/freetype"
    GIT_TAG "VER-2-12-1"
)

FetchContent_GetProperties(freetype)
if (NOT freetype_POPULATED)
    FetchContent_Populate(freetype)
endif()

add_subdirectory(${freetype_SOURCE_DIR} ${freetype_BINARY_DIR} EXCLUDE_FROM_ALL)

set(FREETYPE_LIBRARIES "freetype" PARENT_SCOPE)
set(FREETYPE_INCLUDE_DIRS ${freetype_SOURCE_DIR} ${freetype_BINARY_DIR} PARENT_SCOPE)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/ftoption.h" ORIGINAL_FTOPTION_H)
file(WRITE "${freetype_BINARY_DIR}/include/freetype/config/ftoption.h" "${ORIGINAL_FTOPTION_H}")

