set( CMAKE_SYSTEM_NAME      "Emscripten" )

add_compile_definitions("DEBUG=$<CONFIG:Debug>")

set( CMAKE_C_COMPILER                   "emcc" )
set( CMAKE_CXX_COMPILER                 "em++" )



set( EXECUTABLE_NAME fallout2-ce.js )

option(OPTION_WASM_64 "Build wasm64 module" OFF)
option(OPTION_WASM_JSPI "Build wasm with JSPI support instead of asyncify module" OFF)

# Touch toolchain variable to suppress "unused variable" warning.
# This happens if CMake is invoked with the same command line the second time.
if(CMAKE_TOOLCHAIN_FILE)
endif()
