# Build static lib only
set(SDL_SHARED OFF)
set(SDL_STATIC ON)

# Fixes _ftol2_sse already defined
set(SDL_LIBC ON)

if (ANDROID)
    set(SDL_STATIC_PIC ON)
endif()

include(FetchContent)

FetchContent_Declare(sdl2
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL"
    GIT_TAG "release-2.26.1"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(sdl2)

set(SDL2_LIBRARIES SDL2-static SDL2::SDL2main PARENT_SCOPE)
