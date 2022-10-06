#include "settings.h"

namespace fallout {

static void settingsFromConfig();
static void settingsToConfig();
static void settingsRead(const char* section, const char* key, std::string& value);
static void settingsRead(const char* section, const char* key, int& value);
static void settingsRead(const char* section, const char* key, bool& value);
static void settingsRead(const char* section, const char* key, double& value);
static void settingsWrite(const char* section, const char* key, std::string& value);
static void settingsWrite(const char* section, const char* key, int& value);
static void settingsWrite(const char* section, const char* key, bool& value);
static void settingsWrite(const char* section, const char* key, double& value);

Settings settings;

bool settingsInit(bool isMapper, int argc, char** argv)
{
    if (!gameConfigInit(isMapper, argc, argv)) {
        return false;
    }

    settingsFromConfig();

    return true;
}

bool settingsSave()
{
    settingsToConfig();
    return gameConfigSave();
}

bool settingsExit(bool shouldSave)
{
    if (shouldSave) {
        settingsToConfig();
    }

    return gameConfigExit(shouldSave);
}

static void settingsFromConfig()
{
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_EXECUTABLE_KEY, settings.system.executable);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_DAT_KEY, settings.system.master_dat_path);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, settings.system.master_patches_path);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_DAT_KEY, settings.system.critter_dat_path);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_PATCHES_KEY, settings.system.critter_patches_path);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, settings.system.language);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SCROLL_LOCK_KEY, settings.system.scroll_lock);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_INTERRUPT_WALK_KEY, settings.system.interrupt_walk);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_ART_CACHE_SIZE_KEY, settings.system.art_cache_size);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_COLOR_CYCLING_KEY, settings.system.color_cycling);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CYCLE_SPEED_FACTOR_KEY, settings.system.cycle_speed_factor);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_HASHING_KEY, settings.system.hashing);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, settings.system.splash);
    settingsRead(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_FREE_SPACE_KEY, settings.system.free_space);

    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, settings.preferences.game_difficulty);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, settings.preferences.combat_difficulty);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, settings.preferences.violence_level);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, settings.preferences.target_highlight);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, settings.preferences.item_highlight);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_LOOKS_KEY, settings.preferences.combat_looks);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, settings.preferences.combat_messages);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, settings.preferences.combat_taunts);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, settings.preferences.language_filter);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, settings.preferences.running);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, settings.preferences.subtitles);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, settings.preferences.combat_speed);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEED_KEY, settings.preferences.player_speedup);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, settings.preferences.text_base_delay);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, settings.preferences.text_line_delay);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, settings.preferences.brightness);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, settings.preferences.mouse_sensitivity);
    settingsRead(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, settings.preferences.running_burning_guy);

    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_INITIALIZE_KEY, settings.sound.initialize);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_KEY, settings.sound.debug);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_SFXC_KEY, settings.sound.debug_sfxc);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEVICE_KEY, settings.sound.device);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_PORT_KEY, settings.sound.port);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_IRQ_KEY, settings.sound.irq);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DMA_KEY, settings.sound.dma);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SOUNDS_KEY, settings.sound.sounds);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_KEY, settings.sound.music);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_KEY, settings.sound.speech);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, settings.sound.master_volume);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, settings.sound.music_volume);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, settings.sound.sndfx_volume);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, settings.sound.speech_volume);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_CACHE_SIZE_KEY, settings.sound.cache_size);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH1_KEY, settings.sound.music_path1);
    settingsRead(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH2_KEY, settings.sound.music_path2);

    settingsRead(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_MODE_KEY, settings.debug.mode);
    settingsRead(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_TILE_NUM_KEY, settings.debug.show_tile_num);
    settingsRead(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, settings.debug.show_script_messages);
    settingsRead(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_LOAD_INFO_KEY, settings.debug.show_load_info);
    settingsRead(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_OUTPUT_MAP_DATA_INFO_KEY, settings.debug.output_map_data_info);

    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_OVERRIDE_LIBRARIAN_KEY, settings.mapper.override_librarian);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_LIBRARIAN_KEY, settings.mapper.librarian);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_USE_ART_NOT_PROTOS_KEY, settings.mapper.user_art_not_protos);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_REBUILD_PROTOS_KEY, settings.mapper.rebuild_protos);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_OBJECTS_KEY, settings.mapper.fix_map_objects);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_INVENTORY_KEY, settings.mapper.fix_map_inventory);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_IGNORE_REBUILD_ERRORS_KEY, settings.mapper.rebuild_protos);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SHOW_PID_NUMBERS_KEY, settings.mapper.show_pid_numbers);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SAVE_TEXT_MAPS_KEY, settings.mapper.save_text_maps);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_RUN_MAPPER_AS_GAME_KEY, settings.mapper.run_mapper_as_game);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_DEFAULT_F8_AS_GAME_KEY, settings.mapper.default_f8_as_game);
    settingsRead(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SORT_SCRIPT_LIST_KEY, settings.mapper.sort_script_list);
}

