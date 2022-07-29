#include "game.h"

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "automap.h"
#include "character_editor.h"
#include "character_selector.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "core.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "electronic_registration.h"
#include "endgame.h"
#include "font_manager.h"
#include "game_config.h"
#include "game_dialog.h"
#include "game_memory.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "interface.h"
#include "inventory.h"
#include "item.h"
#include "loadsave.h"
#include "map.h"
#include "memory.h"
#include "movie.h"
#include "movie_effect.h"
#include "object.h"
#include "options.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "sfall_config.h"
#include "skill.h"
#include "skilldex.h"
#include "stat.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "trap.h"
#include "version.h"
#include "window_manager.h"
#include "world_map.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h> // access
#endif

#include <stdio.h>
#include <string.h>

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
static int _game_state_cur = 0;

// 0x5186BC
static bool gIsMapper = false;

// 0x5186C0
int* gGameGlobalVars = NULL;

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

// master.dat loading result
//
// 0x58E948
int _master_db_handle;

// critter.dat loading result
//
// 0x58E94C
int _critter_db_handle;

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

    gameConfigInit(isMapper, argc, argv);

    gIsMapper = isMapper;

    if (gameDbInit() == -1) {
        gameConfigExit(false);
        sfallConfigExit();
        return -1;
    }

    runElectronicRegistration();
    programWindowSetTitle(windowTitle);
    _initWindow(1, a4);
    paletteInit();

    char* language;
    if (configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language)) {
        if (compat_stricmp(language, FRENCH) == 0) {
            keyboardSetLayout(KEYBOARD_LAYOUT_FRENCH);
        } else if (compat_stricmp(language, GERMAN) == 0) {
            keyboardSetLayout(KEYBOARD_LAYOUT_GERMAN);
        } else if (compat_stricmp(language, ITALIAN) == 0) {
            keyboardSetLayout(KEYBOARD_LAYOUT_ITALIAN);
        } else if (compat_stricmp(language, SPANISH) == 0) {
            keyboardSetLayout(KEYBOARD_LAYOUT_SPANISH);
        }
    }

    // SFALL: Allow to skip splash screen
    int skipOpeningMovies = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, &skipOpeningMovies);

    if (!gIsMapper && skipOpeningMovies < 2) {
        showSplash();
    }

    _trap_init();

    interfaceFontsInit();
    fontManagerAdd(&gModernFontManager);
    fontSetCurrent(font);

    screenshotHandlerConfigure(KEY_F12, gameTakeScreenshot);
    pauseHandlerConfigure(-1, NULL);

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

    if (worldmapInit() != 0) {
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

    sprintf(path, "%s%s", asc_5186C8, "misc.msg");

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

    if (_init_options_menu() != 0) {
        debugPrint("Failed on init_options_menu\n");
        return -1;
    }

    debugPrint(">init_options_menu\n");

    if (endgameDeathEndingInit() != 0) {
        debugPrint("Failed on endgameDeathEndingInit");
        return -1;
    }

    debugPrint(">endgameDeathEndingInit\n");

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
    worldmapReset();
    partyMembersReset();
    characterEditorInit();
    pipboyReset();
    _ResetLoadSave();
    gameDialogReset();
    combatReset();
    _game_user_wants_to_quit = 0;
    automapReset();
    _init_options_menu();
}

// 0x442C34
void gameExit()
{
    debugPrint("\nGame Exit\n");

    tileDisable();
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
    worldmapExit();
    partyMembersExit();
    endgameDeathEndingExit();
    interfaceFontsExit();
    _trap_init();
    _windowClose();
    dbExit();
    gameConfigExit(true);
    sfallConfigExit();
}

