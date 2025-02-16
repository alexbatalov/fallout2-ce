#include "loadsave.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include "art.h"
#include "automap.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "delay.h"
#include "display_monitor.h"
#include "draw.h"
#include "file_utils.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
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
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "version.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

#define LOAD_SAVE_SIGNATURE "FALLOUT SAVE FILE"
#define LOAD_SAVE_DESCRIPTION_LENGTH 30
#define LOAD_SAVE_HANDLER_COUNT 27

#define LSGAME_MSG_NAME "LSGAME.MSG"

#define LS_WINDOW_WIDTH 640
#define LS_WINDOW_HEIGHT 480

#define LS_PREVIEW_WIDTH 224
#define LS_PREVIEW_HEIGHT 133
#define LS_PREVIEW_SIZE ((LS_PREVIEW_WIDTH) * (LS_PREVIEW_HEIGHT))

#define LS_COMMENT_WINDOW_X 169
#define LS_COMMENT_WINDOW_Y 116

// NOTE: The following are "normalized" path components for "proto/critters" and
// "proto/items". The original code does not use uniform case for them (as
// opposed to other path components like MAPS, SAVE.DAT, etc). It does not have
// effect on Windows, but it's important on Linux and Mac, where filesystem is
// case-sensitive. Lowercase is preferred as it is used in other parts of the
// codebase (see `protoInit`, `gArtListDescriptions`).

#define PROTO_DIR_NAME "proto"
#define CRITTERS_DIR_NAME "critters"
#define ITEMS_DIR_NAME "items"
#define PROTO_FILE_EXT "pro"

typedef int LoadGameHandler(File* stream);
typedef int SaveGameHandler(File* stream);

typedef enum LoadSaveWindowType {
    LOAD_SAVE_WINDOW_TYPE_SAVE_GAME,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT,
} LoadSaveWindowType;

typedef enum LoadSaveSlotState {
    SLOT_STATE_EMPTY,
    SLOT_STATE_OCCUPIED,
    SLOT_STATE_ERROR,
    SLOT_STATE_UNSUPPORTED_VERSION,
} LoadSaveSlotState;

typedef enum LoadSaveScrollDirection {
    LOAD_SAVE_SCROLL_DIRECTION_NONE,
    LOAD_SAVE_SCROLL_DIRECTION_UP,
    LOAD_SAVE_SCROLL_DIRECTION_DOWN,
} LoadSaveScrollDirection;

typedef struct LoadSaveSlotData {
    char signature[24];
    short versionMinor;
    short versionMajor;
    // TODO: The type is probably char, but it's read with the same function as
    // reading unsigned chars, which in turn probably result of collapsing
    // reading functions.
    unsigned char versionRelease;
    char characterName[32];
    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    short fileMonth;
    short fileDay;
    short fileYear;
    int fileTime;
    short gameMonth;
    short gameDay;
    short gameYear;
    unsigned int gameTime;
    short elevation;
    short map;
    char fileName[16];
} LoadSaveSlotData;

typedef enum LoadSaveFrm {
    LOAD_SAVE_FRM_BACKGROUND,
    LOAD_SAVE_FRM_BOX,
    LOAD_SAVE_FRM_PREVIEW_COVER,
    LOAD_SAVE_FRM_RED_BUTTON_PRESSED,
    LOAD_SAVE_FRM_RED_BUTTON_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_PRESSED,
    LOAD_SAVE_FRM_ARROW_UP_NORMAL,
    LOAD_SAVE_FRM_ARROW_UP_PRESSED,
    LOAD_SAVE_FRM_COUNT,
} LoadSaveFrm;

static int _QuickSnapShot();
static int lsgWindowInit(int windowType);
static int lsgWindowFree(int windowType);
static int lsgPerformSaveGame();
static int lsgLoadGameInSlot(int slot);
static int lsgSaveHeaderInSlot(int slot);
static int lsgLoadHeaderInSlot(int slot);
static int _GetSlotList();
static void _ShowSlotList(int windowType);
static void _DrawInfoBox(int slot);
static int _LoadTumbSlot(int slot);
static int _GetComment(int slot);
static int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
static int _DummyFunc(File* stream);
static int _PrepLoad(File* stream);
static int _EndLoad(File* stream);
static int _GameMap2Slot(File* stream);
static int _SlotMap2Game(File* stream);
static int _mygets(char* dest, File* stream);
static int _copy_file(const char* existingFileName, const char* newFileName);
static int _SaveBackup();
static int _RestoreSave();
static int _LoadObjDudeCid(File* stream);
static int _SaveObjDudeCid(File* stream);
static int _EraseSave();

// 0x47B7C0
static const int gLoadSaveFrmIds[LOAD_SAVE_FRM_COUNT] = {
    237, // lsgame.frm - load/save game
    238, // lsgbox.frm - load/save game
    239, // lscover.frm - load/save game
    9, // lilreddn.frm - little red button down
    8, // lilredup.frm - little red button up
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// 0x5193B8
static int _slot_cursor = 0;

// 0x5193BC
static bool _quick_done = false;

// 0x5193C0
static bool gLoadSaveWindowIsoWasEnabled = false;

// 0x5193C4
static int _map_backup_count = -1;

// 0x5193C8
static bool _automap_db_flag = false;

// 0x5193CC
static const char* _patches = nullptr;

// 0x5193EC
static SaveGameHandler* _master_save_list[LOAD_SAVE_HANDLER_COUNT] = {
    _DummyFunc,
    _SaveObjDudeCid,
    scriptsSaveGameGlobalVars,
    _GameMap2Slot,
    scriptsSaveGameGlobalVars,
    _obj_save_dude,
    critterSave,
    killsSave,
    skillsSave,
    randomSave,
    perksSave,
    combatSave,
    aiSave,
    statsSave,
    itemsSave,
    traitsSave,
    automapSave,
    preferencesSave,
    characterEditorSave,
    wmWorldMap_save,
    pipboySave,
    gameMoviesSave,
    skillsUsageSave,
    partyMembersSave,
    queueSave,
    interfaceSave,
    _DummyFunc,
};

// 0x519458
static LoadGameHandler* _master_load_list[LOAD_SAVE_HANDLER_COUNT] = {
    _PrepLoad,
    _LoadObjDudeCid,
    scriptsLoadGameGlobalVars,
    _SlotMap2Game,
    scriptsSkipGameGlobalVars,
    _obj_load_dude,
    critterLoad,
    killsLoad,
    skillsLoad,
    randomLoad,
    perksLoad,
    combatLoad,
    aiLoad,
    statsLoad,
    itemsLoad,
    traitsLoad,
    automapLoad,
    preferencesLoad,
    characterEditorLoad,
    wmWorldMap_load,
    pipboyLoad,
    gameMoviesLoad,
    skillsUsageLoad,
    partyMembersLoad,
    queueLoad,
    interfaceLoad,
    _EndLoad,
};

// 0x5194C4
static bool _loadingGame = false;

// lsgame.msg
//
// 0x613D28
static MessageList gLoadSaveMessageList;

// 0x613D30
static LoadSaveSlotData _LSData[10];

// 0x614280
static int _LSstatus[10];

// 0x6142A8
static unsigned char* _thumbnail_image;

// 0x6142AC
static unsigned char* _snapshotBuf;

// 0x6142B0
static MessageListItem gLoadSaveMessageListItem;

// 0x6142C0
static int _dbleclkcntr;

// 0x6142C4
static int gLoadSaveWindow;

// 0x6142EC
static unsigned char* _snapshot;

// 0x6142F0
static char _str2[COMPAT_MAX_PATH];

// 0x6143F4
static char _str0[COMPAT_MAX_PATH];

// 0x6144F8
static char _str1[COMPAT_MAX_PATH];

// 0x6145FC
static char _str[COMPAT_MAX_PATH];

// 0x614700
static unsigned char* gLoadSaveWindowBuffer;

// 0x614704
static char _gmpath[COMPAT_MAX_PATH];

// 0x614808
static File* _flptr;

// 0x61480C
static int _ls_error_code;

// 0x614810
static int gLoadSaveWindowOldFont;

static FrmImage _loadsaveFrmImages[LOAD_SAVE_FRM_COUNT];

static int quickSaveSlots = 0;
static bool autoQuickSaveSlots = false;

// 0x47B7E4
void _InitLoadSave()
{
    _quick_done = false;
    _slot_cursor = 0;
    _patches = settings.system.master_patches_path.c_str();

    MapDirErase("MAPS\\", "SAV");
    MapDirErase(PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME "\\", PROTO_FILE_EXT);
    MapDirErase(PROTO_DIR_NAME "\\" ITEMS_DIR_NAME "\\", PROTO_FILE_EXT);

    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_AUTO_QUICK_SAVE, &quickSaveSlots);
    if (quickSaveSlots > 0 && quickSaveSlots <= 10) {
        autoQuickSaveSlots = true;
    }
}