static void settingsToConfig()
{
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_EXECUTABLE_KEY, settings.system.executable);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_DAT_KEY, settings.system.master_dat_path);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, settings.system.master_patches_path);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_DAT_KEY, settings.system.critter_dat_path);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_PATCHES_KEY, settings.system.critter_patches_path);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, settings.system.language);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SCROLL_LOCK_KEY, settings.system.scroll_lock);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_INTERRUPT_WALK_KEY, settings.system.interrupt_walk);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_ART_CACHE_SIZE_KEY, settings.system.art_cache_size);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_COLOR_CYCLING_KEY, settings.system.color_cycling);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CYCLE_SPEED_FACTOR_KEY, settings.system.cycle_speed_factor);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_HASHING_KEY, settings.system.hashing);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, settings.system.splash);
    settingsWrite(GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_FREE_SPACE_KEY, settings.system.free_space);

    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, settings.preferences.game_difficulty);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, settings.preferences.combat_difficulty);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, settings.preferences.violence_level);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TARGET_HIGHLIGHT_KEY, settings.preferences.target_highlight);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, settings.preferences.item_highlight);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_LOOKS_KEY, settings.preferences.combat_looks);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_MESSAGES_KEY, settings.preferences.combat_messages);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_TAUNTS_KEY, settings.preferences.combat_taunts);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_LANGUAGE_FILTER_KEY, settings.preferences.language_filter);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, settings.preferences.running);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_SUBTITLES_KEY, settings.preferences.subtitles);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_SPEED_KEY, settings.preferences.combat_speed);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_PLAYER_SPEED_KEY, settings.preferences.player_speedup);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_BASE_DELAY_KEY, settings.preferences.text_base_delay);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_TEXT_LINE_DELAY_KEY, settings.preferences.text_line_delay);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_BRIGHTNESS_KEY, settings.preferences.brightness);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_MOUSE_SENSITIVITY_KEY, settings.preferences.mouse_sensitivity);
    settingsWrite(GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, settings.preferences.running_burning_guy);

    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_INITIALIZE_KEY, settings.sound.initialize);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_KEY, settings.sound.debug);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEBUG_SFXC_KEY, settings.sound.debug_sfxc);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DEVICE_KEY, settings.sound.device);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_PORT_KEY, settings.sound.port);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_IRQ_KEY, settings.sound.irq);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_DMA_KEY, settings.sound.dma);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SOUNDS_KEY, settings.sound.sounds);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_KEY, settings.sound.music);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_KEY, settings.sound.speech);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MASTER_VOLUME_KEY, settings.sound.master_volume);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_VOLUME_KEY, settings.sound.music_volume);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SNDFX_VOLUME_KEY, settings.sound.sndfx_volume);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_SPEECH_VOLUME_KEY, settings.sound.speech_volume);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_CACHE_SIZE_KEY, settings.sound.cache_size);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH1_KEY, settings.sound.music_path1);
    settingsWrite(GAME_CONFIG_SOUND_KEY, GAME_CONFIG_MUSIC_PATH2_KEY, settings.sound.music_path2);

    settingsWrite(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_MODE_KEY, settings.debug.mode);
    settingsWrite(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_TILE_NUM_KEY, settings.debug.show_tile_num);
    settingsWrite(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, settings.debug.show_script_messages);
    settingsWrite(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_LOAD_INFO_KEY, settings.debug.show_load_info);
    settingsWrite(GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_OUTPUT_MAP_DATA_INFO_KEY, settings.debug.output_map_data_info);

    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_OVERRIDE_LIBRARIAN_KEY, settings.mapper.override_librarian);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_LIBRARIAN_KEY, settings.mapper.librarian);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_USE_ART_NOT_PROTOS_KEY, settings.mapper.user_art_not_protos);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_REBUILD_PROTOS_KEY, settings.mapper.rebuild_protos);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_OBJECTS_KEY, settings.mapper.fix_map_objects);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_FIX_MAP_INVENTORY_KEY, settings.mapper.fix_map_inventory);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_IGNORE_REBUILD_ERRORS_KEY, settings.mapper.rebuild_protos);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SHOW_PID_NUMBERS_KEY, settings.mapper.show_pid_numbers);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SAVE_TEXT_MAPS_KEY, settings.mapper.save_text_maps);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_RUN_MAPPER_AS_GAME_KEY, settings.mapper.run_mapper_as_game);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_DEFAULT_F8_AS_GAME_KEY, settings.mapper.default_f8_as_game);
    settingsWrite(GAME_CONFIG_MAPPER_KEY, GAME_CONFIG_SORT_SCRIPT_LIST_KEY, settings.mapper.sort_script_list);
}

static void settingsRead(const char* section, const char* key, std::string& value)
{
    char* v;
    if (configGetString(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, int& value)
{
    int v;
    if (configGetInt(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, bool& value)
{
    bool v;
    if (configGetBool(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsRead(const char* section, const char* key, double& value)
{
    double v;
    if (configGetDouble(&gGameConfig, section, key, &v)) {
        value = v;
    }
}

static void settingsWrite(const char* section, const char* key, std::string& value)
{
    configSetString(&gGameConfig, section, key, value.c_str());
}

static void settingsWrite(const char* section, const char* key, int& value)
{
    configSetInt(&gGameConfig, section, key, value);
}

static void settingsWrite(const char* section, const char* key, bool& value)
{
    configSetBool(&gGameConfig, section, key, value);
}

static void settingsWrite(const char* section, const char* key, double& value)
{
    configSetDouble(&gGameConfig, section, key, value);
}

} // namespace fallout
