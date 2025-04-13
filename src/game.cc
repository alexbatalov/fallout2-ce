#include "game.h"

#include <stdio.h>
#include <string.h>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "automap.h"
#include "character_editor.h"
#include "character_selector.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "endgame.h"
#include "font_manager.h"
#include "game_dialog.h"
#include "game_memory.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "inventory.h"
#include "item.h"
#include "kb.h"
#include "loadsave.h"
#include "map.h"
#include "memory.h"
#include "mouse.h"
#include "movie.h"
#include "movie_effect.h"
#include "object.h"
#include "options.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_arrays.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_ini.h"
#include "sfall_lists.h"
#include "skill.h"
#include "skilldex.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "version.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "worldmap.h"

namespace fallout {

#define HELP_SCREEN_WIDTH (640)
#define HELP_SCREEN_HEIGHT (480)

#define SPLASH_WIDTH (640)
#define SPLASH_HEIGHT (480)
#define SPLASH_COUNT (10)

static int gameLoadGlobalVars();
static int gameTakeScreenshot(int width, int height, unsigned char* buffer, unsigned char* palette);
static void gameFreeGlobalVars();
static void showHelp();
static int gameDbInit();
static void showSplash();

// 0x501C9C
static char _aGame_0[] = "game\\";

// 0x5020B8
static char _aDec11199816543[] = VERSION_BUILD_TIME;

// 0x5186B4
static bool gGameUiDisabled = false;

// 0x5186B8
static int gGameState = GAME_STATE_0;

// 0x5186BC
static bool gIsMapper = false;

// 0x5186C0
int* gGameGlobalVars = nullptr;

// 0x5186C4
int gGameGlobalVarsLength = 0;

// 0x5186C8
const char* asc_5186C8 = _aGame_0;

// 0x5186CC
int _game_user_wants_to_quit = 0;

// misc.msg
//
// 0x58E940
MessageList gMiscMessageList;

// CE: Sonora folks like to store objects in global variables.
static void** gGameGlobalPointers = nullptr;

// 0x442580
int gameInitWithOptions(const char* windowTitle, bool isMapper, int font, int a4, int argc, char** argv)
{
    char path[COMPAT_MAX_PATH];

    if (gameMemoryInit() == -1) {
        return -1;
    }

    // Sfall config should be initialized before game config, since it can
    // override it's file name.
    sfallConfigInit(argc, argv);

    settingsInit(isMapper, argc, argv);

    gIsMapper = isMapper;

    if (gameDbInit() == -1) {
        settingsExit(false);
        sfallConfigExit();
        return -1;
    }

    // Message list repository is considered a specialized file manager, so
    // it should be initialized early in the process.
    messageListRepositoryInit();

    programWindowSetTitle(windowTitle);
    _initWindow(1, a4);
    paletteInit();

    const char* language = settings.system.language.c_str();
    if (compat_stricmp(language, FRENCH) == 0) {
        keyboardSetLayout(KEYBOARD_LAYOUT_FRENCH);
    } else if (compat_stricmp(language, GERMAN) == 0) {
        keyboardSetLayout(KEYBOARD_LAYOUT_GERMAN);
    } else if (compat_stricmp(language, ITALIAN) == 0) {
        keyboardSetLayout(KEYBOARD_LAYOUT_ITALIAN);
    } else if (compat_stricmp(language, SPANISH) == 0) {
        keyboardSetLayout(KEYBOARD_LAYOUT_SPANISH);
    }

    // SFALL: Allow to skip splash screen
    int skipOpeningMovies = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, &skipOpeningMovies);
    
    // load preferences before Splash screen to get proper brightness
    if (_init_options_menu() != 0) {
        debugPrint("Failed on init_options_menu\n");
        return -1;
    }

    debugPrint(">init_options_menu\n");

    if (!gIsMapper && skipOpeningMovies < 2) {
        showSplash();
    }

    // CE: Handle debug mode (exactly as seen in `mapper2.exe`).
    const char* debugMode = settings.debug.mode.c_str();
    if (compat_stricmp(debugMode, "environment") == 0) {
        _debug_register_env();
    } else if (compat_stricmp(debugMode, "screen") == 0) {
        _debug_register_screen();
    } else if (compat_stricmp(debugMode, "log") == 0) {
        _debug_register_log("debug.log", "wt");
    } else if (compat_stricmp(debugMode, "mono") == 0) {
        _debug_register_mono();
    } else if (compat_stricmp(debugMode, "gnw") == 0) {
        _debug_register_func(_win_debug);
    }

    interfaceFontsInit();
    fontManagerAdd(&gModernFontManager);
    fontSetCurrent(font);

    screenshotHandlerConfigure(KEY_F12, gameTakeScreenshot);

    tileDisable();

    randomInit();
    badwordsInit();
    skillsInit();
    statsInit();

    if (partyMembersInit() != 0) {
        debugPrint("Failed on partyMember_init\n");
        return -1;
    }

    perksInit();
    traitsInit();
    itemsInit();
    queueInit();
    critterInit();
    aiInit();
    _inven_reset_dude();

    if (gameSoundInit() != 0) {
        debugPrint("Sound initialization failed.\n");
    }

    debugPrint(">gsound_init\t");

    movieInit();
    debugPrint(">initMovie\t\t");

    if (gameMoviesInit() != 0) {
        debugPrint("Failed on gmovie_init\n");
        return -1;
    }

    debugPrint(">gmovie_init\t");