// 0x47B85C
void _ResetLoadSave()
{
    MapDirErase("MAPS\\", "SAV");
    MapDirErase(PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME "\\", PROTO_FILE_EXT);
    MapDirErase(PROTO_DIR_NAME "\\" ITEMS_DIR_NAME "\\", PROTO_FILE_EXT);
}

// SaveGame
// 0x47B88C
int lsgSaveGame(int mode)
{
    ScopedGameMode gm(GameMode::kSaveGame);

    MessageListItem messageListItem;

    _ls_error_code = 0;
    _patches = settings.system.master_patches_path.c_str();

    // SFALL: skip slot selection if auto quicksave is enabled
    if (autoQuickSaveSlots) {
        _quick_done = true;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        // SFALL: cycle through first N slots for quicksaving
        if (autoQuickSaveSlots) {
            if (++_slot_cursor >= quickSaveSlots) {
                _slot_cursor = 0;
            }
        }
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        strcat(_gmpath, "SAVE.DAT");

        _flptr = fileOpen(_gmpath, "rb");
        if (_flptr != nullptr) {
            lsgLoadHeaderInSlot(_slot_cursor);
            fileClose(_flptr);
        }

        if (!messageListInit(&gLoadSaveMessageList)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }

        _snapshotBuf = nullptr;
        int v6 = _QuickSnapShot();
        if (v6 == 1) {
            int v7 = lsgPerformSaveGame();
            if (v7 != -1) {
                v6 = v7;
            }
        }

        if (_snapshotBuf != nullptr) {
            internal_free(_snapshot);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);

        if (v6 != -1) {
            return 1;
        }

        soundPlayFile("iisxxxx1");

        // Error saving game!
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 132));
        // Unable to save game.
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 133));

        const char* body[] = {
            _str1,
        };
        showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

        messageListFree(&gLoadSaveMessageList);

        return -1;
    }

    _quick_done = false;

    int windowType = mode == LOAD_SAVE_MODE_QUICK
        ? LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT
        : LOAD_SAVE_WINDOW_TYPE_SAVE_GAME;
    if (lsgWindowInit(windowType) == -1) {
        debugPrint("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    if (_GetSlotList() == -1) {
        windowRefresh(gLoadSaveWindow);

        soundPlayFile("iisxxxx1");

        // Error loading save game list!
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 106));
        // Save game directory:
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 107));

        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");

        // TODO: Check.
        strcpy(_str2, getmsg(&gLoadSaveMessageList, &messageListItem, 108));

        const char* body[] = {
            _str1,
            _str2,
        };
        showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

        lsgWindowFree(0);

        return -1;
    }

    switch (_LSstatus[_slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        blitBufferToBuffer(_snapshotBuf,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    default:
        _LoadTumbSlot(_slot_cursor);
        blitBufferToBuffer(_thumbnail_image,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
    _DrawInfoBox(_slot_cursor);
    windowRefresh(gLoadSaveWindow);

    _dbleclkcntr = 24;

    int rc = -1;
    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        bool selectionChanged = false;
        int scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                _slot_cursor -= 1;
                if (_slot_cursor < 0) {
                    _slot_cursor = 0;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_ARROW_DOWN:
                _slot_cursor += 1;
                if (_slot_cursor > 9) {
                    _slot_cursor = 9;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                _slot_cursor = 0;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_END:
                _slot_cursor = 9;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;
            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;
            case 502:
                if (1) {
                    int mouseX;
                    int mouseY;
                    mouseGetPositionInWindow(gLoadSaveWindow, &mouseX, &mouseY);

                    _slot_cursor = (mouseY - 79) / (3 * fontGetLineHeight() + 4);
                    if (_slot_cursor < 0) {
                        _slot_cursor = 0;
                    }
                    if (_slot_cursor > 9) {
                        _slot_cursor = 9;
                    }

                    selectionChanged = 1;

                    if (_slot_cursor == doubleClickSlot) {
                        keyCode = 500;
                        soundPlayFile("ib1p1xx1");
                    }

                    doubleClickSlot = _slot_cursor;
                    scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
                }
                break;
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                showQuitConfirmationDialog();

                if (_game_user_wants_to_quit != 0) {
                    rc = 0;
                }
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_MINUS:
            case KEY_UNDERSCORE:
                brightnessDecrease();
                break;
            case KEY_RETURN:
                keyCode = 500;
                break;
            }
        }

        if (keyCode == 500) {
            if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
                rc = 1;
                // Save game already exists, overwrite?
                const char* title = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 131);
                if (showDialogBox(title, nullptr, 0, 169, 131, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_YES_NO) == 0) {
                    rc = -1;
                }
            } else {
                rc = 1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = getTicks();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }

                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        _slot_cursor -= 1;
                        if (_slot_cursor < 0) {
                            _slot_cursor = 0;
                        }
                    } else {
                        _slot_cursor += 1;
                        if (_slot_cursor > 9) {
                            _slot_cursor = 9;
                        }
                    }

                    // TODO: Does not check for unsupported version error like
                    // other switches do.
                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                        blitBufferToBuffer(_snapshotBuf,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                }

                if (scrollCounter > 14.4) {
                    delay_ms(1000 / scrollVelocity - (getTicks() - start));
                } else {
                    delay_ms(1000 / 24 - (getTicks() - start));
                }

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                switch (_LSstatus[_slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    blitBufferToBuffer(_snapshotBuf,
                        LS_PREVIEW_WIDTH - 1,
                        LS_PREVIEW_HEIGHT - 1,
                        LS_PREVIEW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    _LoadTumbSlot(_slot_cursor);
                    blitBufferToBuffer(_thumbnail_image,
                        LS_PREVIEW_WIDTH - 1,
                        LS_PREVIEW_HEIGHT - 1,
                        LS_PREVIEW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                _DrawInfoBox(_slot_cursor);
                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
            }

            windowRefresh(gLoadSaveWindow);

            _dbleclkcntr -= 1;
            if (_dbleclkcntr == 0) {
                _dbleclkcntr = 24;
                doubleClickSlot = -1;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (rc == 1) {
            int v50 = _GetComment(_slot_cursor);
            if (v50 == -1) {
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                soundPlayFile("iisxxxx1");
                debugPrint("\nLOADSAVE: ** Error getting save file comment **\n");

                // Error saving game!
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 132));
                // Unable to save game.
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 133));

                const char* body[1] = {
                    _str1,
                };
                showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
            } else if (v50 == 0) {
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                rc = -1;
            } else if (v50 == 1) {
                if (lsgPerformSaveGame() == -1) {
                    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                    soundPlayFile("iisxxxx1");

                    // Error saving game!
                    strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 132));
                    // Unable to save game.
                    strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 133));

                    rc = -1;

                    const char* body[1] = {
                        _str1,
                    };
                    showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

                    if (_GetSlotList() == -1) {
                        windowRefresh(gLoadSaveWindow);
                        soundPlayFile("iisxxxx1");

                        // Error loading save agme list!
                        strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106));
                        // Save game directory:
                        strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");

                        char text[260];
                        // Doesn't exist or is corrupted.
                        strcpy(text, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        const char* body[2] = {
                            _str1,
                            _str2,
                        };
                        showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

                        lsgWindowFree(0);

                        return -1;
                    }

                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        blitBufferToBuffer(_snapshotBuf,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                    _dbleclkcntr = 24;
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    lsgWindowFree(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);

    tileWindowRefresh();

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            _quick_done = true;
        }
    }

    return rc;
}

