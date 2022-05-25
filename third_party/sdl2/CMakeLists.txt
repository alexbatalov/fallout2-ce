# Build static lib only
set(BUILD_SHARED_LIBS OFF)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

# Fixes _ftol2_sse already defined
set(SDL_LIBC ON)

include(FetchContent)

FetchContent_Declare(sdl2
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
    GIT_TAG "release-2.0.22"
)

FetchContent_GetProperties(sdl2)
if (NOT sdl2_POPULATED)
    FetchContent_Populate(sdl2)
endif()

add_subdirectory(${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR} EXCLUDE_FROM_ALL)
target_include_directories(fallout2-ce PUBLIC ${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR})
target_link_libraries(fallout2-ce SDL2-static SDL2::SDL2main)
