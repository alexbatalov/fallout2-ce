# Build static lib only
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(BUILD_SHARED_LIBS OFF)


if (ANDROID)
    set(SDL_STATIC_PIC ON)
endif()

include(FetchContent)

FetchContent_Declare(sdl2ttf
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL_ttf"
    GIT_TAG "release-2.20.1"
)

FetchContent_GetProperties(sdl2ttf)
if (NOT sdl2ttf_POPULATED)
    FetchContent_Populate(sdl2ttf)
endif()

add_subdirectory(${sdl2ttf_SOURCE_DIR} ${sdl2ttf_BINARY_DIR} EXCLUDE_FROM_ALL)

set(SDL2TTF_INCLUDE_DIRS ${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR} PARENT_SCOPE)
set(SDL2TTF_LIBRARIES SDL2_ttf::SDL2_ttf-static PARENT_SCOPE)