// 0x47C5B4
static int _QuickSnapShot()
{
    _snapshot = (unsigned char*)internal_malloc(LS_PREVIEW_SIZE);
    if (_snapshot == nullptr) {
        return -1;
    }

    bool gameMouseWasVisible = gameMouseObjectsIsVisible();
    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    mouseHideCursor();
    tileWindowRefresh();
    mouseShowCursor();

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    // For preview take 640x380 area in the center of isometric window.
    Window* window = windowGetWindow(gIsoWindow);
    unsigned char* isoWindowBuffer = window->buffer
        + window->width * (window->height - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
        + (window->width - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
    blitBufferToBufferStretch(isoWindowBuffer,
        ORIGINAL_ISO_WINDOW_WIDTH,
        ORIGINAL_ISO_WINDOW_HEIGHT,
        windowGetWidth(gIsoWindow),
        _snapshot,
        LS_PREVIEW_WIDTH,
        LS_PREVIEW_HEIGHT,
        LS_PREVIEW_WIDTH);

    _snapshotBuf = _snapshot;

    return 1;
}

// LoadGame
// 0x47C640
int lsgLoadGame(int mode)
{
    ScopedGameMode gm(GameMode::kLoadGame);

    MessageListItem messageListItem;

    const char* body[] = {
        _str1,
        _str2,
    };

    _ls_error_code = 0;
    _patches = settings.system.master_patches_path.c_str();

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        int quickSaveWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
        int quickSaveWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
        int window = windowCreate(quickSaveWindowX,
            quickSaveWindowY,
            LS_WINDOW_WIDTH,
            LS_WINDOW_HEIGHT,
            256,
            WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (window != -1) {
            unsigned char* windowBuffer = windowGetBuffer(window);
            bufferFill(windowBuffer, LS_WINDOW_WIDTH, LS_WINDOW_HEIGHT, LS_WINDOW_WIDTH, _colorTable[0]);
            windowRefresh(window);
            renderPresent();
        }

        if (lsgLoadGameInSlot(_slot_cursor) != -1) {
            if (window != -1) {
                windowDestroy(window);
            }
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            return 1;
        }

        if (!messageListInit(&gLoadSaveMessageList)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }

        if (window != -1) {
            windowDestroy(window);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        soundPlayFile("iisxxxx1");
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 134));
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 135));
        showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);

        messageListFree(&gLoadSaveMessageList);
        mapNewMap();
        _game_user_wants_to_quit = 2;

        return -1;
    }

    _quick_done = false;

    int windowType;
    switch (mode) {
    case LOAD_SAVE_MODE_FROM_MAIN_MENU:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU;
        break;
    case LOAD_SAVE_MODE_NORMAL:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME;
        break;
    case LOAD_SAVE_MODE_QUICK:
        windowType = LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    if (lsgWindowInit(windowType) == -1) {
        debugPrint("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    if (_GetSlotList() == -1) {
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        windowRefresh(gLoadSaveWindow);
        renderPresent();
        soundPlayFile("iisxxxx1");
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106));
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));
        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");
        showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
        lsgWindowFree(windowType);
        return -1;
    }

    switch (_LSstatus[_slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
            LS_WINDOW_WIDTH);
        break;
    default:
        _LoadTumbSlot(_slot_cursor);
        blitBufferToBuffer(_thumbnail_image,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
    _DrawInfoBox(_slot_cursor);
    windowRefresh(gLoadSaveWindow);
    renderPresent();
    _dbleclkcntr = 24;

    int rc = -1;
    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int time = getTicks();
        int keyCode = inputGetInput();
        bool selectionChanged = false;
        int scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                _slot_cursor--;
                if (_slot_cursor < 0) {
                    _slot_cursor = 0;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_ARROW_DOWN:
                _slot_cursor++;
                if (_slot_cursor > 9) {
                    _slot_cursor = 9;
                }
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                _slot_cursor = 0;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_END:
                _slot_cursor = 9;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;
            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;
            case 502:
                if (1) {
                    int mouseX;
                    int mouseY;
                    mouseGetPositionInWindow(gLoadSaveWindow, &mouseX, &mouseY);

                    int clickedSlot = (mouseY - 79) / (3 * fontGetLineHeight() + 4);
                    if (clickedSlot < 0) {
                        clickedSlot = 0;
                    } else if (clickedSlot > 9) {
                        clickedSlot = 9;
                    }

                    _slot_cursor = clickedSlot;
                    if (clickedSlot == doubleClickSlot) {
                        keyCode = 500;
                        soundPlayFile("ib1p1xx1");
                    }

                    selectionChanged = true;
                    scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
                    doubleClickSlot = _slot_cursor;
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
            case KEY_RETURN:
                keyCode = 500;
                break;
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                showQuitConfirmationDialog();
                if (_game_user_wants_to_quit != 0) {
                    rc = 0;
                }
                break;
            }
        }

        if (keyCode == 500) {
            if (_LSstatus[_slot_cursor] != SLOT_STATE_EMPTY) {
                rc = 1;
            } else {
                rc = -1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection != LOAD_SAVE_SCROLL_DIRECTION_NONE) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = getTicks();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }

                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        _slot_cursor -= 1;
                        if (_slot_cursor < 0) {
                            _slot_cursor = 0;
                        }
                    } else {
                        _slot_cursor += 1;
                        if (_slot_cursor > 9) {
                            _slot_cursor = 9;
                        }
                    }

                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            LS_WINDOW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                }

                if (scrollCounter > 14.4) {
                    delay_ms(1000 / scrollVelocity - (getTicks() - start));
                } else {
                    delay_ms(1000 / 24 - (getTicks() - start));
                }

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                switch (_LSstatus[_slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    _LoadTumbSlot(_slot_cursor);
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        LS_WINDOW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    blitBufferToBuffer(_thumbnail_image,
                        LS_PREVIEW_WIDTH - 1,
                        LS_PREVIEW_HEIGHT - 1,
                        LS_PREVIEW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                _DrawInfoBox(_slot_cursor);
                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
            }

            windowRefresh(gLoadSaveWindow);

            _dbleclkcntr -= 1;
            if (_dbleclkcntr == 0) {
                _dbleclkcntr = 24;
                doubleClickSlot = -1;
            }
        }

        if (rc == 1) {
            switch (_LSstatus[_slot_cursor]) {
            case SLOT_STATE_UNSUPPORTED_VERSION:
                soundPlayFile("iisxxxx1");
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136));
                strcpy(_str2, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135));
                showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
                break;
            case SLOT_STATE_ERROR:
                soundPlayFile("iisxxxx1");
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136));
                showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                rc = -1;
                break;
            default:
                if (lsgLoadGameInSlot(_slot_cursor) == -1) {
                    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                    soundPlayFile("iisxxxx1");
                    strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                    strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135));
                    showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], nullptr, _colorTable[32328], DIALOG_BOX_LARGE);
                    mapNewMap();
                    _game_user_wants_to_quit = 2;
                    rc = -1;
                }
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    lsgWindowFree(mode == LOAD_SAVE_MODE_FROM_MAIN_MENU
            ? LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU
            : LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            _quick_done = true;
        }
    }

    return rc;
}