    if (movieEffectsInit() != 0) {
        debugPrint("Failed on moviefx_init\n");
        return -1;
    }

    debugPrint(">moviefx_init\t");

    if (isoInit() != 0) {
        debugPrint("Failed on iso_init\n");
        return -1;
    }

    debugPrint(">iso_init\t");

    if (gameMouseInit() != 0) {
        debugPrint("Failed on gmouse_init\n");
        return -1;
    }

    debugPrint(">gmouse_init\t");

    if (protoInit() != 0) {
        debugPrint("Failed on proto_init\n");
        return -1;
    }

    debugPrint(">proto_init\t");

    animationInit();
    debugPrint(">anim_init\t");

    if (scriptsInit() != 0) {
        debugPrint("Failed on scr_init\n");
        return -1;
    }

    debugPrint(">scr_init\t");

    if (gameLoadGlobalVars() != 0) {
        debugPrint("Failed on game_load_info\n");
        return -1;
    }

    debugPrint(">game_load_info\t");

    if (_scr_game_init() != 0) {
        debugPrint("Failed on scr_game_init\n");
        return -1;
    }

    debugPrint(">scr_game_init\t");

    if (wmWorldMap_init() != 0) {
        debugPrint("Failed on wmWorldMap_init\n");
        return -1;
    }

    debugPrint(">wmWorldMap_init\t");

    characterEditorInit();
    debugPrint(">CharEditInit\t");

    pipboyInit();
    debugPrint(">pip_init\t\t");

    _InitLoadSave();
    lsgInit();
    debugPrint(">InitLoadSave\t");

    if (gameDialogInit() != 0) {
        debugPrint("Failed on gdialog_init\n");
        return -1;
    }

    debugPrint(">gdialog_init\t");

    if (combatInit() != 0) {
        debugPrint("Failed on combat_init\n");
        return -1;
    }

    debugPrint(">combat_init\t");

    if (automapInit() != 0) {
        debugPrint("Failed on automap_init\n");
        return -1;
    }

    debugPrint(">automap_init\t");

    if (!messageListInit(&gMiscMessageList)) {
        debugPrint("Failed on message_init\n");
        return -1;
    }

    debugPrint(">message_init\t");

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "misc.msg");

    if (!messageListLoad(&gMiscMessageList, path)) {
        debugPrint("Failed on message_load\n");
        return -1;
    }

    debugPrint(">message_load\t");

    if (scriptsDisable() != 0) {
        debugPrint("Failed on scr_disable\n");
        return -1;
    }

    debugPrint(">scr_disable\t");

    if (endgameDeathEndingInit() != 0) {
        debugPrint("Failed on endgameDeathEndingInit");
        return -1;
    }

    debugPrint(">endgameDeathEndingInit\n");

    // SFALL
    premadeCharactersInit();

    if (!sfall_gl_vars_init()) {
        debugPrint("Failed on sfall_gl_vars_init");
        return -1;
    }

    if (!sfallListsInit()) {
        debugPrint("Failed on sfallListsInit");
        return -1;
    }

    if (!sfallArraysInit()) {
        debugPrint("Failed on sfallArraysInit");
        return -1;
    }

    if (!sfall_gl_scr_init()) {
        debugPrint("Failed on sfall_gl_scr_init");
        return -1;
    }

    char* customConfigBasePath;
    configGetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_INI_CONFIG_FOLDER, &customConfigBasePath);
    sfall_ini_set_base_path(customConfigBasePath);

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_MISC, &gMiscMessageList);

    return 0;
}

// 0x442B84
void gameReset()
{
    tileDisable();
    paletteReset();
    randomReset();
    skillsReset();
    statsReset();
    perksReset();
    traitsReset();
    itemsReset();
    queueExit();
    animationReset();
    lsgInit();
    critterReset();
    aiReset();
    _inven_reset_dude();
    gameSoundReset();
    _movieStop();
    movieEffectsReset();
    gameMoviesReset();
    isoReset();
    gameMouseReset();
    protoReset();
    _scr_reset();
    gameLoadGlobalVars();
    scriptsReset();
    wmWorldMap_reset();
    partyMembersReset();
    characterEditorInit();
    pipboyReset();
    _ResetLoadSave();
    gameDialogReset();
    combatReset();
    _game_user_wants_to_quit = 0;
    automapReset();
    _init_options_menu();

    // SFALL
    sfall_gl_vars_reset();
    sfallListsReset();
    messageListRepositoryReset();
    sfallArraysReset();
    sfall_gl_scr_reset();
}

// 0x442C34
void gameExit()
{
    debugPrint("\nGame Exit\n");

    // SFALL
    sfall_gl_scr_exit();
    sfallArraysExit();
    sfallListsExit();
    sfall_gl_vars_exit();
    premadeCharactersExit();

    tileDisable();
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_MISC, nullptr);
    messageListFree(&gMiscMessageList);
    combatExit();
    gameDialogExit();
    _scr_game_exit();

    // NOTE: Uninline.
    gameFreeGlobalVars();

    scriptsExit();
    animationExit();
    protoExit();
    gameMouseExit();
    isoExit();
    movieEffectsExit();
    movieExit();
    gameSoundExit();
    aiExit();
    critterExit();
    itemsExit();
    queueExit();
    perksExit();
    statsExit();
    skillsExit();
    traitsExit();
    randomExit();
    badwordsExit();
    automapExit();
    paletteExit();
    wmWorldMap_exit();
    partyMembersExit();
    endgameDeathEndingExit();
    interfaceFontsExit();
    _windowClose();
    messageListRepositoryExit();
    dbExit();
    settingsExit(true);
    sfallConfigExit();
}

