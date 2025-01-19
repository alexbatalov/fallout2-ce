set( CMAKE_SYSTEM_NAME      "Emscripten" )

add_compile_definitions("DEBUG=$<CONFIG:Debug>")

set( CMAKE_C_COMPILER                   "emcc" )
set( CMAKE_CXX_COMPILER                 "em++" )
set( CMAKE_C_FLAGS                      "${CMAKE_C_FLAGS} \
                                        --use-port=sdl2 \
                                        --use-port=sdl2_mixer \
                                        --use-port=zlib" )
set( CMAKE_CXX_FLAGS                    "${CMAKE_CXX_FLAGS} \
                                        --use-port=sdl2 \
                                        --use-port=sdl2_mixer \
                                        --use-port=zlib" )

set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -gsource-map")
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -gsource-map")

set( EXECUTABLE_NAME fallout2-ce.js )