// 0x47D2E4
static int lsgWindowInit(int windowType)
{
    gLoadSaveWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    gLoadSaveWindowIsoWasEnabled = false;
    if (!messageListInit(&gLoadSaveMessageList)) {
        return -1;
    }

    snprintf(_str, sizeof(_str), "%s%s", asc_5186C8, LSGAME_MSG_NAME);
    if (!messageListLoad(&gLoadSaveMessageList, _str)) {
        return -1;
    }

    _snapshot = (unsigned char*)internal_malloc(61632);
    if (_snapshot == nullptr) {
        messageListFree(&gLoadSaveMessageList);
        fontSetCurrent(gLoadSaveWindowOldFont);
        return -1;
    }

    _thumbnail_image = _snapshot;
    _snapshotBuf = _snapshot + LS_PREVIEW_SIZE;

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        gLoadSaveWindowIsoWasEnabled = isoDisable();
    }

    colorCycleDisable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (windowType == LOAD_SAVE_WINDOW_TYPE_SAVE_GAME || windowType == LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT) {
        bool gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }

        mouseHideCursor();
        tileWindowRefresh();
        mouseShowCursor();

        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        // For preview take 640x380 area in the center of isometric window.
        Window* window = windowGetWindow(gIsoWindow);
        unsigned char* isoWindowBuffer = window->buffer
            + window->width * (window->height - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
            + (window->width - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
        blitBufferToBufferStretch(isoWindowBuffer,
            ORIGINAL_ISO_WINDOW_WIDTH,
            ORIGINAL_ISO_WINDOW_HEIGHT,
            windowGetWidth(gIsoWindow),
            _snapshotBuf,
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH);
    }

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gLoadSaveFrmIds[index], 0, 0, 0);
        if (!_loadsaveFrmImages[index].lock(fid)) {
            while (--index >= 0) {
                _loadsaveFrmImages[index].unlock();
            }
            internal_free(_snapshot);
            messageListFree(&gLoadSaveMessageList);
            fontSetCurrent(gLoadSaveWindowOldFont);

            if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
                if (gLoadSaveWindowIsoWasEnabled) {
                    isoEnable();
                }
            }

            colorCycleEnable();
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            return -1;
        }
    }

    int lsWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
    int lsWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
    gLoadSaveWindow = windowCreate(lsWindowX,
        lsWindowY,
        LS_WINDOW_WIDTH,
        LS_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (gLoadSaveWindow == -1) {
        // FIXME: Leaking frms.
        internal_free(_snapshot);
        messageListFree(&gLoadSaveMessageList);
        fontSetCurrent(gLoadSaveWindowOldFont);

        if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
            if (gLoadSaveWindowIsoWasEnabled) {
                isoEnable();
            }
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gLoadSaveWindowBuffer = windowGetBuffer(gLoadSaveWindow);
    memcpy(gLoadSaveWindowBuffer, _loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData(), LS_WINDOW_WIDTH * LS_WINDOW_HEIGHT);

    int messageId;
    switch (windowType) {
    case LOAD_SAVE_WINDOW_TYPE_SAVE_GAME:
        // SAVE GAME
        messageId = 102;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT:
        // PICK A QUICK SAVE SLOT
        messageId = 103;
        break;
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME:
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU:
        // LOAD GAME
        messageId = 100;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT:
        // PICK A QUICK LOAD SLOT
        messageId = 101;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    char* msg;

    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, messageId);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 27 + 48, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, _colorTable[18979]);

    // DONE
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 104);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 348 + 410, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, _colorTable[18979]);

    // CANCEL
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 105);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 348 + 515, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, _colorTable[18979]);

    int btn;

    btn = buttonCreate(gLoadSaveWindow,
        391,
        349,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        500,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        495,
        349,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getHeight() + 58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(gLoadSaveWindow, 55, 87, 230, 353, -1, -1, -1, 502, nullptr, nullptr, nullptr, BUTTON_FLAG_TRANSPARENT);
    fontSetCurrent(101);

    return 0;
}

// 0x47D824
static int lsgWindowFree(int windowType)
{
    windowDestroy(gLoadSaveWindow);
    fontSetCurrent(gLoadSaveWindowOldFont);
    messageListFree(&gLoadSaveMessageList);

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        _loadsaveFrmImages[index].unlock();
    }

    internal_free(_snapshot);

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        if (gLoadSaveWindowIsoWasEnabled) {
            isoEnable();
        }
    }

    colorCycleEnable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    return 0;
}

