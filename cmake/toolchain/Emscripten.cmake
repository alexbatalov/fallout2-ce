set( CMAKE_SYSTEM_NAME      "Emscripten" )

set( CMAKE_C_COMPILER                   "emcc" )
set( CMAKE_CXX_COMPILER                 "em++" )
set( CMAKE_C_FLAGS                      "--use-port=sdl2 \
                                        --use-port=sdl2_mixer \
                                        --use-port=zlib \
                                        -gsource-map" )
set( CMAKE_CXX_FLAGS                    "--use-port=sdl2 \
                                        --use-port=sdl2_mixer \
                                        --use-port=zlib \
                                        -gsource-map" )
set( EXECUTABLE_NAME fallout2-ce.js )