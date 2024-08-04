# Find SDL2 library
# This module defines
#  SDL2_INCLUDE_DIR, where to find SDL2.h, etc.
#  SDL2_LIBRARIES, the libraries to link against
#  SDL2_FOUND, if false, do not try to use SDL2.

find_path(SDL2_INCLUDE_DIR SDL.h
    HINTS
    ${CMAKE_PREFIX_PATH}/include/SDL2
)

find_library(SDL2_LIBRARY
    NAMES SDL2
    HINTS
    ${CMAKE_PREFIX_PATH}/lib
)

find_library(SDL2_TTF_LIBRARY
    NAMES SDL2_ttf
    HINTS
    ${CMAKE_PREFIX_PATH}/lib
)

find_library(SDL2_MIXER_LIBRARY
    NAMES SDL2_mixer
    HINTS
    ${CMAKE_PREFIX_PATH}/lib
)

find_library(SDL2_IMAGE_LIBRARY
    NAMES SDL2_image
    HINTS
    ${CMAKE_PREFIX_PATH}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 DEFAULT_MSG SDL2_LIBRARY SDL2_INCLUDE_DIR)

if(SDL2_FOUND)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY} ${SDL2_TTF_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_IMAGE_LIBRARY})
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
else()
    set(SDL2_LIBRARIES)
    set(SDL2_INCLUDE_DIRS)
endif()

mark_as_advanced(SDL2_INCLUDE_DIR SDL2_LIBRARY SDL2_LIBRARIES)