// 0x442D44
int gameHandleKey(int eventCode, bool isInCombatMode)
{
    // NOTE: Uninline.
    if (gameGetState() == GAME_STATE_5) {
        _gdialogSystemEnter();
    }

    if (eventCode == -1) {
        if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            int dx = 0;
            if (wheelX > 0) {
                dx = 1;
            } else if (wheelX < 0) {
                dx = -1;
            }

            int dy = 0;
            if (wheelY > 0) {
                dy = -1;
            } else if (wheelY < 0) {
                dy = 1;
            }

            mapScroll(dx, dy);
        }
        return 0;
    }

    if (eventCode == -2) {
        int mouseState = mouseGetEvent();
        int mouseX;
        int mouseY;
        mouseGetPosition(&mouseX, &mouseY);

        if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
            if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
                if (mouseX == _scr_size.left || mouseX == _scr_size.right
                    || mouseY == _scr_size.top || mouseY == _scr_size.bottom) {
                    _gmouse_clicked_on_edge = true;
                } else {
                    _gmouse_clicked_on_edge = false;
                }
            }
        } else {
            if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                _gmouse_clicked_on_edge = false;
            }
        }

        _gmouse_handle_event(mouseX, mouseY, mouseState);
        return 0;
    }

    if (_gmouse_is_scrolling()) {
        return 0;
    }

    switch (eventCode) {
    case -20:
        if (interfaceBarEnabled()) {
            _intface_use_item();
        }
        break;
    case -2:
        if (1) {
            int mouseEvent = mouseGetEvent();
            int mouseX;
            int mouseY;
            mouseGetPosition(&mouseX, &mouseY);

            if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0) {
                    if (mouseX == _scr_size.left || mouseX == _scr_size.right
                        || mouseY == _scr_size.top || mouseY == _scr_size.bottom) {
                        _gmouse_clicked_on_edge = true;
                    } else {
                        _gmouse_clicked_on_edge = false;
                    }
                }
            } else {
                if ((mouseEvent & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
                    _gmouse_clicked_on_edge = false;
                }
            }

            _gmouse_handle_event(mouseX, mouseY, mouseEvent);
        }
        break;
    case KEY_CTRL_Q:
    case KEY_CTRL_X:
    case KEY_F10:
        soundPlayFile("ib1p1xx1");
        showQuitConfirmationDialog();
        break;
    case KEY_TAB:
        if (interfaceBarEnabled()
            && gPressedPhysicalKeys[SDL_SCANCODE_LALT] == 0
            && gPressedPhysicalKeys[SDL_SCANCODE_RALT] == 0) {
            soundPlayFile("ib1p1xx1");
            automapShow(true, false);
        }
        break;
    case KEY_CTRL_P:
        soundPlayFile("ib1p1xx1");
        showPause(false);
        break;
    case KEY_UPPERCASE_A:
    case KEY_LOWERCASE_A:
        if (interfaceBarEnabled()) {
            if (!isInCombatMode) {
                _combat(nullptr);
            }
        }
        break;
    case KEY_UPPERCASE_N:
    case KEY_LOWERCASE_N:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            interfaceCycleItemAction();
        }
        break;
    case KEY_UPPERCASE_M:
    case KEY_LOWERCASE_M:
        gameMouseCycleMode();
        break;
    case KEY_UPPERCASE_B:
    case KEY_LOWERCASE_B:
        // change active hand
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            interfaceBarSwapHands(true);
        }
        break;
    case KEY_UPPERCASE_C:
    case KEY_LOWERCASE_C:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            bool isoWasEnabled = isoDisable();
            characterEditorShow(false);
            if (isoWasEnabled) {
                isoEnable();
            }
        }
        break;
    case KEY_UPPERCASE_I:
    case KEY_LOWERCASE_I:
        // open inventory
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            inventoryOpen();
        }
        break;
    case KEY_ESCAPE:
    case KEY_UPPERCASE_O:
    case KEY_LOWERCASE_O:
        // options
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            showOptions();
        }
        break;
    case KEY_UPPERCASE_P:
    case KEY_LOWERCASE_P:
        // pipboy
        if (interfaceBarEnabled()) {
            if (isInCombatMode) {
                soundPlayFile("iisxxxx1");

                // Pipboy not available in combat!
                MessageListItem messageListItem;
                char title[128];
                strcpy(title, getmsg(&gMiscMessageList, &messageListItem, 7));
                showDialogBox(title, nullptr, 0, 192, 116, _colorTable[32328], nullptr, _colorTable[32328], 0);
            } else {
                soundPlayFile("ib1p1xx1");
                pipboyOpen(PIPBOY_OPEN_INTENT_UNSPECIFIED);
            }
        }
        break;
    case KEY_UPPERCASE_S:
    case KEY_LOWERCASE_S:
        // skilldex
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");

            int mode = -1;

            // NOTE: There is an `inc` for this value to build jump table which
            // is not needed.
            int rc = skilldexOpen();

            // Remap Skilldex result code to action.
            switch (rc) {
            case SKILLDEX_RC_ERROR:
                debugPrint("\n ** Error calling skilldex_select()! ** \n");
                break;
            case SKILLDEX_RC_SNEAK:
                _action_skill_use(SKILL_SNEAK);
                break;
            case SKILLDEX_RC_LOCKPICK:
                mode = GAME_MOUSE_MODE_USE_LOCKPICK;
                break;
            case SKILLDEX_RC_STEAL:
                mode = GAME_MOUSE_MODE_USE_STEAL;
                break;
            case SKILLDEX_RC_TRAPS:
                mode = GAME_MOUSE_MODE_USE_TRAPS;
                break;
            case SKILLDEX_RC_FIRST_AID:
                mode = GAME_MOUSE_MODE_USE_FIRST_AID;
                break;
            case SKILLDEX_RC_DOCTOR:
                mode = GAME_MOUSE_MODE_USE_DOCTOR;
                break;
            case SKILLDEX_RC_SCIENCE:
                mode = GAME_MOUSE_MODE_USE_SCIENCE;
                break;
            case SKILLDEX_RC_REPAIR:
                mode = GAME_MOUSE_MODE_USE_REPAIR;
                break;
            default:
                break;
            }

            if (mode != -1) {
                gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
                gameMouseSetMode(mode);
            }
        }
        break;
    case KEY_UPPERCASE_Z:
    case KEY_LOWERCASE_Z:
        if (interfaceBarEnabled()) {
            if (isInCombatMode) {
                soundPlayFile("iisxxxx1");

                // Pipboy not available in combat!
                MessageListItem messageListItem;
                char title[128];
                strcpy(title, getmsg(&gMiscMessageList, &messageListItem, 7));
                showDialogBox(title, nullptr, 0, 192, 116, _colorTable[32328], nullptr, _colorTable[32328], 0);
            } else {
                soundPlayFile("ib1p1xx1");
                pipboyOpen(PIPBOY_OPEN_INTENT_REST);
            }
        }
        break;
    case KEY_HOME:
        if (gDude->elevation != gElevation) {
            mapSetElevation(gDude->elevation);
        }

        if (gIsMapper) {
            tileSetCenter(gDude->tile, TILE_SET_CENTER_REFRESH_WINDOW);
        } else {
            _tile_scroll_to(gDude->tile, 2);
        }

        break;
    case KEY_1:
    case KEY_EXCLAMATION:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            _action_skill_use(SKILL_SNEAK);
        }
        break;
    case KEY_2:
    case KEY_AT:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_LOCKPICK);
        }
        break;
    case KEY_3:
    case KEY_NUMBER_SIGN:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_STEAL);
        }
        break;
    case KEY_4:
    case KEY_DOLLAR:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_TRAPS);
        }
        break;
    case KEY_5:
    case KEY_PERCENT:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_FIRST_AID);
        }
        break;
    case KEY_6:
    case KEY_CARET:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_DOCTOR);
        }
        break;
    case KEY_7:
    case KEY_AMPERSAND:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_SCIENCE);
        }
        break;
    case KEY_8:
    case KEY_ASTERISK:
        if (interfaceBarEnabled()) {
            soundPlayFile("ib1p1xx1");
            gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_USE_REPAIR);
        }
        break;
    case KEY_MINUS:
    case KEY_UNDERSCORE:
        brightnessDecrease();
        break;
    case KEY_EQUAL:
    case KEY_PLUS:
        brightnessIncrease();
        break;
    case KEY_COMMA:
    case KEY_LESS:
        if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) == 0) {
            animationRegisterRotateCounterClockwise(gDude);
            reg_anim_end();
        }
        break;
    case KEY_DOT:
    case KEY_GREATER:
        if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) == 0) {
            animationRegisterRotateClockwise(gDude);
            reg_anim_end();
        }
        break;
    case KEY_SLASH:
    case KEY_QUESTION:
        if (1) {
            soundPlayFile("ib1p1xx1");

            int month;
            int day;
            int year;
            gameTimeGetDate(&month, &day, &year);

            MessageList messageList;
            if (messageListInit(&messageList)) {
                char path[COMPAT_MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", asc_5186C8, "editor.msg");

                if (messageListLoad(&messageList, path)) {
                    MessageListItem messageListItem;
                    messageListItem.num = 500 + month - 1;
                    if (messageListGetItem(&messageList, &messageListItem)) {
                        char* time = gameTimeGetTimeString();

                        char date[128];
                        snprintf(date, sizeof(date), "%s: %d/%d %s", messageListItem.text, day, year, time);

                        displayMonitorAddMessage(date);
                    }
                }

                messageListFree(&messageList);
            }
        }
        break;
    case KEY_F1:
        soundPlayFile("ib1p1xx1");
        showHelp();
        break;
    case KEY_F2:
        gameSoundSetMasterVolume(gameSoundGetMasterVolume() - 2047);
        break;
    case KEY_F3:
        gameSoundSetMasterVolume(gameSoundGetMasterVolume() + 2047);
        break;
    case KEY_CTRL_S:
    case KEY_F4:
        soundPlayFile("ib1p1xx1");
        if (lsgSaveGame(1) == -1) {
            debugPrint("\n ** Error calling SaveGame()! **\n");
        }
        break;
    case KEY_CTRL_L:
    case KEY_F5:
        soundPlayFile("ib1p1xx1");
        if (lsgLoadGame(LOAD_SAVE_MODE_NORMAL) == -1) {
            debugPrint("\n ** Error calling LoadGame()! **\n");
        }
        break;
    case KEY_F6:
        if (1) {
            soundPlayFile("ib1p1xx1");

            int rc = lsgSaveGame(LOAD_SAVE_MODE_QUICK);
            if (rc == -1) {
                debugPrint("\n ** Error calling SaveGame()! **\n");
            } else if (rc == 1) {
                MessageListItem messageListItem;
                // Quick save game successfully saved.
                char* msg = getmsg(&gMiscMessageList, &messageListItem, 5);
                displayMonitorAddMessage(msg);
            }
        }
        break;
    case KEY_F7:
        if (1) {
            soundPlayFile("ib1p1xx1");

            int rc = lsgLoadGame(LOAD_SAVE_MODE_QUICK);
            if (rc == -1) {
                debugPrint("\n ** Error calling LoadGame()! **\n");
            } else if (rc == 1) {
                MessageListItem messageListItem;
                // Quick load game successfully loaded.
                char* msg = getmsg(&gMiscMessageList, &messageListItem, 4);
                displayMonitorAddMessage(msg);
            }
        }
        break;
    case KEY_CTRL_V:
        if (1) {
            soundPlayFile("ib1p1xx1");

            char version[VERSION_MAX];
            versionGetVersion(version, sizeof(version));
            displayMonitorAddMessage(version);
            displayMonitorAddMessage(_aDec11199816543);
        }
        break;
    case KEY_ARROW_LEFT:
        mapScroll(-1, 0);
        break;
    case KEY_ARROW_RIGHT:
        mapScroll(1, 0);
        break;
    case KEY_ARROW_UP:
        mapScroll(0, -1);
        break;
    case KEY_ARROW_DOWN:
        mapScroll(0, 1);
        break;
    }

    return 0;
}