// 0x47D88C
static int lsgPerformSaveGame()
{
    _ls_error_code = 0;
    _map_backup_count = -1;
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);

    backgroundSoundPause();

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s", _patches, "SAVEGAME");
    compat_mkdir(_gmpath);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    compat_mkdir(_gmpath);

    strcat(_gmpath, "\\" PROTO_DIR_NAME);
    compat_mkdir(_gmpath);

    char* protoBasePath = _gmpath + strlen(_gmpath);

    strcpy(protoBasePath, "\\" CRITTERS_DIR_NAME);
    compat_mkdir(_gmpath);

    strcpy(protoBasePath, "\\" ITEMS_DIR_NAME);
    compat_mkdir(_gmpath);

    if (_SaveBackup() == -1) {
        debugPrint("\nLOADSAVE: Warning, can't backup save file!\n");
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    debugPrint("\nLOADSAVE: Save name: %s\n", _gmpath);

    _flptr = fileOpen(_gmpath, "wb");
    if (_flptr == nullptr) {
        debugPrint("\nLOADSAVE: ** Error opening save game for writing! **\n");
        _RestoreSave();
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        MapDirErase(_gmpath, "BAK");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    long pos = fileTell(_flptr);
    if (lsgSaveHeaderInSlot(_slot_cursor) == -1) {
        debugPrint("\nLOADSAVE: ** Error writing save game header! **\n");
        debugPrint("LOADSAVE: Save file header size written: %d bytes.\n", fileTell(_flptr) - pos);
        fileClose(_flptr);
        _RestoreSave();
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        MapDirErase(_gmpath, "BAK");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    for (int index = 0; index < LOAD_SAVE_HANDLER_COUNT; index++) {
        long pos = fileTell(_flptr);
        SaveGameHandler* handler = _master_save_list[index];
        if (handler(_flptr) == -1) {
            debugPrint("\nLOADSAVE: ** Error writing save function #%d data! **\n", index);
            fileClose(_flptr);
            _RestoreSave();
            snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
            MapDirErase(_gmpath, "BAK");
            _partyMemberUnPrepSave();
            backgroundSoundResume();
            return -1;
        }

        debugPrint("LOADSAVE: Save function #%d data size written: %d bytes.\n", index, fileTell(_flptr) - pos);
    }

    debugPrint("LOADSAVE: Total save data written: %ld bytes.\n", fileTell(_flptr));

    fileClose(_flptr);

    // SFALL: Save sfallgv.sav.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "sfallgv.sav");

    _flptr = fileOpen(_gmpath, "wb");
    if (_flptr != nullptr) {
        do {
            if (!sfall_gl_vars_save(_flptr)) {
                debugPrint("LOADSAVE (SFALL): ** Error saving global vars **\n");
                break;
            }

            // TODO: For now fill remaining sections with zeros to that Sfall
            // can successfully read our global vars and skip the rest.

            int nextObjectId = 0;
            if (fileWrite(&nextObjectId, sizeof(nextObjectId), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving next object id **\n");
                break;
            }

            int addedYears = 0;
            if (fileWrite(&addedYears, sizeof(addedYears), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving added years **\n");
                break;
            }

            int fakeTraitsCount = 0;
            if (fileWrite(&fakeTraitsCount, sizeof(fakeTraitsCount), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving fake traits **\n");
                break;
            }

            int fakePerksCount = 0;
            if (fileWrite(&fakePerksCount, sizeof(fakePerksCount), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving fake perks **\n");
                break;
            }

            int fakeSelectablePerksCount = 0;
            if (fileWrite(&fakeSelectablePerksCount, sizeof(fakeSelectablePerksCount), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving fake selectable perks **\n");
                break;
            }

            int arraysCountOld = 0;
            if (fileWrite(&arraysCountOld, sizeof(arraysCountOld), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving arrays (old fmt) **\n");
                break;
            }

            int arraysCountNew = 0;
            if (fileWrite(&arraysCountNew, sizeof(arraysCountNew), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving arrays (new fmt) **\n");
                break;
            }

            int drugPidsCount = 0;
            if (fileWrite(&drugPidsCount, sizeof(drugPidsCount), 1, _flptr) != 1) {
                debugPrint("LOADSAVE (SFALL): ** Error saving drug pids **\n");
                break;
            }
        } while (0);

        fileClose(_flptr);
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    MapDirErase(_gmpath, "BAK");

    gLoadSaveMessageListItem.num = 140;
    if (messageListGetItem(&gLoadSaveMessageList, &gLoadSaveMessageListItem)) {
        displayMonitorAddMessage(gLoadSaveMessageListItem.text);
    } else {
        debugPrint("\nError: Couldn't find LoadSave Message!");
    }

    backgroundSoundResume();

    return 0;
}

// 0x47DC60
bool _isLoadingGame()
{
    return _loadingGame;
}

// 0x47DC68
static int lsgLoadGameInSlot(int slot)
{
    _loadingGame = true;

    if (isInCombat()) {
        interfaceBarEndButtonsHide(false);
        _combat_over_from_load();
        gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    LoadSaveSlotData* ptr = &(_LSData[slot]);
    debugPrint("\nLOADSAVE: Load name: %s\n", ptr->description);

    _flptr = fileOpen(_gmpath, "rb");
    if (_flptr == nullptr) {
        debugPrint("\nLOADSAVE: ** Error opening load game file for reading! **\n");
        _loadingGame = false;
        return -1;
    }

    long pos = fileTell(_flptr);
    if (lsgLoadHeaderInSlot(slot) == -1) {
        debugPrint("\nLOADSAVE: ** Error reading save  game header! **\n");
        fileClose(_flptr);
        gameReset();
        _loadingGame = false;
        return -1;
    }

    debugPrint("LOADSAVE: Load file header size read: %d bytes.\n", fileTell(_flptr) - pos);

    for (int index = 0; index < LOAD_SAVE_HANDLER_COUNT; index += 1) {
        long pos = fileTell(_flptr);
        LoadGameHandler* handler = _master_load_list[index];
        if (handler(_flptr) == -1) {
            debugPrint("\nLOADSAVE: ** Error reading load function #%d data! **\n", index);
            int v12 = fileTell(_flptr);
            debugPrint("LOADSAVE: Load function #%d data size read: %d bytes.\n", index, fileTell(_flptr) - pos);
            fileClose(_flptr);
            gameReset();
            _loadingGame = false;
            return -1;
        }

        debugPrint("LOADSAVE: Load function #%d data size read: %d bytes.\n", index, fileTell(_flptr) - pos);
    }

    debugPrint("LOADSAVE: Total load data read: %ld bytes.\n", fileTell(_flptr));
    fileClose(_flptr);

    // SFALL: Load sfallgv.sav.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "sfallgv.sav");

    _flptr = fileOpen(_gmpath, "rb");
    if (_flptr != nullptr) {
        do {
            if (!sfall_gl_vars_load(_flptr)) {
                debugPrint("LOADSAVE (SFALL): ** Error loading global vars **\n");
                break;
            }

            // TODO: For now silently ignore remaining sections.
        } while (0);

        fileClose(_flptr);
    }

    snprintf(_str, sizeof(_str), "%s\\", "MAPS");
    MapDirErase(_str, "BAK");
    _proto_dude_update_gender();

    // Game Loaded.
    gLoadSaveMessageListItem.num = 141;
    if (messageListGetItem(&gLoadSaveMessageList, &gLoadSaveMessageListItem) == 1) {
        displayMonitorAddMessage(gLoadSaveMessageListItem.text);
    } else {
        debugPrint("\nError: Couldn't find LoadSave Message!");
    }

    _loadingGame = false;

    // SFALL: Start global scripts.
    sfall_gl_scr_exec_start_proc();

    return 0;
}

// 0x47DF10
static int lsgSaveHeaderInSlot(int slot)
{
    _ls_error_code = 4;

    LoadSaveSlotData* ptr = &(_LSData[slot]);
    strncpy(ptr->signature, LOAD_SAVE_SIGNATURE, 24);

    if (fileWrite(ptr->signature, 1, 24, _flptr) == -1) {
        return -1;
    }

    short temp[3];
    temp[0] = VERSION_MAJOR;
    temp[1] = VERSION_MINOR;

    ptr->versionMinor = temp[0];
    ptr->versionMajor = temp[1];

    if (fileWriteInt16List(_flptr, temp, 2) == -1) {
        return -1;
    }

    ptr->versionRelease = VERSION_RELEASE;
    if (fileWriteUInt8(_flptr, VERSION_RELEASE) == -1) {
        return -1;
    }

    char* characterName = critterGetName(gDude);
    strncpy(ptr->characterName, characterName, 32);

    if (fileWrite(ptr->characterName, 32, 1, _flptr) != 1) {
        return -1;
    }

    if (fileWrite(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }

    time_t now = time(nullptr);
    struct tm* local = localtime(&now);

    temp[0] = local->tm_mday;
    temp[1] = local->tm_mon + 1;
    temp[2] = local->tm_year + 1900;

    ptr->fileDay = temp[0];
    ptr->fileMonth = temp[1];
    ptr->fileYear = temp[2];
    ptr->fileTime = local->tm_hour + local->tm_min;

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (_db_fwriteLong(_flptr, ptr->fileTime) == -1) {
        return -1;
    }

    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    temp[0] = month;
    temp[1] = day;
    temp[2] = year;
    ptr->gameTime = gameTimeGetTime();

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (fileWriteUInt32(_flptr, ptr->gameTime) == -1) {
        return -1;
    }

    ptr->elevation = gElevation;
    if (fileWriteInt16(_flptr, ptr->elevation) == -1) {
        return -1;
    }

    ptr->map = mapGetCurrentMap();
    if (fileWriteInt16(_flptr, ptr->map) == -1) {
        return -1;
    }

    char mapName[128];
    strcpy(mapName, gMapHeader.name);

    // NOTE: Uppercased from "sav".
    char* v1 = _strmfe(_str, mapName, "SAV");
    strncpy(ptr->fileName, v1, 16);
    if (fileWrite(ptr->fileName, 16, 1, _flptr) != 1) {
        return -1;
    }

    if (fileWrite(_snapshotBuf, LS_PREVIEW_SIZE, 1, _flptr) != 1) {
        return -1;
    }

    memset(mapName, 0, 128);
    if (fileWrite(mapName, 1, 128, _flptr) != 128) {
        return -1;
    }

    _ls_error_code = 0;

    return 0;
}

// 0x47E2E4
static int lsgLoadHeaderInSlot(int slot)
{
    _ls_error_code = 3;

    LoadSaveSlotData* ptr = &(_LSData[slot]);

    if (fileRead(ptr->signature, 1, 24, _flptr) != 24) {
        return -1;
    }

    if (strncmp(ptr->signature, LOAD_SAVE_SIGNATURE, 18) != 0) {
        debugPrint("\nLOADSAVE: ** Invalid save file on load! **\n");
        _ls_error_code = 2;
        return -1;
    }

    short v8[3];
    if (fileReadInt16List(_flptr, v8, 2) == -1) {
        return -1;
    }

    ptr->versionMinor = v8[0];
    ptr->versionMajor = v8[1];

    if (fileReadUInt8(_flptr, &(ptr->versionRelease)) == -1) {
        return -1;
    }

    if (ptr->versionMinor != 1 || ptr->versionMajor != 2 || ptr->versionRelease != 'R') {
        debugPrint("\nLOADSAVE: Load slot #%d Version: %d.%d%c\n", slot, ptr->versionMinor, ptr->versionMajor, ptr->versionRelease);
        _ls_error_code = 1;
        return -1;
    }

    if (fileRead(ptr->characterName, 32, 1, _flptr) != 1) {
        return -1;
    }

    if (fileRead(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->fileMonth = v8[0];
    ptr->fileDay = v8[1];
    ptr->fileYear = v8[2];

    if (_db_freadInt(_flptr, &(ptr->fileTime)) == -1) {
        return -1;
    }

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->gameMonth = v8[0];
    ptr->gameDay = v8[1];
    ptr->gameYear = v8[2];

    if (fileReadUInt32(_flptr, &(ptr->gameTime)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->elevation)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->map)) == -1) {
        return -1;
    }

    if (fileRead(ptr->fileName, 1, 16, _flptr) != 16) {
        return -1;
    }

    if (fileSeek(_flptr, LS_PREVIEW_SIZE, SEEK_CUR) != 0) {
        return -1;
    }

    if (fileSeek(_flptr, 128, 1) != 0) {
        return -1;
    }

    _ls_error_code = 0;

    return 0;
}

// 0x47E5D0
static int _GetSlotList()
{
    int index = 0;
    for (; index < 10; index += 1) {
        snprintf(_str, sizeof(_str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", index + 1, "SAVE.DAT");

        int fileSize;
        if (dbGetFileSize(_str, &fileSize) != 0) {
            _LSstatus[index] = SLOT_STATE_EMPTY;
        } else {
            _flptr = fileOpen(_str, "rb");

            if (_flptr == nullptr) {
                debugPrint("\nLOADSAVE: ** Error opening save  game for reading! **\n");
                return -1;
            }

            if (lsgLoadHeaderInSlot(index) == -1) {
                if (_ls_error_code == 1) {
                    debugPrint("LOADSAVE: ** save file #%d is an older version! **\n", _slot_cursor);
                    _LSstatus[index] = SLOT_STATE_UNSUPPORTED_VERSION;
                } else {
                    debugPrint("LOADSAVE: ** Save file #%d corrupt! **", index);
                    _LSstatus[index] = SLOT_STATE_ERROR;
                }
            } else {
                _LSstatus[index] = SLOT_STATE_OCCUPIED;
            }

            fileClose(_flptr);
        }
    }
    return index;
}

// 0x47E6D8
static void _ShowSlotList(int windowType)
{
    bufferFill(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 87 + 55, 230, 353, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer[LS_WINDOW_WIDTH * 86 + 55] & 0xFF);

    int y = 87;
    for (int index = 0; index < 10; index += 1) {

        int color = index == _slot_cursor ? _colorTable[32747] : _colorTable[992];
        const char* text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, windowType != 0 ? 110 : 109);
        snprintf(_str, sizeof(_str), "[   %s %.2d:   ]", text, index + 1);
        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

        y += fontGetLineHeight();
        switch (_LSstatus[index]) {
        case SLOT_STATE_OCCUPIED:
            strcpy(_str, _LSData[index].description);
            break;
        case SLOT_STATE_EMPTY:
            // - EMPTY -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 111);
            snprintf(_str, sizeof(_str), "       %s", text);
            break;
        case SLOT_STATE_ERROR:
            // - CORRUPT SAVE FILE -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 112);
            snprintf(_str, sizeof(_str), "%s", text);
            color = _colorTable[32328];
            break;
        case SLOT_STATE_UNSUPPORTED_VERSION:
            // - OLD VERSION -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 113);
            snprintf(_str, sizeof(_str), " %s", text);
            color = _colorTable[32328];
            break;
        }

        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
        y += 2 * fontGetLineHeight() + 4;
    }
}

// 0x47E8E0
static void _DrawInfoBox(int slot)
{
    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 254 + 396, 164, 60, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 254 + 396, 640);

    unsigned char* dest;
    const char* text;
    int color = _colorTable[992];

    switch (_LSstatus[slot]) {
    case SLOT_STATE_OCCUPIED:
        if (1) {
            LoadSaveSlotData* ptr = &(_LSData[slot]);
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 254 + 396, ptr->characterName, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            snprintf(_str,
                sizeof(_str),
                "%.2d %s %.4d   %.4d",
                ptr->gameDay,
                getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 116 + ptr->gameMonth),
                ptr->gameYear,
                100 * ((ptr->gameTime / 600) / 60 % 24) + (ptr->gameTime / 600) % 60);

            int v2 = fontGetLineHeight();
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * (256 + v2) + 397, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            snprintf(_str,
                sizeof(_str),
                "%s %s",
                mapGetCityName(ptr->map),
                mapGetName(ptr->map, ptr->elevation));

            int y = v2 + 3 + v2 + 256;
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (wordWrap(_str, 164, beginnings, &count) == 0) {
                for (int index = 0; index < count - 1; index += 1) {
                    char* beginning = _str + beginnings[index];
                    char* ending = _str + beginnings[index + 1];
                    char c = *ending;
                    *ending = '\0';
                    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 399, beginning, 164, LS_WINDOW_WIDTH, color);
                    y += v2 + 2;
                }
            }
        }
        return;
    case SLOT_STATE_EMPTY:
        // Empty.
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 114);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 404;
        break;
    case SLOT_STATE_ERROR:
        // Error!
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 115);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 404;
        color = _colorTable[32328];
        break;
    case SLOT_STATE_UNSUPPORTED_VERSION:
        // Old version.
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 116);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 400;
        color = _colorTable[32328];
        break;
    default:
        assert(false && "Should be unreachable");
    }

    fontDrawText(dest, text, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
}

// 0x47EC48
static int _LoadTumbSlot(int slot)
{
    if (_LSstatus[_slot_cursor] != SLOT_STATE_EMPTY
        && _LSstatus[_slot_cursor] != SLOT_STATE_ERROR
        && _LSstatus[_slot_cursor] != SLOT_STATE_UNSUPPORTED_VERSION) {
        snprintf(_str, sizeof(_str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", _slot_cursor + 1, "SAVE.DAT");
        debugPrint(" Filename %s\n", _str);

        File* stream = fileOpen(_str, "rb");
        if (stream == nullptr) {
            debugPrint("\nLOADSAVE: ** (A) Error reading thumbnail #%d! **\n", slot);
            return -1;
        }

        if (fileSeek(stream, 131, SEEK_SET) != 0) {
            debugPrint("\nLOADSAVE: ** (B) Error reading thumbnail #%d! **\n", slot);
            fileClose(stream);
            return -1;
        }

        if (fileRead(_thumbnail_image, LS_PREVIEW_SIZE, 1, stream) != 1) {
            debugPrint("\nLOADSAVE: ** (C) Error reading thumbnail #%d! **\n", slot);
            fileClose(stream);
            return -1;
        }

        fileClose(stream);
    }

    return 0;
}

// 0x47ED5C
static int _GetComment(int slot)
{
    // Maintain original position in original resolution, otherwise center it.
    int commentWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth()) / 2
        : LS_COMMENT_WINDOW_X;
    int commentWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight()) / 2
        : LS_COMMENT_WINDOW_Y;
    int window = windowCreate(commentWindowX,
        commentWindowY,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (window == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight() * _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth());

    fontSetCurrent(103);

    const char* msg;

    // DONE
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 104);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 57 + 56,
        msg,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _colorTable[18979]);

    // CANCEL
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 105);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 57 + 181,
        msg,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _colorTable[18979]);

    // DESCRIPTION
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 130);

    char title[260];
    strcpy(title, msg);

    int width = fontGetStringWidth(title);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 7 + (_loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() - width) / 2,
        title,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _colorTable[18979]);

    fontSetCurrent(101);

    int btn;

    // DONE
    btn = buttonCreate(window,
        34,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        507,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // CANCEL
    btn = buttonCreate(window,
        160,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        508,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
        strncpy(description, _LSData[slot].description, LOAD_SAVE_DESCRIPTION_LENGTH);
    } else {
        memset(description, '\0', LOAD_SAVE_DESCRIPTION_LENGTH);
    }

    int rc;

    int backgroundColor = *(_loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getData() + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 35 + 24);
    if (_get_input_str2(window, 507, 508, description, LOAD_SAVE_DESCRIPTION_LENGTH - 1, 24, 35, _colorTable[992], backgroundColor, 0) == 0) {
        strncpy(_LSData[slot].description, description, LOAD_SAVE_DESCRIPTION_LENGTH);
        _LSData[slot].description[LOAD_SAVE_DESCRIPTION_LENGTH - 1] = '\0';
        rc = 1;
    } else {
        rc = 0;
    }

    windowDestroy(window);

    return rc;
}

// 0x47F084
static int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
{
    int cursorWidth = fontGetStringWidth("_") - 4;
    int windowWidth = windowGetWidth(win);
    int lineHeight = fontGetLineHeight();
    unsigned char* windowBuffer = windowGetBuffer(win);
    if (maxLength > 255) {
        maxLength = 255;
    }

    char text[256];
    strcpy(text, description);

    size_t textLength = strlen(text);
    text[textLength] = ' ';
    text[textLength + 1] = '\0';

    int nameWidth = fontGetStringWidth(text);

    bufferFill(windowBuffer + windowWidth * y + x, nameWidth, lineHeight, windowWidth, backgroundColor);
    fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);

    windowRefresh(win);
    renderPresent();

    beginTextInput();

    int blinkCounter = 3;
    bool blink = false;

    int v1 = 0;

    int rc = 1;
    while (rc == 1) {
        sharedFpsLimiter.mark();

        int tick = getTicks();

        int keyCode = inputGetInput();
        if ((keyCode & 0x80000000) == 0) {
            v1++;
        }

        if (keyCode == doneKeyCode || keyCode == KEY_RETURN) {
            rc = 0;
        } else if (keyCode == cancelKeyCode || keyCode == KEY_ESCAPE) {
            rc = -1;
        } else {
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && textLength > 0) {
                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(text), lineHeight, windowWidth, backgroundColor);

                // TODO: Probably incorrect, needs testing.
                if (v1 == 1) {
                    textLength = 1;
                }

                text[textLength - 1] = ' ';
                text[textLength] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength--;
            } else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && textLength < maxLength) {
                if ((flags & 0x01) != 0) {
                    if (!_isdoschar(keyCode)) {
                        break;
                    }
                }

                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(text), lineHeight, windowWidth, backgroundColor);

                text[textLength] = keyCode & 0xFF;
                text[textLength + 1] = ' ';
                text[textLength + 2] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength++;

                windowRefresh(win);
            }
        }

        blinkCounter -= 1;
        if (blinkCounter == 0) {
            blinkCounter = 3;
            blink = !blink;

            int color = blink ? backgroundColor : textColor;
            bufferFill(windowBuffer + windowWidth * y + x + fontGetStringWidth(text) - cursorWidth, cursorWidth, lineHeight - 2, windowWidth, color);
            windowRefresh(win);
        }

        delay_ms(1000 / 24 - (getTicks() - tick));

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        text[textLength] = '\0';
        strcpy(description, text);
    }

    return rc;
}

