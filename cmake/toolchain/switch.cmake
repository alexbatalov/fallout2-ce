## Toolchain file for Nintendo Switch homebrew with devkitA64 & libnx.

## Generic settings

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION "DKA-NX-14")
set(CMAKE_SYSTEM_PROCESSOR "aarch64")

# If you're doing multiplatform builds, use this variable to check
# whether you're building for the Switch.
# A macro of the same name is defined as well, to be used within code.
set(SWITCH TRUE)

## devkitPro ecosystem settings

# Define a few important devkitPro system paths.
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" DEVKITPRO)
if(NOT IS_DIRECTORY ${DEVKITPRO})
    message(FATAL_ERROR "Please install devkitA64 or set DEVKITPRO in your environment.")
endif()

set(DEVKITA64 "${DEVKITPRO}/devkitA64")
set(LIBNX "${DEVKITPRO}/libnx")
set(PORTLIBS "${DEVKITPRO}/portlibs/switch")

# Add devkitA64 GCC tools to CMake.
if(WIN32)
    set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc.exe")
    set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++.exe")
    set(CMAKE_LINKER "${DEVKITA64}/bin/aarch64-none-elf-ld.exe")
    set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-gcc-ar.exe" CACHE STRING "")
    set(CMAKE_AS "${DEVKITA64}/bin/aarch64-none-elf-as.exe" CACHE STRING "")
    set(CMAKE_NM "${DEVKITA64}/bin/aarch64-none-elf-gcc-nm.exe" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-gcc-ranlib.exe" CACHE STRING "")
else()
    set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
    set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++")
    set(CMAKE_LINKER "${DEVKITA64}/bin/aarch64-none-elf-ld")
    set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-gcc-ar" CACHE STRING "")
    set(CMAKE_AS "${DEVKITA64}/bin/aarch64-none-elf-as" CACHE STRING "")
    set(CMAKE_NM "${DEVKITA64}/bin/aarch64-none-elf-gcc-nm" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-gcc-ranlib" CACHE STRING "")
endif()

# devkitPro and devkitA64 provide various tools for working with
# Switch file formats, which should be made accessible from CMake.
list(APPEND CMAKE_PROGRAM_PATH "${DEVKITPRO}/tools/bin")
list(APPEND CMAKE_PROGRAM_PATH "${DEVKITA64}/bin")

# devkitPro maintains a repository of so-called portlibs,
# which can be found at https://github.com/devkitPro/pacman-packages/tree/master/switch.
# They store PKGBUILDs and patches for various libraries
# so they can be cross-compiled and used within homebrew.
# These can be installed with (dkp-)pacman.
set(WITH_PORTLIBS ON CACHE BOOL "Use portlibs?")

## Cross-compilation settings

if(WITH_PORTLIBS)
    set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITA64} ${PORTLIBS})
else()
    set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITA64})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_INSTALL_PREFIX ${PORTLIBS} CACHE PATH "Install libraries to the portlibs directory")
set(CMAKE_PREFIX_PATH ${PORTLIBS} CACHE PATH "Find libraries in the portlibs directory")

## Options for code generation

# Technically, the Switch does support shared libraries, but the toolchain doesn't.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

add_definitions(-DSWITCH -D__SWITCH__)

set(ARCH "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O2 -ffunction-sections ${ARCH}" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions" CACHE STRING "C++ flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g ${ARCH}" CACHE STRING "ASM flags")
# These flags are purposefully empty to use the default flags when invoking the
# devkitA64 linker. Otherwise the linker may complain about duplicate flags.
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Executable linker flags")
set(CMAKE_STATIC_LINKER_FLAGS "" CACHE STRING "Library linker flags")
set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "Module linker flags")