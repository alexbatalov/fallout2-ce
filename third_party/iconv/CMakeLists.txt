
include(FetchContent)

FetchContent_Declare(iconv
    GIT_REPOSITORY "https://github.com/sonilyan/libiconv_for_fallout2ce.git"
)

FetchContent_GetProperties(iconv)
if (NOT iconv_POPULATED)
    FetchContent_Populate(iconv)
endif()

add_subdirectory(${iconv_SOURCE_DIR} ${iconv_BINARY_DIR} EXCLUDE_FROM_ALL)

set(ICONV_LIBRARIES "iconv-static" PARENT_SCOPE)
set(ICONV_INCLUDE_DIRS ${iconv_SOURCE_DIR} ${iconv_BINARY_DIR}/include PARENT_SCOPE)



