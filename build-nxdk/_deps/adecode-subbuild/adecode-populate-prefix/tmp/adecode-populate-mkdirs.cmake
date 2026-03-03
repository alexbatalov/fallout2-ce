# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-src")
  file(MAKE_DIRECTORY "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-src")
endif()
file(MAKE_DIRECTORY
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-build"
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix"
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/tmp"
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/src/adecode-populate-stamp"
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/src"
  "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/src/adecode-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/src/adecode-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/milenko/Documents/GitHub/fallout2-ce/build-nxdk/_deps/adecode-subbuild/adecode-populate-prefix/src/adecode-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