// game_ui_disable
// 0x443BFC
void gameUiDisable(int a1)
{
    if (!gGameUiDisabled) {
        gameMouseObjectsHide();
        _gmouse_disable(a1);
        keyboardDisable();
        interfaceBarDisable();
        gGameUiDisabled = true;
    }
}

// game_ui_enable
// 0x443C30
void gameUiEnable()
{
    if (gGameUiDisabled) {
        interfaceBarEnable();
        keyboardEnable();
        keyboardReset();
        _gmouse_enable();
        gameMouseObjectsShow();
        gGameUiDisabled = false;
    }
}

// game_ui_is_disabled
// 0x443C60
bool gameUiIsDisabled()
{
    return gGameUiDisabled;
}

// 0x443C68
int gameGetGlobalVar(int var)
{
    if (var < 0 || var >= gGameGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference global var out of range: %d", var);
        return 0;
    }

    return gGameGlobalVars[var];
}

// 0x443C98
int gameSetGlobalVar(int var, int value)
{
    if (var < 0 || var >= gGameGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference global var out of range: %d", var);
        return -1;
    }

    // SFALL: Display karma changes.
    if (var == GVAR_PLAYER_REPUTATION) {
        bool shouldDisplayKarmaChanges = false;
        configGetBool(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_DISPLAY_KARMA_CHANGES_KEY, &shouldDisplayKarmaChanges);
        if (shouldDisplayKarmaChanges) {
            int diff = value - gGameGlobalVars[var];
            if (diff != 0) {
                char formattedMessage[80];
                if (diff > 0) {
                    snprintf(formattedMessage, sizeof(formattedMessage), "You gained %d karma.", diff);
                } else {
                    snprintf(formattedMessage, sizeof(formattedMessage), "You lost %d karma.", -diff);
                }
                displayMonitorAddMessage(formattedMessage);
            }
        }
    }

    gGameGlobalVars[var] = value;

    return 0;
}