// 0x442D44
int gameHandleKey(int eventCode, bool isInCombatMode)
{
    if (_game_state_cur == 5) {
        _gdialogSystemEnter();
    }

    if (eventCode == -1) {
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
                _combat(NULL);
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
                showDialogBox(title, NULL, 0, 192, 116, _colorTable[32328], NULL, _colorTable[32328], 0);
            } else {
                soundPlayFile("ib1p1xx1");
                pipboyOpen(false);
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
                showDialogBox(title, NULL, 0, 192, 116, _colorTable[32328], NULL, _colorTable[32328], 0);
            } else {
                soundPlayFile("ib1p1xx1");
                pipboyOpen(true);
            }
        }
        break;
    case KEY_HOME:
        if (gDude->elevation != gElevation) {
            mapSetElevation(gDude->elevation);
        }

        if (gIsMapper) {
            tileSetCenter(gDude->tile, TILE_SET_CENTER_FLAG_0x01);
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
        if (reg_anim_begin(0) == 0) {
            animationRegisterRotateCounterClockwise(gDude);
            reg_anim_end();
        }
        break;
    case KEY_DOT:
    case KEY_GREATER:
        if (reg_anim_begin(0) == 0) {
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
                sprintf(path, "%s%s", asc_5186C8, "editor.msg");

                if (messageListLoad(&messageList, path)) {
                    MessageListItem messageListItem;
                    messageListItem.num = 500 + month - 1;
                    if (messageListGetItem(&messageList, &messageListItem)) {
                        char* time = gameTimeGetTimeString();

                        char date[128];
                        sprintf(date, "%s: %d/%d %s", messageListItem.text, day, year, time);

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
            versionGetVersion(version);
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

    // TODO: Incomplete.

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
                    sprintf(formattedMessage, "You gained %d karma.", diff);
                } else {
                    sprintf(formattedMessage, "You lost %d karma.", -diff);
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
    return globalVarsRead("data\\vault13.gam", "GAME_GLOBAL_VARS:", &gGameGlobalVarsLength, &gGameGlobalVars);
}

// 0x443CE8
int globalVarsRead(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr)
{
    _inven_reset_dude();

    File* stream = fileOpen(path, "rt");
    if (stream == NULL) {
        return -1;
    }

    if (*variablesListLengthPtr != 0) {
        internal_free(*variablesListPtr);
        *variablesListPtr = NULL;
        *variablesListLengthPtr = 0;
    }

    char string[260];
    if (section != NULL) {
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
        if (semicolon != NULL) {
            *semicolon = '\0';
        }

        *variablesListLengthPtr = *variablesListLengthPtr + 1;
        *variablesListPtr = (int*)internal_realloc(*variablesListPtr, sizeof(int) * *variablesListLengthPtr);

        if (*variablesListPtr == NULL) {
            exit(1);
        }

        char* equals = strchr(string, '=');
        if (equals != NULL) {
            sscanf(equals + 1, "%d", *variablesListPtr + *variablesListLengthPtr - 1);
        } else {
            *variablesListPtr[*variablesListLengthPtr - 1] = 0;
        }
    }

    fileClose(stream);

    return 0;
}

// 0x443E2C
int _game_state()
{
    return _game_state_cur;
}

// 0x443E34
int _game_state_request(int a1)
{
    if (a1 == 0) {
        a1 = 1;
    } else if (a1 == 2) {
        a1 = 3;
    } else if (a1 == 4) {
        a1 = 5;
    }

    if (_game_state_cur != 4 || a1 != 5) {
        _game_state_cur = a1;
        return 0;
    }

    return -1;
}

// 0x443E90
void _game_state_update()
{
    int v0;

    v0 = _game_state_cur;
    switch (_game_state_cur) {
    case 1:
        v0 = 0;
        break;
    case 3:
        v0 = 2;
        break;
    case 5:
        v0 = 4;
    }

    _game_state_cur = v0;
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
    if (gGameGlobalVars != NULL) {
        internal_free(gGameGlobalVars);
        gGameGlobalVars = NULL;
    }
}

// 0x443F74
static void showHelp()
{
    bool isoWasEnabled = isoDisable();
    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool colorCycleWasEnabled = colorCycleEnabled();
    colorCycleDisable();

    int helpWindowX = (screenGetWidth() - HELP_SCREEN_WIDTH) / 2;
    int helpWindowY = (screenGetHeight() - HELP_SCREEN_HEIGHT) / 2;
    int win = windowCreate(helpWindowX, helpWindowY, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, 0, WINDOW_HIDDEN | WINDOW_FLAG_0x04);
    if (win != -1) {
        unsigned char* windowBuffer = windowGetBuffer(win);
        if (windowBuffer != NULL) {
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 297, 0, 0, 0);
            CacheEntry* backgroundHandle;
            unsigned char* backgroundData = artLockFrameData(backgroundFid, 0, 0, &backgroundHandle);
            if (backgroundData != NULL) {
                paletteSetEntries(gPaletteBlack);
                blitBufferToBuffer(backgroundData, HELP_SCREEN_WIDTH, HELP_SCREEN_HEIGHT, HELP_SCREEN_WIDTH, windowBuffer, HELP_SCREEN_WIDTH);
                artUnlock(backgroundHandle);
                windowUnhide(win);
                colorPaletteLoad("art\\intrface\\helpscrn.pal");
                paletteSetEntries(_cmap);

                while (_get_input() == -1 && _game_user_wants_to_quit == 0) {
                }

                while (mouseGetEvent() != 0) {
                    _get_input();
                }

                paletteSetEntries(gPaletteBlack);
            }
        }

        windowDestroy(win);
        colorPaletteLoad("color.pal");
        paletteSetEntries(_cmap);
    }

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
        rc = showDialogBox(messageListItem.text, 0, 0, 169, 117, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_YES_NO);
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
    int hashing;
    char* main_file_name;
    char* patch_file_name;
    int patch_index;
    char filename[COMPAT_MAX_PATH];

    hashing = 0;
    main_file_name = NULL;
    patch_file_name = NULL;

    if (configGetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_HASHING_KEY, &hashing)) {
        _db_enable_hash_table_();
    }

    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_DAT_KEY, &main_file_name);
    if (*main_file_name == '\0') {
        main_file_name = NULL;
    }

    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &patch_file_name);
    if (*patch_file_name == '\0') {
        patch_file_name = NULL;
    }

    _master_db_handle = dbOpen(main_file_name, 0, patch_file_name, 1);
    if (_master_db_handle == -1) {
        showMesageBox("Could not find the master datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_DAT_KEY, &main_file_name);
    if (*main_file_name == '\0') {
        main_file_name = NULL;
    }

    configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_CRITTER_PATCHES_KEY, &patch_file_name);
    if (*patch_file_name == '\0') {
        patch_file_name = NULL;
    }

    _critter_db_handle = dbOpen(main_file_name, 0, patch_file_name, 1);
    if (_critter_db_handle == -1) {
        showMesageBox("Could not find the critter datafile. Please make sure the FALLOUT CD is in the drive and that you are running FALLOUT from the directory you installed it to.");
        return -1;
    }

    for (patch_index = 0; patch_index < 1000; patch_index++) {
        sprintf(filename, "patch%03d.dat", patch_index);

        if (access(filename, 0) == 0) {
            dbOpen(filename, 0, NULL, 1);
        }
    }

    return 0;
}