// 0x47F48C
static int _DummyFunc(File* stream)
{
    return 0;
}

// 0x47F490
static int _PrepLoad(File* stream)
{
    gameReset();
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    gMapHeader.name[0] = '\0';
    gameTimeSetTime(_LSData[_slot_cursor].gameTime);
    return 0;
}

// 0x47F4C8
static int _EndLoad(File* stream)
{
    wmMapMusicStart();
    dudeSetName(_LSData[_slot_cursor].characterName);
    interfaceBarRefresh();
    indicatorBarRefresh();
    tileWindowRefresh();
    if (isInCombat()) {
        scriptsRequestCombat(nullptr);
    }
    return 0;
}

// 0x47F510
static int _GameMap2Slot(File* stream)
{
    if (_partyMemberPrepSave() == -1) {
        return -1;
    }

    if (_map_save_in_game(false) == -1) {
        return -1;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index += 1) {
        int pid = gPartyMemberPids[index];
        if (pid == -2) {
            continue;
        }

        char path[COMPAT_MAX_PATH];
        if (_proto_list_str(pid, path) != 0) {
            continue;
        }

        const char* critterItemPath = (pid >> 24) == OBJ_TYPE_CRITTER
            ? PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME
            : PROTO_DIR_NAME "\\" ITEMS_DIR_NAME;
        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, critterItemPath, path);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, critterItemPath, path);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            return -1;
        }
    }

    snprintf(_str0, sizeof(_str0), "%s\\*.%s", "MAPS", "SAV");

    char** fileNameList;
    int fileNameListLength = fileNameListInit(_str0, &fileNameList, 0, 0);
    if (fileNameListLength == -1) {
        return -1;
    }

    if (fileWriteInt32(stream, fileNameListLength) == -1) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    if (fileNameListLength == 0) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    if (MapDirErase(_gmpath, "SAV") == -1) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    strcat(_gmpath, _str0);
    compat_remove(_gmpath);

    for (int index = 0; index < fileNameListLength; index += 1) {
        char* string = fileNameList[index];
        if (fileWrite(string, strlen(string) + 1, 1, stream) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }

        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", string);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, string);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }
    }

    fileNameListFree(&fileNameList, 0);

    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, _str0);
    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");

    if (fileCopyCompressed(_str0, _str1) == -1) {
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\%s", "MAPS", "AUTOMAP.DB");
    File* inStream = fileOpen(_str0, "rb");
    if (inStream == nullptr) {
        return -1;
    }

    int fileSize = fileGetSize(inStream);
    if (fileSize == -1) {
        fileClose(inStream);
        return -1;
    }

    fileClose(inStream);

    if (fileWriteInt32(stream, fileSize) == -1) {
        return -1;
    }

    if (_partyMemberUnPrepSave() == -1) {
        return -1;
    }

    return 0;
}