// game_load_info
// 0x443CC8
static int gameLoadGlobalVars()
{
    if (globalVarsRead("data\\vault13.gam", "GAME_GLOBAL_VARS:", &gGameGlobalVarsLength, &gGameGlobalVars) != 0) {
        return -1;
    }

    gGameGlobalPointers = reinterpret_cast<void**>(internal_malloc(sizeof(*gGameGlobalPointers) * gGameGlobalVarsLength));
    if (gGameGlobalPointers == nullptr) {
        return -1;
    }

    memset(gGameGlobalPointers, 0, sizeof(*gGameGlobalPointers) * gGameGlobalVarsLength);

    return 0;
}

// 0x443CE8
int globalVarsRead(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr)
{
    _inven_reset_dude();

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    if (*variablesListLengthPtr != 0) {
        internal_free(*variablesListPtr);
        *variablesListPtr = nullptr;
        *variablesListLengthPtr = 0;
    }

    char string[260];
    if (section != nullptr) {
        while (fileReadString(string, 258, stream)) {
            if (strncmp(string, section, 16) == 0) {
                break;
            }
        }
    }

    while (fileReadString(string, 258, stream)) {
        if (string[0] == '\n') {
            continue;
        }

        if (string[0] == '/' && string[1] == '/') {
            continue;
        }

        char* semicolon = strchr(string, ';');
        if (semicolon != nullptr) {
            *semicolon = '\0';
        }

        *variablesListLengthPtr = *variablesListLengthPtr + 1;
        *variablesListPtr = (int*)internal_realloc(*variablesListPtr, sizeof(int) * *variablesListLengthPtr);

        if (*variablesListPtr == nullptr) {
            exit(1);
        }

        char* equals = strchr(string, '=');
        if (equals != nullptr) {
            sscanf(equals + 1, "%d", *variablesListPtr + *variablesListLengthPtr - 1);
        } else {
            *variablesListPtr[*variablesListLengthPtr - 1] = 0;
        }
    }

    fileClose(stream);

    return 0;
}

