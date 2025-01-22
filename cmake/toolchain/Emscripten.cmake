set( CMAKE_SYSTEM_NAME      "Emscripten" )

add_compile_definitions("DEBUG=$<CONFIG:Debug>")

set( CMAKE_C_COMPILER                   "emcc" )
set( CMAKE_CXX_COMPILER                 "em++" )

set( EXECUTABLE_NAME fallout2-ce.js )