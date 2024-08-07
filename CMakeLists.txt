cmake_minimum_required(VERSION 3.13)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(EXECUTABLE_NAME fallout2-ce)

project(${EXECUTABLE_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(NOT DEFINED ENV{F2_VERSION})
  set(F2_VERSION "0.0.0")
else()
  set(F2_VERSION $ENV{F2_VERSION})
endif()

set(SDL2_INCLUDE_DIR "/opt/devkitpro/portlibs/switch/include/SDL2")
set(SDL2_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libSDL2.a")
set(SDL2_TTF_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libSDL2_ttf.a")
set(SDL2_MIXER_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libSDL2_mixer.a")
set(SDL2_IMAGE_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libSDL2_image.a")

set(ZLIB_INCLUDE_DIR "/opt/devkitpro/portlibs/switch/include")
set(ZLIB_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libz.a")

set(EGL_INCLUDE_DIR "/opt/devkitpro/portlibs/switch/include/EGL")
set(EGL_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libEGL.a")

set(NOUVEAU_INCLUDE_DIR "/opt/devkitpro/portlibs/switch/include/nouveau")
set(NOUVEAU_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libdrm_nouveau.a")

set(GLAPI_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libglapi.a")
set(GLES_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libGLESv2.a")

set(NX_LIBS "/opt/devkitpro/libnx/lib/libnx.a")

set(FREETYPE_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libfreetype.a")
set(HARFBUZZ_LIBRARY "/opt/devkitpro/portlibs/switch/lib/libharfbuzz.a")

include_directories(${SDL2_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} ${EGL_INCLUDE_DIR} ${NOUVEAU_INCLUDE_DIR} /opt/devkitpro/libnx/include)
link_directories(/opt/devkitpro/portlibs/switch/lib /opt/devkitpro/libnx/lib)

include_directories(${CMAKE_SOURCE_DIR}/third_party/fpattern)

add_definitions(-D__SWITCH__)

if(NX)
    add_executable(${EXECUTABLE_NAME}
        "src/actions.cc"
        "src/actions.h"
        "src/animation.cc"
        "src/animation.h"
        "src/art.cc"
        "src/art.h"
        "src/audio_file.cc"
        "src/audio_file.h"
        "src/audio.cc"
        "src/audio.h"
        "src/automap.cc"
        "src/automap.h"
        "src/autorun.cc"
        "src/autorun.h"
        "src/cache.cc"
        "src/cache.h"
        "src/character_editor.cc"
        "src/character_editor.h"
        "src/character_selector.cc"
        "src/character_selector.h"
        "src/color.cc"
        "src/color.h"
        "src/combat_ai.cc"
        "src/combat_ai.h"
        "src/combat_defs.h"
        "src/combat.cc"
        "src/combat.h"
        "src/config.cc"
        "src/config.h"
        "src/credits.cc"
        "src/credits.h"
        "src/critter.cc"
        "src/critter.h"
        "src/cycle.cc"
        "src/cycle.h"
        "src/datafile.cc"
        "src/datafile.h"
        "src/db.cc"
        "src/db.h"
        "src/dbox.cc"
        "src/dbox.h"
        "src/debug.cc"
        "src/debug.h"
        "src/dfile.cc"
        "src/dfile.h"
        "src/dialog.cc"
        "src/dialog.h"
        "src/dictionary.cc"
        "src/dictionary.h"
        "src/dinput.cc"
        "src/dinput.h"
        "src/display_monitor.cc"
        "src/display_monitor.h"
        "src/draw.cc"
        "src/draw.h"
        "src/elevator.cc"
        "src/elevator.h"
        "src/endgame.cc"
        "src/endgame.h"
        "src/export.cc"
        "src/export.h"
        "src/file_find.cc"
        "src/file_find.h"
        "src/file_utils.cc"
        "src/file_utils.h"
        "src/font_manager.cc"
        "src/font_manager.h"
        "src/game_config.cc"
        "src/game_config.h"
        "src/game_dialog.cc"
        "src/game_dialog.h"
        "src/game_memory.cc"
        "src/game_memory.h"
        "src/game_mouse.cc"
        "src/game_mouse.h"
        "src/game_movie.cc"
        "src/game_movie.h"
        "src/game_sound.cc"
        "src/game_sound.h"
        "src/game_vars.h"
        "src/game.cc"
        "src/game.h"
        "src/geometry.cc"
        "src/geometry.h"
        "src/graph_lib.cc"
        "src/graph_lib.h"
        "src/heap.cc"
        "src/heap.h"
        "src/input.cc"
        "src/input.h"
        "src/interface.cc"
        "src/interface.h"
        "src/interpreter_extra.cc"
        "src/interpreter_extra.h"
        "src/interpreter_lib.cc"
        "src/interpreter_lib.h"
        "src/interpreter.cc"
        "src/interpreter.h"
        "src/inventory.cc"
        "src/inventory.h"
        "src/item.cc"
        "src/item.h"
        "src/kb.cc"
        "src/kb.h"
        "src/light.cc"
        "src/light.h"
        "src/lips.cc"
        "src/lips.h"
        "src/loadsave.cc"
        "src/loadsave.h"
        "src/main.cc"
        "src/main.h"
        "src/mainmenu.cc"
        "src/mainmenu.h"
        "src/map_defs.h"
        "src/map.cc"
        "src/map.h"
        "src/memory_defs.h"
        "src/memory_manager.cc"
        "src/memory_manager.h"
        "src/memory.cc"
        "src/memory.h"
        "src/message.cc"
        "src/message.h"
        "src/mouse_manager.cc"
        "src/mouse_manager.h"
        "src/mouse.cc"
        "src/movie_effect.cc"
        "src/movie_effect.h"
        "src/movie_lib.cc"
        "src/movie_lib.h"
        "src/movie.cc"
        "src/movie.h"
        "src/nevs.cc"
        "src/nevs.h"
        "src/obj_types.h"
        "src/object.cc"
        "src/object.h"
        "src/options.cc"
        "src/options.h"
        "src/palette.cc"
        "src/palette.h"
        "src/party_member.cc"
        "src/party_member.h"
        "src/pcx.cc"
        "src/pcx.h"
        "src/perk_defs.h"
        "src/perk.cc"
        "src/perk.h"
        "src/pipboy.cc"
        "src/pipboy.h"
        "src/proto_instance.cc"
        "src/proto_instance.h"
        "src/proto_types.h"
        "src/proto.cc"
        "src/proto.h"
        "src/queue.cc"
        "src/queue.h"
        "src/random.cc"
        "src/random.h"
        "src/reaction.cc"
        "src/reaction.h"
        "src/region.cc"
        "src/region.h"
        "src/scripts.cc"
        "src/scripts.h"
        "src/select_file_list.cc"
        "src/select_file_list.h"
        "src/selfrun.cc"
        "src/selfrun.h"
        "src/skill_defs.h"
        "src/skill.cc"
        "src/skill.h"
        "src/skilldex.cc"
        "src/skilldex.h"
        "src/sound_decoder.cc"
        "src/sound_decoder.h"
        "src/sound_effects_cache.cc"
        "src/sound_effects_cache.h"
        "src/sound_effects_list.cc"
        "src/sound_effects_list.h"
        "src/sound.cc"
        "src/sound.h"
        "src/stat_defs.h"
        "src/stat.cc"
        "src/stat.h"
        "src/string_parsers.cc"
        "src/string_parsers.h"
        "src/svga.cc"
        "src/svga.h"
        "src/text_font.cc"
        "src/text_font.h"
        "src/text_object.cc"
        "src/text_object.h"
        "src/tile.cc"
        "src/tile.h"
        "src/trait_defs.h"
        "src/trait.cc"
        "src/trait.h"
        "src/vcr.cc"
        "src/vcr.h"
        "src/version.cc"
        "src/version.h"
        "src/widget.cc"
        "src/widget.h"
        "src/win32.cc"
        "src/win32.h"
        "src/window_manager_private.cc"
        "src/window_manager_private.h"
        "src/window_manager.cc"
        "src/window_manager.h"
        "src/window.cc"
        "src/window.h"
        "src/word_wrap.cc"
        "src/word_wrap.h"
        "src/worldmap.cc"
        "src/worldmap.h"
        "src/xfile.cc"
        "src/xfile.h"
        "src/audio_engine.cc"
        "src/audio_engine.h"
        "src/delay.cc"
        "src/delay.h"
        "src/fps_limiter.cc"
        "src/fps_limiter.h"
        "src/platform_compat.cc"
        "src/platform_compat.h"
        "src/pointer_registry.cc"
        "src/pointer_registry.h"
        "src/preferences.cc"
        "src/preferences.h"
        "src/settings.cc"
        "src/settings.h"
        "src/sfall_config.cc"
        "src/sfall_config.h"
        "src/sfall_global_vars.cc"
        "src/sfall_global_vars.h"
        "src/sfall_global_scripts.cc"
        "src/sfall_global_scripts.h"
        "src/sfall_ini.cc"
        "src/sfall_ini.h"
        "src/sfall_kb_helpers.cc"
        "src/sfall_kb_helpers.h"
        "src/sfall_lists.cc"
        "src/sfall_lists.h"
        "src/sfall_metarules.cc"
        "src/sfall_metarules.h"
        "src/sfall_opcodes.cc"
        "src/sfall_opcodes.h"
        "src/sfall_arrays.cc"
        "src/sfall_arrays.h"
        "src/touch.cc"
        "src/touch.h"
        "third_party/fpattern/fpattern.c"
        "third_party/fpattern/fpattern.h"
        "src/logger.cc"
        "src/logger.h"
        "src/utils.cc"
        "src/utils.h"
    )
else()
    message(FATAL_ERROR "This CMakeLists.txt is configured to build for Switch only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(NX)
    set(CMAKE_PREFIX_PATH "/opt/devkitpro/portlibs/switch")

    include_directories(/opt/devkitpro/portlibs/switch/include)
    link_directories(/opt/devkitpro/portlibs/switch/lib /opt/devkitpro/libnx/lib)

    find_package(SDL2 REQUIRED)
    find_package(ZLIB REQUIRED)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -specs=/opt/devkitpro/libnx/switch.specs")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -specs=/opt/devkitpro/libnx/switch.specs")

    target_link_libraries(${EXECUTABLE_NAME}
        ${SDL2_LIBRARY}
        ${SDL2_TTF_LIBRARY}
        ${SDL2_MIXER_LIBRARY}
        ${SDL2_IMAGE_LIBRARY}
        ${ZLIB_LIBRARY}
        ${EGL_LIBRARY}
        ${NOUVEAU_LIBRARY}
        ${GLAPI_LIBRARY}
        ${GLES_LIBRARY}
        ${NX_LIBS}
        ${FREETYPE_LIBRARY}
        ${HARFBUZZ_LIBRARY}
        png
        bz2
        z
        m
    )

    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND /opt/devkitpro/tools/bin/nacptool --create "fallout2-ce" "Interplay / alexbatalov / ryandeering" "${F2_VERSION}" ${CMAKE_BINARY_DIR}/fallout2-ce.nacp
        COMMAND /opt/devkitpro/tools/bin/elf2nso ${EXECUTABLE_NAME} ${EXECUTABLE_NAME}.nso
        COMMAND /opt/devkitpro/tools/bin/elf2nro ${EXECUTABLE_NAME} ${EXECUTABLE_NAME}.nro --icon=${CMAKE_SOURCE_DIR}/assets/icon.jpg --nacp=${CMAKE_BINARY_DIR}/fallout2-ce.nacp
    )

endif()