// 0x444384
static void showSplash()
{
    int splash;
    configGetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, &splash);

    char path[64];
    char* language;
    if (configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_LANGUAGE_KEY, &language) && compat_stricmp(language, ENGLISH) != 0) {
        sprintf(path, "art\\%s\\splash\\", language);
    } else {
        sprintf(path, "art\\splash\\");
    }

    File* stream;
    for (int index = 0; index < SPLASH_COUNT; index++) {
        char filePath[64];
        sprintf(filePath, "%ssplash%d.rix", path, splash);
        stream = fileOpen(filePath, "rb");
        if (stream != NULL) {
            break;
        }

        splash++;

        if (splash >= SPLASH_COUNT) {
            splash = 0;
        }
    }

    if (stream == NULL) {
        return;
    }

    unsigned char* palette = (unsigned char*)internal_malloc(768);
    if (palette == NULL) {
        fileClose(stream);
        return;
    }

    unsigned char* data = (unsigned char*)internal_malloc(SPLASH_WIDTH * SPLASH_HEIGHT);
    if (data == NULL) {
        internal_free(palette);
        fileClose(stream);
        return;
    }

    paletteSetEntries(gPaletteBlack);
    fileSeek(stream, 10, SEEK_SET);
    fileRead(palette, 1, 768, stream);
    fileRead(data, 1, SPLASH_WIDTH * SPLASH_HEIGHT, stream);
    fileClose(stream);

    int splashWindowX = (screenGetWidth() - SPLASH_WIDTH) / 2;
    int splashWindowY = (screenGetHeight() - SPLASH_HEIGHT) / 2;
    _scr_blit(data, SPLASH_WIDTH, SPLASH_HEIGHT, 0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, splashWindowX, splashWindowY);
    paletteFadeTo(palette);

    internal_free(data);
    internal_free(palette);

    configSetInt(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_SPLASH_KEY, splash + 1);
}