// 0x443E2C
int gameGetState()
{
    return gGameState;
}

// 0x443E34
int gameRequestState(int newGameState)
{
    switch (newGameState) {
    case GAME_STATE_0:
        newGameState = GAME_STATE_1;
        break;
    case GAME_STATE_2:
        newGameState = GAME_STATE_3;
        break;
    case GAME_STATE_4:
        newGameState = GAME_STATE_5;
        break;
    }

    if (gGameState == GAME_STATE_4 && newGameState == GAME_STATE_5) {
        return -1;
    }

    gGameState = newGameState;
    return 0;
}

// 0x443E90
void gameUpdateState()
{
    switch (gGameState) {
    case GAME_STATE_1:
        gGameState = GAME_STATE_0;
        break;
    case GAME_STATE_3:
        gGameState = GAME_STATE_2;
        break;
    case GAME_STATE_5:
        gGameState = GAME_STATE_4;
        break;
    }
}

// 0x443EF0
static int gameTakeScreenshot(int width, int height, unsigned char* buffer, unsigned char* palette)
{
    MessageListItem messageListItem;

    if (screenshotHandlerDefaultImpl(width, height, buffer, palette) != 0) {
        // Error saving screenshot.
        messageListItem.num = 8;
        if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }

        return -1;
    }

    // Saved screenshot.
    messageListItem.num = 3;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        displayMonitorAddMessage(messageListItem.text);
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x443F50
static void gameFreeGlobalVars()
{
    gGameGlobalVarsLength = 0;
    if (gGameGlobalVars != nullptr) {
        internal_free(gGameGlobalVars);
        gGameGlobalVars = nullptr;
    }

    if (gGameGlobalPointers != nullptr) {
        internal_free(gGameGlobalPointers);
        gGameGlobalPointers = nullptr;
    }
}

// 0x443F74
static void showHelp()
{
    ScopedGameMode gm(GameMode::kHelp); // Switch to Help game mode.

    bool isoWasEnabled = isoDisable();
    gameMouseObjectsHide();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool colorCycleWasEnabled = colorCycleEnabled();
    colorCycleDisable();

    // load the optional stretch setting from f2_res.ini
    int helpScreenStretchMode = 0;
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "HELP_SCRN_SIZE", &helpScreenStretchMode);
        }
        configFree(&config);
    }

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    // Create an invisible fullscreen overlay window to cover everything.
    int overlay = windowCreate(0, 0, screenWidth, screenHeight, 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);

    FrmImage helpFrmImage;
    int helpFid = buildFid(OBJ_TYPE_INTERFACE, 297, 0, 0, 0); // Load Help screen graphic (FID 297).
    if (!helpFrmImage.lock(helpFid)) {
        if (colorCycleWasEnabled) colorCycleEnable();
        gameMouseObjectsShow();
        if (isoWasEnabled) isoEnable();
        return;
    }

    unsigned char* helpData = helpFrmImage.getData();
    int helpWidth = HELP_SCREEN_WIDTH;
    int helpHeight = HELP_SCREEN_HEIGHT;

    unsigned char* bufferToBlit = helpData;
    int blitWidth = helpWidth;
    int blitHeight = helpHeight;

    // Check if stretching is needed (based on config or screen too small).
    if (helpScreenStretchMode != 0 || screenWidth < helpWidth || screenHeight < helpHeight) {
        int scaledWidth, scaledHeight;

        // Fullscreen stretch
        if (helpScreenStretchMode == 2) {
            scaledWidth = screenWidth;
            scaledHeight = screenHeight;
        } else {
            // Maintain aspect ratio
            if (screenHeight * helpWidth >= screenWidth * helpHeight) {
                scaledWidth = screenWidth;
                scaledHeight = screenWidth * helpHeight / helpWidth;
            } else {
                scaledWidth = screenHeight * helpWidth / helpHeight;
                scaledHeight = screenHeight;
            }
        }

        // Allocate memory for the scaled image.
        unsigned char* scaled = reinterpret_cast<unsigned char*>(internal_malloc((scaledWidth + 1) * (scaledHeight + 1)));
        if (scaled != nullptr) {
            // Stretch the original help image into the new buffer.
            blitBufferToBufferStretch(helpData, helpWidth, helpHeight, helpWidth, scaled, scaledWidth, scaledHeight, scaledWidth);

            // Fix rightmost column (copy last good pixel).
            for (int i = 0; i < scaledHeight; i++) {
                scaled[i * scaledWidth + (scaledWidth - 1)] = scaled[i * scaledWidth + (scaledWidth - 2)];
            }

            // Fix bottom row (copy last good pixel).
            for (int i = 0; i < scaledWidth; i++) {
                scaled[(scaledHeight - 1) * scaledWidth + i] = scaled[(scaledHeight - 2) * scaledWidth + i];
            }

            bufferToBlit = scaled;
            blitWidth = scaledWidth;
            blitHeight = scaledHeight;
        }
    }

    // Center the window on the screen.
    int x = (screenWidth - blitWidth) / 2;
    int y = (screenHeight - blitHeight) / 2;

    // Create window for the help screen.
    int win = windowCreate(x, y, blitWidth, blitHeight, 0, WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (win != -1) {
        unsigned char* windowBuffer = windowGetBuffer(win);
        if (windowBuffer != nullptr) {
            // Clear palette to black before displaying.
            paletteSetEntries(gPaletteBlack);

            // Copy the help image into the window.
            blitBufferToBuffer(bufferToBlit, blitWidth, blitHeight, blitWidth, windowBuffer, blitWidth);

            // Load the help screen's specific palette.
            colorPaletteLoad("art\\intrface\\helpscrn.pal");
            paletteSetEntries(_cmap);

            // Fill the overlay with the darkest palette color to hide everything behind.
            bufferFill(windowGetBuffer(overlay), screenWidth, screenHeight, screenWidth, intensityColorTable[_colorTable[0]][0]);

            windowShow(overlay);
            windowShow(win);

            while (inputGetInput() == -1 && _game_user_wants_to_quit == 0) {
                sharedFpsLimiter.mark();
                renderPresent();
                sharedFpsLimiter.throttle();
            }

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();
                inputGetInput();
                renderPresent();
                sharedFpsLimiter.throttle();
            }

            // Fade back to black.
            paletteSetEntries(gPaletteBlack);
        }

        // Cleanup
        windowDestroy(overlay);
        windowDestroy(win);
        colorPaletteLoad("color.pal");
        paletteSetEntries(_cmap);
    }

    // Free temporary scaled buffer if it was allocated.
    if (bufferToBlit != helpData) {
        internal_free(bufferToBlit);
    }

    // Restore previous state.
    if (colorCycleWasEnabled) {
        colorCycleEnable();
    }

    gameMouseObjectsShow();

    if (isoWasEnabled) {
        isoEnable();
    }
}