// SlotMap2Game
// 0x47F990
static int _SlotMap2Game(File* stream)
{
    debugPrint("LOADSAVE: in SlotMap2Game\n");

    int fileNameListLength;
    if (fileReadInt32(stream, &fileNameListLength) == -1) {
        debugPrint("LOADSAVE: returning 1\n");
        return -1;
    }

    if (fileNameListLength == 0) {
        debugPrint("LOADSAVE: returning 2\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME);

    if (MapDirErase(_str0, PROTO_FILE_EXT) == -1) {
        debugPrint("LOADSAVE: returning 3\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", PROTO_DIR_NAME "\\" ITEMS_DIR_NAME);
    if (MapDirErase(_str0, PROTO_FILE_EXT) == -1) {
        debugPrint("LOADSAVE: returning 4\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", "MAPS");
    if (MapDirErase(_str0, "SAV") == -1) {
        debugPrint("LOADSAVE: returning 5\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    compat_remove(_str0);

    for (int index = 1; index < gPartyMemberDescriptionsLength; index += 1) {
        int pid = gPartyMemberPids[index];
        if (pid != -2) {
            char protoPath[COMPAT_MAX_PATH];
            if (_proto_list_str(pid, protoPath) == 0) {
                const char* basePath = PID_TYPE(pid) == OBJ_TYPE_CRITTER
                    ? PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME
                    : PROTO_DIR_NAME "\\" ITEMS_DIR_NAME;
                snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, basePath, protoPath);
                snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, basePath, protoPath);

                if (_gzdecompress_file(_str1, _str0) == -1) {
                    debugPrint("LOADSAVE: returning 6\n");
                    return -1;
                }
            }
        }
    }

    for (int index = 0; index < fileNameListLength; index += 1) {
        char fileName[COMPAT_MAX_PATH];
        if (_mygets(fileName, stream) == -1) {
            break;
        }

        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, fileName);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s", _patches, "MAPS", fileName);

        if (_gzdecompress_file(_str0, _str1) == -1) {
            debugPrint("LOADSAVE: returning 7\n");
            return -1;
        }
    }

    const char* automapFileName = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, automapFileName);
    snprintf(_str1, sizeof(_str1), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    if (fileCopyDecompressed(_str0, _str1) == -1) {
        debugPrint("LOADSAVE: returning 8\n");
        return -1;
    }

    snprintf(_str1, sizeof(_str1), "%s\\%s", "MAPS", "AUTOMAP.DB");

    int v12;
    if (fileReadInt32(stream, &v12) == -1) {
        debugPrint("LOADSAVE: returning 9\n");
        return -1;
    }

    if (mapLoadSaved(_LSData[_slot_cursor].fileName) == -1) {
        debugPrint("LOADSAVE: returning 13\n");
        return -1;
    }

    return 0;
}

// 0x47FE14
static int _mygets(char* dest, File* stream)
{
    int index = 14;
    while (true) {
        int c = fileReadChar(stream);
        if (c == -1) {
            return -1;
        }

        index -= 1;

        *dest = c & 0xFF;
        dest += 1;

        if (index == -1 || c == '\0') {
            break;
        }
    }

    if (index == 0) {
        return -1;
    }

    return 0;
}

// 0x47FE58
static int _copy_file(const char* existingFileName, const char* newFileName)
{
    File* stream1;
    File* stream2;
    int length;
    int chunk_length;
    void* buf;
    int result;

    stream1 = nullptr;
    stream2 = nullptr;
    buf = nullptr;
    result = -1;

    stream1 = fileOpen(existingFileName, "rb");
    if (stream1 == nullptr) {
        goto out;
    }

    length = fileGetSize(stream1);
    if (length == -1) {
        goto out;
    }

    stream2 = fileOpen(newFileName, "wb");
    if (stream2 == nullptr) {
        goto out;
    }

    buf = internal_malloc(0xFFFF);
    if (buf == nullptr) {
        goto out;
    }

    while (length != 0) {
        chunk_length = std::min(length, 0xFFFF);

        if (fileRead(buf, chunk_length, 1, stream1) != 1) {
            break;
        }

        if (fileWrite(buf, chunk_length, 1, stream2) != 1) {
            break;
        }

        length -= chunk_length;
    }

    if (length != 0) {
        goto out;
    }

    result = 0;

out:

    if (stream1 != nullptr) {
        fileClose(stream1);
    }

    if (stream2 != nullptr) {
        fileClose(stream2);
    }

    if (buf != nullptr) {
        internal_free(buf);
    }

    return result;
}

// InitLoadSave
// 0x48000C
void lsgInit()
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\", "MAPS");
    MapDirErase(path, "SAV");
}

// 0x480040
int MapDirErase(const char* relativePath, const char* extension)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s*.%s", relativePath, extension);

    char** fileList;
    int fileListLength = fileNameListInit(path, &fileList, 0, 0);
    while (--fileListLength >= 0) {
        snprintf(path, sizeof(path), "%s\\%s%s", _patches, relativePath, fileList[fileListLength]);
        compat_remove(path);
    }
    fileNameListFree(&fileList, 0);

    return 0;
}

// 0x4800C8
int _MapDirEraseFile_(const char* a1, const char* a2)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "%s\\%s%s", _patches, a1, a2);
    if (compat_remove(path) != 0) {
        return -1;
    }

    return 0;
}