// 0x4440B8
int showQuitConfirmationDialog()
{
    bool isoWasEnabled = isoDisable();

    bool gameMouseWasVisible;
    if (isoWasEnabled) {
        gameMouseWasVisible = gameMouseObjectsIsVisible();
    } else {
        gameMouseWasVisible = false;
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    int oldCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int rc;

    // Are you sure you want to quit?
    MessageListItem messageListItem;
    messageListItem.num = 0;
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        rc = showDialogBox(messageListItem.text, nullptr, 0, 169, 117, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_YES_NO);
        if (rc != 0) {
            _game_user_wants_to_quit = 2;
        }
    } else {
        rc = -1;
    }

    gameMouseSetCursor(oldCursor);

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    if (isoWasEnabled) {
        isoEnable();
    }

    return rc;
}

// 0x44418C
static int gameDbInit()
{
    const char* main_file_name;
    const char* patch_file_name;
    int patch_index;
    char filename[COMPAT_MAX_PATH];

    main_file_name = nullptr;
    patch_file_name = nullptr;

    main_file_name = settings.system.master_dat_path.c_str();
    if (*main_file_name == '\0') {
        main_file_name = nullptr;
    }

    patch_file_name = settings.system.master_patches_path.c_str();
    if (*patch_file_name == '\0') {
        patch_file_name = nullptr;
    }

    int master_db_handle = dbOpen(main_file_name, 0, patch_file_name, 1);
    if (master_db_handle == -1) {
        showMesageBox("Could not find the master datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    main_file_name = settings.system.critter_dat_path.c_str();
    if (*main_file_name == '\0') {
        main_file_name = nullptr;
    }

    patch_file_name = settings.system.critter_patches_path.c_str();
    if (*patch_file_name == '\0') {
        patch_file_name = nullptr;
    }

    int critter_db_handle = dbOpen(main_file_name, 0, patch_file_name, 1);
    if (critter_db_handle == -1) {
        showMesageBox("Could not find the critter datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    // SFALL: custom patch file name.
    char* path_file_name_template = nullptr;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PATCH_FILE, &path_file_name_template);
    if (path_file_name_template == nullptr || *path_file_name_template == '\0') {
        path_file_name_template = "patch%03d.dat";
    }

    for (patch_index = 0; patch_index < 1000; patch_index++) {
        snprintf(filename, sizeof(filename), path_file_name_template, patch_index);

        if (compat_access(filename, 0) == 0) {
            dbOpen(filename, 0, nullptr, 1);
        }
    }

    if (compat_access("f2_res.dat", 0) == 0) {
        dbOpen("f2_res.dat", 0, nullptr, 1);
    }

    return 0;
}

// 0x444384
static void showSplash()
{
    int splashIndex = settings.system.splash;

    // path to local splash folder
    char path[64];
    const char* language = settings.system.language.c_str();
    if (compat_stricmp(language, ENGLISH) != 0) {
        snprintf(path, sizeof(path), "art\\%s\\splash\\", language);
    } else {
        snprintf(path, sizeof(path), "art\\splash\\");
    }

    // open a splash file, cycle through SPLASH_COUNT entries
    File* stream = nullptr;
    for (int attempt = 0; attempt < SPLASH_COUNT; attempt++) {
        char filePath[64];
        snprintf(filePath, sizeof(filePath), "%ssplash%d.rix", path, splashIndex);
        stream = fileOpen(filePath, "rb");
        if (stream != nullptr) {
            break;
        }

        splashIndex++;
        if (splashIndex >= SPLASH_COUNT) {
            splashIndex = 0;
        }
    }

    if (stream == nullptr) {
        return;
    }

    // Allocate memory for the palette
    unsigned char* palette = reinterpret_cast<unsigned char*>(internal_malloc(768));
    if (palette == nullptr) {
        fileClose(stream);
        return;
    }

    // Verify RIX image version (needed?)
    int version;
    fileReadInt32(stream, &version);
    if (version != 'RIX3') {
        fileClose(stream);
        internal_free(palette);
        return;
    }

    // Read image dimensions
    short splashWidth, splashHeight;
    fileRead(&splashWidth, sizeof(splashWidth), 1, stream);
    fileRead(&splashHeight, sizeof(splashHeight), 1, stream);

    // Allocate memory for the image pixel data
    unsigned char* splashData = reinterpret_cast<unsigned char*>(internal_malloc(splashWidth * splashHeight));
    if (splashData == nullptr) {
        internal_free(palette);
        fileClose(stream);
        return;
    }

    // Load palette and image data
    paletteSetEntries(gPaletteBlack);
    fileSeek(stream, 10, SEEK_SET);
    fileRead(palette, 1, 768, stream);
    fileRead(splashData, 1, splashWidth * splashHeight, stream);
    fileClose(stream);

    int splashScreenStretchMode = 0;

    // Load the optional stretch mode from f2_res.ini
    Config config;
    if (configInit(&config)) {
        if (configRead(&config, "f2_res.ini", false)) {
            configGetInt(&config, "STATIC_SCREENS", "SPLASH_SCRN_SIZE", &splashScreenStretchMode);
        }
        configFree(&config);
    }

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();

    // Stretch image if required or if it doesn't fit the screen
    if (splashScreenStretchMode != 0 || screenWidth < splashWidth || screenHeight < splashHeight) {
        int scaledWidth, scaledHeight;

        if (splashScreenStretchMode == 2) {
            // Fullscreen stretch
            scaledWidth = screenWidth;
            scaledHeight = screenHeight;
        } else {
            // Maintianing aspect ratio
            if (screenHeight * splashWidth >= screenWidth * splashHeight) {
                scaledWidth = screenWidth;
                scaledHeight = screenWidth * splashHeight / splashWidth;
            } else {
                scaledWidth = screenHeight * splashWidth / splashHeight;
                scaledHeight = screenHeight;
            }
        }

        unsigned char* scaled = reinterpret_cast<unsigned char*>(internal_malloc((scaledWidth) * (scaledHeight)));
        if (scaled != nullptr) {
            // Stretch the image buffer to fit target resolution
            blitBufferToBufferStretch(splashData, splashWidth, splashHeight, splashWidth, scaled, scaledWidth, scaledHeight, scaledWidth);

            int x = (screenWidth > scaledWidth) ? (screenWidth - scaledWidth) / 2 : 0;
            int y = (screenHeight > scaledHeight) ? (screenHeight - scaledHeight) / 2 : 0;

            // Fix rightmost column (copy last good pixel).
            for (int i = 0; i < scaledHeight; i++) {
                scaled[i * scaledWidth + (scaledWidth - 1)] = scaled[i * scaledWidth + (scaledWidth - 2)];
            }
            
            // Fix bottom row (copy last good pixel).
            for (int i = 0; i < scaledWidth; i++) {
                scaled[(scaledHeight - 1) * scaledWidth + i] = scaled[(scaledHeight - 2) * scaledWidth + i];
            }

            _scr_blit(scaled, scaledWidth, scaledHeight, 0, 0, scaledWidth, scaledHeight, x, y);
            paletteFadeTo(palette);

            internal_free(scaled);
        }
    } else {
        // Center image without scaling
        int x = (screenWidth - splashWidth) / 2;
        int y = (screenHeight - splashHeight) / 2;
        _scr_blit(splashData, splashWidth, splashHeight, 0, 0, splashWidth, splashHeight, x, y);
        paletteFadeTo(palette);
    }

    // Clean up
    internal_free(splashData);
    internal_free(palette);

    // Save index for next splash image
    settings.system.splash = splashIndex + 1;
}


int gameShowDeathDialog(const char* message)
{
    bool isoWasEnabled = isoDisable();

    bool gameMouseWasVisible;
    if (isoWasEnabled) {
        gameMouseWasVisible = gameMouseObjectsIsVisible();
    } else {
        gameMouseWasVisible = false;
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    int oldCursor = gameMouseGetCursor();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int oldUserWantsToQuit = _game_user_wants_to_quit;
    _game_user_wants_to_quit = 0;

    int rc = showDialogBox(message, nullptr, 0, 169, 117, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

    _game_user_wants_to_quit = oldUserWantsToQuit;

    gameMouseSetCursor(oldCursor);

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    if (isoWasEnabled) {
        isoEnable();
    }

    return rc;
}

void* gameGetGlobalPointer(int var)
{
    if (var < 0 || var >= gGameGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference global pointer out of range: %d", var);
        return nullptr;
    }

    return gGameGlobalPointers[var];
}

int gameSetGlobalPointer(int var, void* value)
{
    if (var < 0 || var >= gGameGlobalVarsLength) {
        debugPrint("ERROR: attempt to reference global var out of range: %d", var);
        return -1;
    }

    gGameGlobalPointers[var] = value;

    return 0;
}

int GameMode::currentGameMode = 0;

void GameMode::enterGameMode(int gameMode)
{
    currentGameMode |= gameMode;
}

void GameMode::exitGameMode(int gameMode)
{
    currentGameMode &= ~gameMode;
}

bool GameMode::isInGameMode(int gameMode)
{
    return (currentGameMode & gameMode) != 0;
}

ScopedGameMode::ScopedGameMode(int gameMode)
{
    this->gameMode = gameMode;
    GameMode::enterGameMode(gameMode);
}

ScopedGameMode::~ScopedGameMode()
{
    GameMode::exitGameMode(gameMode);
}

} // namespace fallout