// 0x480104
static int _SaveBackup()
{
    debugPrint("\nLOADSAVE: Backing up save slot files..\n");

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);

    strcat(_str0, "SAVE.DAT");

    _strmfe(_str1, _str0, "BAK");

    File* stream1 = fileOpen(_str0, "rb");
    if (stream1 != nullptr) {
        fileClose(stream1);
        if (compat_rename(_str0, _str1) != 0) {
            return -1;
        }
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList, 0, 0);
    if (fileListLength == -1) {
        return -1;
    }

    _map_backup_count = fileListLength;

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);

        _strmfe(_str1, _str0, "BAK");
        if (compat_rename(_str0, _str1) != 0) {
            fileNameListFree(&fileList, 0);
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    debugPrint("\nLOADSAVE: %d map files backed up.\n", fileListLength);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    snprintf(_str0, sizeof(_str0), "%s\\%s", _gmpath, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    snprintf(_str1, sizeof(_str1), "%s\\%s", _gmpath, v2);

    _automap_db_flag = false;

    File* stream2 = fileOpen(_str0, "rb");
    if (stream2 != nullptr) {
        fileClose(stream2);

        if (_copy_file(_str0, _str1) == -1) {
            return -1;
        }

        _automap_db_flag = true;
    }

    return 0;
}

// 0x4803D8
static int _RestoreSave()
{
    debugPrint("\nLOADSAVE: Restoring save file backup...\n");

    _EraseSave();

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);
    strcat(_str0, "SAVE.DAT");
    _strmfe(_str1, _str0, "BAK");
    compat_remove(_str0);

    if (compat_rename(_str1, _str0) != 0) {
        _EraseSave();
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "BAK");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList, 0, 0);
    if (fileListLength == -1) {
        return -1;
    }

    if (fileListLength != _map_backup_count) {
        // FIXME: Probably leaks fileList.
        _EraseSave();
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);
        _strmfe(_str1, _str0, "SAV");
        compat_remove(_str1);
        if (compat_rename(_str0, _str1) != 0) {
            // FIXME: Probably leaks fileList.
            _EraseSave();
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    if (!_automap_db_flag) {
        return 0;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    strcpy(_str0, _gmpath);
    strcat(_str0, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    strcpy(_str1, _gmpath);
    strcat(_str1, v2);

    if (compat_rename(_str0, _str1) != 0) {
        _EraseSave();
        return -1;
    }

    return 0;
}

// 0x480710
static int _LoadObjDudeCid(File* stream)
{
    int value;

    if (fileReadInt32(stream, &value) == -1) {
        return -1;
    }

    gDude->cid = value;

    return 0;
}

// 0x480734
static int _SaveObjDudeCid(File* stream)
{
    return fileWriteInt32(stream, gDude->cid);
}

// 0x480754
static int _EraseSave()
{
    debugPrint("\nLOADSAVE: Erasing save(bad) slot...\n");

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);
    strcat(_str0, "SAVE.DAT");
    compat_remove(_str0);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList, 0, 0);
    if (fileListLength == -1) {
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);
        compat_remove(_str0);
    }

    fileNameListFree(&fileList, 0);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    strcpy(_str0, _gmpath);
    strcat(_str0, v1);

    compat_remove(_str0);

    return 0;
}

} // namespace fallout
