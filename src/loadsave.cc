#include "loadsave.h"

#include "automap.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "core.h"
#include "critter.h"
#include "cycle.h"
#include "dbox.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "file_utils.h"
#include "game.h"
#include "game_config.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "interface.h"
#include "item.h"
#include "map.h"
#include "memory.h"
#include "object.h"
#include "options.h"
#include "perk.h"
#include "pipboy.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "version.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "world_map.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#define LS_WINDOW_WIDTH 640
#define LS_WINDOW_HEIGHT 480

#define LS_PREVIEW_WIDTH 224
#define LS_PREVIEW_HEIGHT 133
#define LS_PREVIEW_SIZE ((LS_PREVIEW_WIDTH) * (LS_PREVIEW_HEIGHT))

#define LS_COMMENT_WINDOW_X 169
#define LS_COMMENT_WINDOW_Y 116

// 0x47B7C0
const int gLoadSaveFrmIds[LOAD_SAVE_FRM_COUNT] = {
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
int _slot_cursor = 0;

// 0x5193BC
bool _quick_done = false;

// 0x5193C0
bool gLoadSaveWindowIsoWasEnabled = false;

// 0x5193C4
int _map_backup_count = -1;

// 0x5193C8
int _automap_db_flag = 0;

// 0x5193CC
char* _patches = NULL;

// 0x5193D0
char _emgpath[] = "\\FALLOUT\\CD\\DATA\\SAVEGAME";

// 0x5193EC
SaveGameHandler* _master_save_list[LOAD_SAVE_HANDLER_COUNT] = {
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
    worldmapSave,
    pipboySave,
    gameMoviesSave,
    skillsUsageSave,
    partyMembersSave,
    queueSave,
    interfaceSave,
    _DummyFunc,
};

// 0x519458
LoadGameHandler* _master_load_list[LOAD_SAVE_HANDLER_COUNT] = {
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
    worldmapLoad,
    pipboyLoad,
    gameMoviesLoad,
    skillsUsageLoad,
    partyMembersLoad,
    queueLoad,
    interfaceLoad,
    _EndLoad,
};

// 0x5194C4
int _loadingGame = 0;

// 0x613CE0
Size gLoadSaveFrmSizes[LOAD_SAVE_FRM_COUNT];

// lsgame.msg
//
// 0x613D28
MessageList gLoadSaveMessageList;

// 0x613D30
STRUCT_613D30 _LSData[10];

// 0x614280
int _LSstatus[10];

// 0x6142A8
unsigned char* _thumbnail_image;

// 0x6142AC
unsigned char* _snapshotBuf;

// 0x6142B0
MessageListItem gLoadSaveMessageListItem;

// 0x6142C0
int _dbleclkcntr;

// 0x6142C4
int gLoadSaveWindow;

// 0x6142C8
unsigned char* gLoadSaveFrmData[LOAD_SAVE_FRM_COUNT];

// 0x6142EC
unsigned char* _snapshot;

// 0x6142F0
char _str2[COMPAT_MAX_PATH];

// 0x6143F4
char _str0[COMPAT_MAX_PATH];

// 0x6144F8
char _str1[COMPAT_MAX_PATH];

// 0x6145FC
char _str[COMPAT_MAX_PATH];

// 0x614700
unsigned char* gLoadSaveWindowBuffer;

// 0x614704
char _gmpath[COMPAT_MAX_PATH];

// 0x614808
File* _flptr;

// 0x61480C
int _ls_error_code;

// 0x614810
int gLoadSaveWindowOldFont;

// 0x614814
CacheEntry* gLoadSaveFrmHandles[LOAD_SAVE_FRM_COUNT];

// 0x47B7E4
void _InitLoadSave()
{
    _quick_done = false;
    _slot_cursor = 0;

    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &_patches)) {
        debugPrint("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        _patches = _emgpath;
    }

    _MapDirErase("MAPS\\", "SAV");
    _MapDirErase("PROTO\\CRITTERS\\", "PRO");
    _MapDirErase("PROTO\\ITEMS\\", "PRO");
}

// 0x47B85C
void _ResetLoadSave()
{
    _MapDirErase("MAPS\\", "SAV");
    _MapDirErase("PROTO\\CRITTERS\\", "PRO");
    _MapDirErase("PROTO\\ITEMS\\", "PRO");
}

// SaveGame
// 0x47B88C
int lsgSaveGame(int mode)
{
    MessageListItem messageListItem;

    _ls_error_code = 0;

    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &_patches)) {
        debugPrint("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        _patches = _emgpath;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        strcat(_gmpath, "SAVE.DAT");

        _flptr = fileOpen(_gmpath, "rb");
        if (_flptr != NULL) {
            lsgLoadHeaderInSlot(_slot_cursor);
            fileClose(_flptr);
        }

        _snapshotBuf = NULL;
        int v6 = _QuickSnapShot();
        if (v6 == 1) {
            int v7 = lsgPerformSaveGame();
            if (v7 != -1) {
                v6 = v7;
            }
        }

        if (_snapshotBuf != NULL) {
            internal_free(_snapshot);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);

        if (v6 != -1) {
            return 1;
        }

        if (!messageListInit(&gLoadSaveMessageList)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        sprintf(path, "%s%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }

        soundPlayFile("iisxxxx1");

        // Error saving game!
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 132));
        // Unable to save game.
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 133));

        const char* body[] = {
            _str1,
        };
        showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE);

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

        sprintf(_str2, "\"%s\\\"", "SAVEGAME");

        // TODO: Check.
        strcpy(_str2, getmsg(&gLoadSaveMessageList, &messageListItem, 108));

        const char* body[] = {
            _str1,
            _str2,
        };
        showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE);

        lsgWindowFree(0);

        return -1;
    }

    unsigned char* src;
    int v33 = _LSstatus[_slot_cursor];
    if (v33 != 0 && v33 != 2 && v33 != 3) {
        _LoadTumbSlot(_slot_cursor);
        src = _thumbnail_image;
    } else {
        src = _snapshotBuf;
    }

    blitBufferToBuffer(src, 223, 132, LS_PREVIEW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366, LS_WINDOW_WIDTH);
    _ShowSlotList(0);
    _DrawInfoBox(_slot_cursor);
    windowRefresh(gLoadSaveWindow);

    _dbleclkcntr = 24;

    int rc = -1;
    int v103 = -1;
    while (rc == -1) {
        int tick = _get_time();
        int keyCode = _get_input();
        int v37 = 0;
        int v102 = 0;

        if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                _slot_cursor -= 1;
                if (_slot_cursor < 0) {
                    _slot_cursor = 0;
                }
                v37 = 1;
                v103 = -1;
                break;
            case KEY_ARROW_DOWN:
                _slot_cursor += 1;
                if (_slot_cursor > 9) {
                    _slot_cursor = 9;
                }
                v37 = 1;
                v103 = -1;
                break;
            case KEY_HOME:
                v37 = 1;
                v103 = -1;
                _slot_cursor = 0;
                break;
            case KEY_END:
                v103 = -1;
                v37 = 1;
                _slot_cursor = 9;
                break;
            case 506:
                v102 = 1;
                break;
            case 504:
                v102 = 2;
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

                    v37 = 1;

                    if (_slot_cursor == v103) {
                        keyCode = 500;
                        soundPlayFile("ib1p1xx1");
                    }

                    v103 = _slot_cursor;
                    v102 = 0;
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
            rc = _LSstatus[_slot_cursor];
            if (rc == 1) {
                // Save game already exists, overwrite?
                const char* title = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 131);
                if (showDialogBox(title, NULL, 0, 169, 131, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_YES_NO) == 0) {
                    rc = -1;
                }
            } else {
                rc = 1;
            }

            v37 = 1;
            v102 = 0;
        }

        if (v102) {
            // TODO: Incomplete.
        } else {
            if (v37) {
                unsigned char* src;
                int v49 = _LSstatus[_slot_cursor];
                if (v49 != 0 && v49 != 2 && v49 != 3) {
                    _LoadTumbSlot(_slot_cursor);
                    src = _thumbnail_image;
                } else {
                    src = _snapshotBuf;
                }

                blitBufferToBuffer(src, 223, 132, LS_PREVIEW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366, LS_WINDOW_WIDTH);

                _DrawInfoBox(_slot_cursor);

                _ShowSlotList(0);
            }

            windowRefresh(gLoadSaveWindow);

            _dbleclkcntr -= 1;
            if (_dbleclkcntr == 0) {
                _dbleclkcntr = 24;
                v103 = -1;
            }

            while (getTicksSince(tick) < 1000 / 24) {
            }
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

                rc = -1;

                const char* body[1] = {
                    _str1,
                };
                showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE);
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
                    showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE);

                    if (_GetSlotList() == -1) {
                        windowRefresh(gLoadSaveWindow);
                        soundPlayFile("iisxxxx1");

                        // Error loading save agme list!
                        strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106));
                        // Save game directory:
                        strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        sprintf(_str2, "\"%s\\\"", "SAVEGAME");

                        char text[260];
                        // Doesn't exist or is corrupted.
                        strcpy(text, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        const char* body[2] = {
                            _str1,
                            _str2,
                        };
                        showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], NULL, _colorTable[32328], DIALOG_BOX_LARGE);

                        lsgWindowFree(0);

                        return -1;
                    }

                    unsigned char* src;
                    int state = _LSstatus[_slot_cursor];
                    if (state == SLOT_STATE_EMPTY || state == SLOT_STATE_ERROR || state == SLOT_STATE_UNSUPPORTED_VERSION) {
                        src = _snapshotBuf;
                    } else {
                        _LoadTumbSlot(_slot_cursor);
                        src = _thumbnail_image;
                    }

                    blitBufferToBuffer(src, 223, 132, LS_PREVIEW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366, LS_WINDOW_WIDTH);
                    _ShowSlotList(0);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                    _dbleclkcntr = 24;
                }
            }
        }
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    lsgWindowFree(0);

    tileWindowRefresh();

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            _quick_done = true;
        }
    }

    return rc;
}

// 0x47C5B4
int _QuickSnapShot()
{
    _snapshot = (unsigned char*)internal_malloc(LS_PREVIEW_SIZE);
    if (_snapshot == NULL) {
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
    MessageListItem messageListItem;
    const char* messageListItemText;

    const char* body[] = {
        _str1,
        _str2,
    };

    _ls_error_code = 0;

    if (!configGetString(&gGameConfig, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_MASTER_PATCHES_KEY, &_patches)) {
        debugPrint("\nLOADSAVE: Error reading patches config variable! Using default.\n");
        _patches = _emgpath;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        int quickSaveWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
        int quickSaveWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
        int window = windowCreate(quickSaveWindowX,
            quickSaveWindowY,
            LS_WINDOW_WIDTH,
            LS_WINDOW_HEIGHT,
            256,
            WINDOW_FLAG_0x10 | WINDOW_FLAG_0x02);
        if (window != -1) {
            unsigned char* windowBuffer = windowGetBuffer(window);
            bufferFill(windowBuffer, LS_WINDOW_WIDTH, LS_WINDOW_HEIGHT, LS_WINDOW_WIDTH, _colorTable[0]);
            windowRefresh(window);
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
        sprintf(path, "%s\\%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }

        if (window != -1) {
            windowDestroy(window);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        soundPlayFile("iisxxxx1");

        messageListItemText = getmsg(&gLoadSaveMessageList, &messageListItem, 134);
        strcpy(_str0, messageListItemText);

        messageListItemText = getmsg(&gLoadSaveMessageList, &messageListItem, 135);
        strcpy(_str1, messageListItemText);

        showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], 0, _colorTable[32328], DIALOG_BOX_LARGE);

        messageListFree(&gLoadSaveMessageList);

        _map_new_map();

        _game_user_wants_to_quit = 2;

        return -1;
    } else {
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
            soundPlayFile("iisxxxx1");

            const char* text1 = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106);
            strcpy(_str0, text1);

            const char* text2 = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107);
            strcpy(_str1, text1);

            sprintf(_str2, "\"%s\\\"", "SAVEGAME");

            showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], 0, _colorTable[32328], DIALOG_BOX_LARGE);

            return -1;
        }

        int v36 = _LSstatus[_slot_cursor];
        if (v36 != 0 && v36 != 2 && v36 != 3) {
            _LoadTumbSlot(_slot_cursor);
            blitBufferToBuffer(_thumbnail_image, 223, 132, LS_PREVIEW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366, LS_WINDOW_WIDTH);
        } else {
            blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_PREVIEW_COVER],
                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                LS_WINDOW_WIDTH);
        }

        _ShowSlotList(2);
        _DrawInfoBox(_slot_cursor);
        windowRefresh(gLoadSaveWindow);
        _dbleclkcntr = 24;

        int rc = -1;
        while (rc == -1) {
            while (rc == -1) {
                int v37 = _get_time();
                int keyCode = _get_input();
                int v39 = 0;
                int v107 = 0;
                int v108 = -1;

                if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != 0) {
                    rc = 0;
                } else {
                    switch (keyCode) {
                    case KEY_ARROW_UP:
                        if (--_slot_cursor < 0) {
                            _slot_cursor = 0;
                        }
                        v39 = 1;
                        v108 = -1;
                        break;
                    case KEY_ARROW_DOWN:
                        if (++_slot_cursor > 9) {
                            _slot_cursor = 9;
                        }
                        v39 = 1;
                        v108 = -1;
                        break;
                    case KEY_HOME:
                        _slot_cursor = 0;
                        v108 = -1;
                        v39 = 1;
                        break;
                    case KEY_END:
                        v39 = 1;
                        v108 = -1;
                        _slot_cursor = 9;
                        break;
                    case 506:
                        v107 = 1;
                        break;
                    case 504:
                        v107 = 2;
                        break;
                    case 502:
                        do {
                            int mouseX;
                            int mouseY;
                            mouseGetPositionInWindow(gLoadSaveWindow, &mouseX, &mouseY);
                            int v41 = (mouseY - 79) / (3 * fontGetLineHeight() + 4);
                            if (v41 < 0) {
                                v41 = 0;
                            } else if (v41 > 9) {
                                v41 = 9;
                            }

                            _slot_cursor = v41;
                            if (v41 == v108) {
                                soundPlayFile("ib1p1xx1");
                            }

                            v39 = 1;
                            v107 = 0;
                            v108 = _slot_cursor;
                        } while (0);
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
                    if (_LSstatus[_slot_cursor]) {
                        rc = 1;
                    } else {
                        rc = -1;
                    }

                    v39 = 1;
                    v107 = 0;
                }

                if (v107) {
                    unsigned int v42 = 4;
                    int v106 = 0;
                    int v109 = 0;
                    do {
                        int v45 = _get_time();
                        int v44 = v109 + 1;

                        if (v106 == 0 && v44 == 1 || v106 == 1 && v109 > 14.4) {
                            v106 = 1;

                            if (v109 > 14.4) {
                                v42 += 1;
                                if (v42 > 24) {
                                    v42 = 24;
                                }
                            }

                            if (v107 == 1) {
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

                            int v46 = _LSstatus[_slot_cursor];
                            if (v46 != 0 && v46 != 2 && v46 != 3) {
                                _LoadTumbSlot(_slot_cursor);

                                blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 39 + 340,
                                    gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                    gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                                    LS_WINDOW_WIDTH,
                                    gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                                    LS_WINDOW_WIDTH);

                                blitBufferToBuffer(_thumbnail_image,
                                    223,
                                    132,
                                    LS_PREVIEW_WIDTH,
                                    gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                                    LS_WINDOW_WIDTH);
                            } else {
                                blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_PREVIEW_COVER],
                                    gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                    gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                                    gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                    gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                                    LS_WINDOW_WIDTH);
                            }

                            _ShowSlotList(2);
                            _DrawInfoBox(_slot_cursor);
                            windowRefresh(gLoadSaveWindow);
                        }

                        if (v109 > 14.4) {
                            while (getTicksSince(v45) < 1000 / v42) { }
                        } else {
                            while (getTicksSince(v45) < 1000 / 24) { }
                        }

                        keyCode = _get_input();
                    } while (keyCode != 505 && keyCode != 503);
                } else {
                    if (v39 != 0) {
                        int v48 = _LSstatus[_slot_cursor];
                        if (v48 != 0 && v48 != 2 && v48 != 3) {
                            _LoadTumbSlot(_slot_cursor);

                            blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 39 + 340,
                                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                                LS_WINDOW_WIDTH,
                                gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                                LS_WINDOW_WIDTH);

                            blitBufferToBuffer(_thumbnail_image,
                                223,
                                132,
                                LS_PREVIEW_WIDTH,
                                gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                                LS_WINDOW_WIDTH);
                        } else {
                            blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_PREVIEW_COVER],
                                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].height,
                                gLoadSaveFrmSizes[LOAD_SAVE_FRM_PREVIEW_COVER].width,
                                gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                                LS_WINDOW_WIDTH);
                        }
                        _DrawInfoBox(_slot_cursor);
                        _ShowSlotList(2);
                    }

                    windowRefresh(gLoadSaveWindow);

                    _dbleclkcntr -= 1;
                    if (_dbleclkcntr == 0) {
                        _dbleclkcntr = 24;
                        v108 = -1;
                    }

                    while (getTicksSince(v37) < 1000 / 24) { }
                }
            }

            if (rc == 1) {
                int v50 = _LSstatus[_slot_cursor];
                if (v50 == 3) {
                    const char* text;

                    soundPlayFile("iisxxxx1");

                    text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134);
                    strcpy(_str0, text);

                    text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136);
                    strcpy(_str1, text);

                    text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135);
                    strcpy(_str2, text);

                    showDialogBox(_str0, body, 2, 169, 116, _colorTable[32328], 0, _colorTable[32328], DIALOG_BOX_LARGE);

                    rc = -1;
                } else if (v50 == 2) {
                    const char* text;

                    soundPlayFile("iisxxxx1");

                    text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134);
                    strcpy(_str0, text);

                    text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136);
                    strcpy(_str1, text);

                    showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], 0, _colorTable[32328], DIALOG_BOX_LARGE);

                    rc = -1;
                } else {
                    if (lsgLoadGameInSlot(_slot_cursor) == -1) {
                        const char* text;

                        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                        soundPlayFile("iisxxxx1");

                        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134);
                        strcpy(_str0, text);

                        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135);
                        strcpy(_str1, text);

                        showDialogBox(_str0, body, 1, 169, 116, _colorTable[32328], 0, _colorTable[32328], DIALOG_BOX_LARGE);

                        _map_new_map();

                        _game_user_wants_to_quit = 2;

                        rc = -1;
                    }
                }
            }
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
}

// 0x47D2E4
int lsgWindowInit(int windowType)
{
    gLoadSaveWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    gLoadSaveWindowIsoWasEnabled = false;
    if (!messageListInit(&gLoadSaveMessageList)) {
        return -1;
    }

    sprintf(_str, "%s%s", asc_5186C8, LSGAME_MSG_NAME);
    if (!messageListLoad(&gLoadSaveMessageList, _str)) {
        return -1;
    }

    _snapshot = (unsigned char*)internal_malloc(61632);
    if (_snapshot == NULL) {
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
        int fid = buildFid(6, gLoadSaveFrmIds[index], 0, 0, 0);
        gLoadSaveFrmData[index] = artLockFrameDataReturningSize(fid,
            &(gLoadSaveFrmHandles[index]),
            &(gLoadSaveFrmSizes[index].width),
            &(gLoadSaveFrmSizes[index].height));

        if (gLoadSaveFrmData[index] == NULL) {
            while (--index >= 0) {
                artUnlock(gLoadSaveFrmHandles[index]);
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
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
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
    memcpy(gLoadSaveWindowBuffer, gLoadSaveFrmData[LOAD_SAVE_FRM_BACKGROUND], LS_WINDOW_WIDTH * LS_WINDOW_HEIGHT);

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
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        500,
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        495,
        349,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        501,
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        58,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_ARROW_UP_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_ARROW_UP_PRESSED].height,
        -1,
        505,
        506,
        505,
        gLoadSaveFrmData[LOAD_SAVE_FRM_ARROW_UP_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_ARROW_UP_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_ARROW_UP_PRESSED].height + 58,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].height,
        -1,
        503,
        504,
        503,
        gLoadSaveFrmData[LOAD_SAVE_FRM_ARROW_DOWN_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    buttonCreate(gLoadSaveWindow, 55, 87, 230, 353, -1, -1, -1, 502, NULL, NULL, NULL, BUTTON_FLAG_TRANSPARENT);
    fontSetCurrent(101);

    return 0;
}

// 0x47D824
int lsgWindowFree(int windowType)
{
    windowDestroy(gLoadSaveWindow);
    fontSetCurrent(gLoadSaveWindowOldFont);
    messageListFree(&gLoadSaveMessageList);

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        artUnlock(gLoadSaveFrmHandles[index]);
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
int lsgPerformSaveGame()
{
    _ls_error_code = 0;
    _map_backup_count = -1;
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);

    backgroundSoundPause();

    sprintf(_gmpath, "%s\\%s", _patches, "SAVEGAME");
    compat_mkdir(_gmpath);

    sprintf(_gmpath, "%s\\%s\\%s%.2d", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    compat_mkdir(_gmpath);

    strcat(_gmpath, "\\proto");
    compat_mkdir(_gmpath);

    char* protoBasePath = _gmpath + strlen(_gmpath);

    strcpy(protoBasePath, "\\critters");
    compat_mkdir(_gmpath);

    strcpy(protoBasePath, "\\items");
    compat_mkdir(_gmpath);

    if (_SaveBackup() == -1) {
        debugPrint("\nLOADSAVE: Warning, can't backup save file!\n");
    }

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    debugPrint("\nLOADSAVE: Save name: %s\n", _gmpath);

    _flptr = fileOpen(_gmpath, "wb");
    if (_flptr == NULL) {
        debugPrint("\nLOADSAVE: ** Error opening save game for writing! **\n");
        _RestoreSave();
        sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        _MapDirErase(_gmpath, "BAK");
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
        sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        _MapDirErase(_gmpath, "BAK");
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
            sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
            _MapDirErase(_gmpath, "BAK");
            _partyMemberUnPrepSave();
            backgroundSoundResume();
            return -1;
        }

        debugPrint("LOADSAVE: Save function #%d data size written: %d bytes.\n", index, fileTell(_flptr) - pos);
    }

    debugPrint("LOADSAVE: Total save data written: %ld bytes.\n", fileTell(_flptr));

    fileClose(_flptr);

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    _MapDirErase(_gmpath, "BAK");

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
int _isLoadingGame()
{
    return _loadingGame;
}

// 0x47DC68
int lsgLoadGameInSlot(int slot)
{
    _loadingGame = 1;

    if (isInCombat()) {
        interfaceBarEndButtonsHide(false);
        _combat_over_from_load();
        gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    }

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    STRUCT_613D30* ptr = &(_LSData[slot]);
    debugPrint("\nLOADSAVE: Load name: %s\n", ptr->description);

    _flptr = fileOpen(_gmpath, "rb");
    if (_flptr == NULL) {
        debugPrint("\nLOADSAVE: ** Error opening load game file for reading! **\n");
        _loadingGame = 0;
        return -1;
    }

    long pos = fileTell(_flptr);
    if (lsgLoadHeaderInSlot(slot) == -1) {
        debugPrint("\nLOADSAVE: ** Error reading save  game header! **\n");
        fileClose(_flptr);
        gameReset();
        _loadingGame = 0;
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
            _loadingGame = 0;
            return -1;
        }

        debugPrint("LOADSAVE: Load function #%d data size read: %d bytes.\n", index, fileTell(_flptr) - pos);
    }

    debugPrint("LOADSAVE: Total load data read: %ld bytes.\n", fileTell(_flptr));
    fileClose(_flptr);

    sprintf(_str, "%s\\", "MAPS");
    _MapDirErase(_str, "BAK");
    _proto_dude_update_gender();

    // Game Loaded.
    gLoadSaveMessageListItem.num = 141;
    if (messageListGetItem(&gLoadSaveMessageList, &gLoadSaveMessageListItem) == 1) {
        displayMonitorAddMessage(gLoadSaveMessageListItem.text);
    } else {
        debugPrint("\nError: Couldn't find LoadSave Message!");
    }

    _loadingGame = 0;

    return 0;
}

// 0x47DF10
int lsgSaveHeaderInSlot(int slot)
{
    _ls_error_code = 4;

    STRUCT_613D30* ptr = &(_LSData[slot]);
    strncpy(ptr->field_0, "FALLOUT SAVE FILE", 24);

    if (fileWrite(ptr->field_0, 1, 24, _flptr) == -1) {
        return -1;
    }

    short temp[3];
    temp[0] = VERSION_MAJOR;
    temp[1] = VERSION_MINOR;

    ptr->field_18 = temp[0];
    ptr->field_1A = temp[1];

    if (fileWriteInt16List(_flptr, temp, 2) == -1) {
        return -1;
    }

    ptr->field_1C = VERSION_RELEASE;
    if (fileWriteUInt8(_flptr, VERSION_RELEASE) == -1) {
        return -1;
    }

    char* characterName = critterGetName(gDude);
    strncpy(ptr->character_name, characterName, 32);

    if (fileWrite(ptr->character_name, 32, 1, _flptr) != 1) {
        return -1;
    }

    if (fileWrite(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }

    time_t now = time(NULL);
    struct tm* local = localtime(&now);

    temp[0] = local->tm_mday;
    temp[1] = local->tm_mon + 1;
    temp[2] = local->tm_year + 1900;

    ptr->field_5E = temp[0];
    ptr->field_5C = temp[1];
    ptr->field_60 = temp[2];
    ptr->field_64 = local->tm_hour + local->tm_min;

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (_db_fwriteLong(_flptr, ptr->field_64) == -1) {
        return -1;
    }

    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    temp[0] = month;
    temp[1] = day;
    temp[2] = year;
    ptr->field_70 = gameTimeGetTime();

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (_db_fwriteLong(_flptr, ptr->field_70) == -1) {
        return -1;
    }

    ptr->field_74 = gElevation;
    if (fileWriteInt16(_flptr, ptr->field_74) == -1) {
        return -1;
    }

    ptr->field_76 = mapGetCurrentMap();
    if (fileWriteInt16(_flptr, ptr->field_76) == -1) {
        return -1;
    }

    char mapName[128];
    strcpy(mapName, gMapHeader.name);

    char* v1 = _strmfe(_str, mapName, "sav");
    strncpy(ptr->file_name, v1, 16);
    if (fileWrite(ptr->file_name, 16, 1, _flptr) != 1) {
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
int lsgLoadHeaderInSlot(int slot)
{
    _ls_error_code = 3;

    STRUCT_613D30* ptr = &(_LSData[slot]);

    if (fileRead(ptr->field_0, 1, 24, _flptr) != 24) {
        return -1;
    }

    if (strncmp(ptr->field_0, "FALLOUT SAVE FILE", 18) != 0) {
        debugPrint("\nLOADSAVE: ** Invalid save file on load! **\n");
        _ls_error_code = 2;
        return -1;
    }

    short v8[3];
    if (fileReadInt16List(_flptr, v8, 2) == -1) {
        return -1;
    }

    ptr->field_18 = v8[0];
    ptr->field_1A = v8[1];

    if (fileReadUInt8(_flptr, &(ptr->field_1C)) == -1) {
        return -1;
    }

    if (ptr->field_18 != 1 || ptr->field_1A != 2 || ptr->field_1C != 'R') {
        debugPrint("\nLOADSAVE: Load slot #%d Version: %d.%d%c\n", slot, ptr->field_18, ptr->field_1A, ptr->field_1C);
        _ls_error_code = 1;
        return -1;
    }

    if (fileRead(ptr->character_name, 32, 1, _flptr) != 1) {
        return -1;
    }

    if (fileRead(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->field_5C = v8[0];
    ptr->field_5E = v8[1];
    ptr->field_60 = v8[2];

    if (_db_freadInt(_flptr, &(ptr->field_64)) == -1) {
        return -1;
    }

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->field_68 = v8[0];
    ptr->field_6A = v8[1];
    ptr->field_6C = v8[2];

    if (_db_freadInt(_flptr, &(ptr->field_70)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->field_74)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->field_76)) == -1) {
        return -1;
    }

    if (fileRead(ptr->file_name, 1, 16, _flptr) != 16) {
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
int _GetSlotList()
{
    int index = 0;
    for (; index < 10; index += 1) {
        sprintf(_str, "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", index + 1, "SAVE.DAT");

        int fileSize;
        if (dbGetFileSize(_str, &fileSize) != 0) {
            _LSstatus[index] = SLOT_STATE_EMPTY;
        } else {
            _flptr = fileOpen(_str, "rb");

            if (_flptr == NULL) {
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
void _ShowSlotList(int a1)
{
    bufferFill(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 87 + 55, 230, 353, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer[LS_WINDOW_WIDTH * 86 + 55] & 0xFF);

    int y = 87;
    for (int index = 0; index < 10; index += 1) {

        int color = index == _slot_cursor ? _colorTable[32747] : _colorTable[992];
        const char* text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, a1 != 0 ? 110 : 109);
        sprintf(_str, "[   %s %.2d:   ]", text, index + 1);
        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

        y += fontGetLineHeight();
        switch (_LSstatus[index]) {
        case SLOT_STATE_OCCUPIED:
            strcpy(_str, _LSData[index].description);
            break;
        case SLOT_STATE_EMPTY:
            // - EMPTY -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 111);
            sprintf(_str, "       %s", text);
            break;
        case SLOT_STATE_ERROR:
            // - CORRUPT SAVE FILE -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 112);
            sprintf(_str, "%s", text);
            color = _colorTable[32328];
            break;
        case SLOT_STATE_UNSUPPORTED_VERSION:
            // - OLD VERSION -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 113);
            sprintf(_str, " %s", text);
            color = _colorTable[32328];
            break;
        }

        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
        y += 2 * fontGetLineHeight() + 4;
    }
}

// 0x47E8E0
void _DrawInfoBox(int a1)
{
    blitBufferToBuffer(gLoadSaveFrmData[LOAD_SAVE_FRM_BACKGROUND] + LS_WINDOW_WIDTH * 254 + 396, 164, 60, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 254 + 396, 640);

    unsigned char* dest;
    const char* text;
    int color = _colorTable[992];

    switch (_LSstatus[a1]) {
    case SLOT_STATE_OCCUPIED:
        do {
            STRUCT_613D30* ptr = &(_LSData[a1]);
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 254 + 396, ptr->character_name, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            int v4 = ptr->field_70 / 600;
            int v5 = v4 % 60;
            int v6 = 25 * (v4 / 60 % 24);
            int v21 = 4 * v6 + v5;

            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 116 + ptr->field_68);
            sprintf(_str, "%.2d %s %.4d   %.4d", ptr->field_6A, text, ptr->field_6C, v21);

            int v2 = fontGetLineHeight();
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * (256 + v2) + 397, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            const char* v22 = mapGetName(ptr->field_76, ptr->field_74);
            const char* v9 = mapGetCityName(ptr->field_76);
            sprintf(_str, "%s %s", v9, v22);

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
        } while (0);
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
int _LoadTumbSlot(int a1)
{
    File* stream;
    int v2;

    v2 = _LSstatus[_slot_cursor];
    if (v2 != 0 && v2 != 2 && v2 != 3) {
        sprintf(_str, "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", _slot_cursor + 1, "SAVE.DAT");
        debugPrint(" Filename %s\n", _str);

        stream = fileOpen(_str, "rb");
        if (stream == NULL) {
            debugPrint("\nLOADSAVE: ** (A) Error reading thumbnail #%d! **\n", a1);
            return -1;
        }

        if (fileSeek(stream, 131, SEEK_SET) != 0) {
            debugPrint("\nLOADSAVE: ** (B) Error reading thumbnail #%d! **\n", a1);
            fileClose(stream);
            return -1;
        }

        if (fileRead(_thumbnail_image, LS_PREVIEW_SIZE, 1, stream) != 1) {
            debugPrint("\nLOADSAVE: ** (C) Error reading thumbnail #%d! **\n", a1);
            fileClose(stream);
            return -1;
        }

        fileClose(stream);
    }

    return 0;
}

// 0x47ED5C
int _GetComment(int a1)
{
    // Maintain original position in original resolution, otherwise center it.
    int commentWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width) / 2
        : LS_COMMENT_WINDOW_X;
    int commentWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].height) / 2
        : LS_COMMENT_WINDOW_Y;
    int window = windowCreate(commentWindowX,
        commentWindowY,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].height,
        256,
        WINDOW_FLAG_0x10 | WINDOW_FLAG_0x04);
    if (window == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        gLoadSaveFrmData[LOAD_SAVE_FRM_BOX],
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].height * gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width);

    fontSetCurrent(103);

    const char* msg;

    // DONE
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 104);
    fontDrawText(windowBuffer + gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width * 57 + 56,
        msg,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        _colorTable[18979]);

    // CANCEL
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 105);
    fontDrawText(windowBuffer + gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width * 57 + 181,
        msg,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        _colorTable[18979]);

    // DESCRIPTION
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 130);

    char title[260];
    strcpy(title, msg);

    int width = fontGetStringWidth(title);
    fontDrawText(windowBuffer + gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width * 7 + (gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width - width) / 2,
        title,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_BOX].width,
        _colorTable[18979]);

    fontSetCurrent(101);

    int btn;

    // DONE
    btn = buttonCreate(window,
        34,
        58,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        507,
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // CANCEL
    btn = buttonCreate(window,
        160,
        58,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].width,
        gLoadSaveFrmSizes[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].height,
        -1,
        -1,
        -1,
        508,
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_NORMAL],
        gLoadSaveFrmData[LOAD_SAVE_FRM_RED_BUTTON_PRESSED],
        NULL,
        BUTTON_FLAG_TRANSPARENT);
    if (btn == -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
        strncpy(description, _LSData[a1].description, LOAD_SAVE_DESCRIPTION_LENGTH);
    } else {
        memset(description, '\0', LOAD_SAVE_DESCRIPTION_LENGTH);
    }

    int rc;

    if (_get_input_str2(window, 507, 508, description, LOAD_SAVE_DESCRIPTION_LENGTH - 1, 24, 35, _colorTable[992], gLoadSaveFrmData[LOAD_SAVE_FRM_BOX][gLoadSaveFrmSizes[1].width * 35 + 24], 0) == 0) {
        strncpy(_LSData[a1].description, description, LOAD_SAVE_DESCRIPTION_LENGTH);
        _LSData[a1].description[LOAD_SAVE_DESCRIPTION_LENGTH - 1] = '\0';
        rc = 1;
    } else {
        rc = 0;
    }

    windowDestroy(window);

    return rc;
}

// 0x47F084
int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
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

    int textLength = strlen(text);
    text[textLength] = ' ';
    text[textLength + 1] = '\0';

    int nameWidth = fontGetStringWidth(text);

    bufferFill(windowBuffer + windowWidth * y + x, nameWidth, lineHeight, windowWidth, backgroundColor);
    fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);

    windowRefresh(win);

    int blinkCounter = 3;
    bool blink = false;

    int v1 = 0;

    int rc = 1;
    while (rc == 1) {
        int tick = _get_time();

        int keyCode = _get_input();
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

        while (getTicksSince(tick) < 1000 / 24) {
        }
    }

    if (rc == 0) {
        text[textLength] = '\0';
        strcpy(description, text);
    }

    return rc;
}

// 0x47F48C
int _DummyFunc(File* stream)
{
    return 0;
}

// 0x47F490
int _PrepLoad(File* stream)
{
    gameReset();
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    gMapHeader.name[0] = '\0';
    gameTimeSetTime(_LSData[_slot_cursor].field_70);
    return 0;
}

// 0x47F4C8
int _EndLoad(File* stream)
{
    worldmapStartMapMusic();
    dudeSetName(_LSData[_slot_cursor].character_name);
    interfaceBarRefresh();
    indicatorBarRefresh();
    tileWindowRefresh();
    if (isInCombat()) {
        scriptsRequestCombat(NULL);
    }
    return 0;
}

// 0x47F510
int _GameMap2Slot(File* stream)
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

        const char* critterItemPath = (pid >> 24) == OBJ_TYPE_CRITTER ? "PROTO\\CRITTERS" : "PROTO\\ITEMS";
        sprintf(_str0, "%s\\%s\\%s", _patches, critterItemPath, path);
        sprintf(_str1, "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, critterItemPath, path);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            return -1;
        }
    }

    sprintf(_str0, "%s\\*.%s", "MAPS", "SAV");

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

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    if (_MapDirErase(_gmpath, "SAV") == -1) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    strcat(_gmpath, _str0);
    remove(_gmpath);

    for (int index = 0; index < fileNameListLength; index += 1) {
        char* string = fileNameList[index];
        if (fileWrite(string, strlen(string) + 1, 1, stream) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }

        sprintf(_str0, "%s\\%s\\%s", _patches, "MAPS", string);
        sprintf(_str1, "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, string);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }
    }

    fileNameListFree(&fileNameList, 0);

    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    sprintf(_str1, "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, _str0);
    sprintf(_str0, "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");

    if (fileCopyCompressed(_str0, _str1) == -1) {
        return -1;
    }

    sprintf(_str0, "%s\\%s", "MAPS", "AUTOMAP.DB");
    File* inStream = fileOpen(_str0, "rb");
    if (inStream == NULL) {
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
int _SlotMap2Game(File* stream)
{
    debugPrint("LOADSAVE: in SlotMap2Game\n");

    int v2;
    if (fileReadInt32(stream, &v2) == 1) {
        debugPrint("LOADSAVE: returning 1\n");
        return -1;
    }

    if (v2 == 0) {
        debugPrint("LOADSAVE: returning 2\n");
        return -1;
    }

    sprintf(_str0, "%s\\", "PROTO\\CRITTERS");

    if (_MapDirErase(_str0, "PRO") == -1) {
        debugPrint("LOADSAVE: returning 3\n");
        return -1;
    }

    sprintf(_str0, "%s\\", "PROTO\\ITEMS");
    if (_MapDirErase(_str0, "PRO") == -1) {
        debugPrint("LOADSAVE: returning 4\n");
        return -1;
    }

    sprintf(_str0, "%s\\", "MAPS");
    if (_MapDirErase(_str0, "SAV") == -1) {
        debugPrint("LOADSAVE: returning 5\n");
        return -1;
    }

    sprintf(_str0, "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    remove(_str0);

    if (gPartyMemberDescriptionsLength > 1) {
        for (int index = 1; index < gPartyMemberDescriptionsLength; index += 1) {
            int pid = gPartyMemberPids[index];
            if (pid != -2) {
                char protoPath[COMPAT_MAX_PATH];
                if (_proto_list_str(pid, protoPath) == 0) {
                    const char* basePath = pid >> 24 == OBJ_TYPE_CRITTER
                        ? "PROTO\\CRITTERS"
                        : "PROTO\\ITEMS";
                    sprintf(_str0, "%s\\%s\\%s", _patches, basePath, protoPath);
                    sprintf(_str1, "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, basePath, protoPath);

                    if (_gzdecompress_file(_str1, _str0) == -1) {
                        debugPrint("LOADSAVE: returning 6\n");
                        return -1;
                    }
                }
            }
        }
    }

    if (v2 > 0) {
        for (int index = 0; index < v2; index += 1) {
            char v11[64]; // TODO: Size is probably wrong.
            if (_mygets(v11, stream) == -1) {
                break;
            }

            sprintf(_str0, "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, v11);
            sprintf(_str1, "%s\\%s\\%s", _patches, "MAPS", v11);

            if (_gzdecompress_file(_str0, _str1) == -1) {
                debugPrint("LOADSAVE: returning 7\n");
                return -1;
            }
        }
    }

    const char* v9 = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    sprintf(_str0, "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, v9);
    sprintf(_str1, "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    if (fileCopyDecompressed(_str0, _str1) == -1) {
        debugPrint("LOADSAVE: returning 8\n");
        return -1;
    }

    sprintf(_str1, "%s\\%s", "MAPS", "AUTOMAP.DB");

    int v12;
    if (fileReadInt32(stream, &v12) == -1) {
        debugPrint("LOADSAVE: returning 9\n");
        return -1;
    }

    if (mapLoadSaved(_LSData[_slot_cursor].file_name) == -1) {
        debugPrint("LOADSAVE: returning 13\n");
        return -1;
    }

    return 0;
}

// 0x47FE14
int _mygets(char* dest, File* stream)
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
int _copy_file(const char* a1, const char* a2)
{
    File* stream1;
    File* stream2;
    int length;
    int chunk_length;
    void* buf;
    int result;

    stream1 = NULL;
    stream2 = NULL;
    buf = NULL;
    result = -1;

    stream1 = fileOpen(a1, "rb");
    if (stream1 == NULL) {
        goto out;
    }

    length = fileGetSize(stream1);
    if (length == -1) {
        goto out;
    }

    stream2 = fileOpen(a2, "wb");
    if (stream2 == NULL) {
        goto out;
    }

    buf = internal_malloc(0xFFFF);
    if (buf == NULL) {
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

    if (stream1 != NULL) {
        fileClose(stream1);
    }

    if (stream2 != NULL) {
        fileClose(stream1);
    }

    if (buf != NULL) {
        internal_free(buf);
    }

    return result;
}

// InitLoadSave
// 0x48000C
void lsgInit()
{
    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s\\", "MAPS");
    _MapDirErase(path, "SAV");
}

// 0x480040
int _MapDirErase(const char* relativePath, const char* extension)
{
    char path[COMPAT_MAX_PATH];
    sprintf(path, "%s*.%s", relativePath, extension);

    char** fileList;
    int fileListLength = fileNameListInit(path, &fileList, 0, 0);
    while (--fileListLength >= 0) {
        sprintf(path, "%s\\%s%s", _patches, relativePath, fileList[fileListLength]);
        remove(path);
    }
    fileNameListFree(&fileList, 0);

    return 0;
}

// 0x4800C8
int _MapDirEraseFile_(const char* a1, const char* a2)
{
    char path[COMPAT_MAX_PATH];

    sprintf(path, "%s\\%s%s", _patches, a1, a2);
    if (remove(path) != 0) {
        return -1;
    }

    return 0;
}

// 0x480104
int _SaveBackup()
{
    debugPrint("\nLOADSAVE: Backing up save slot files..\n");

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);

    strcat(_str0, "SAVE.DAT");

    _strmfe(_str1, _str0, "BAK");

    File* stream1 = fileOpen(_str0, "rb");
    if (stream1 != NULL) {
        fileClose(stream1);
        if (rename(_str0, _str1) != 0) {
            return -1;
        }
    }

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    sprintf(_str0, "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList, 0, 0);
    if (fileListLength == -1) {
        return -1;
    }

    _map_backup_count = fileListLength;

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);

        _strmfe(_str1, _str0, "BAK");
        if (rename(_str0, _str1) != 0) {
            fileNameListFree(&fileList, 0);
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    debugPrint("\nLOADSAVE: %d map files backed up.\n", fileListLength);

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    sprintf(_str0, "%s\\%s", _gmpath, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    sprintf(_str1, "%s\\%s", _gmpath, v2);

    _automap_db_flag = 0;

    File* stream2 = fileOpen(_str0, "rb");
    if (stream2 != NULL) {
        fileClose(stream2);

        if (_copy_file(_str0, _str1) == -1) {
            return -1;
        }

        _automap_db_flag = 1;
    }

    return 0;
}

// 0x4803D8
int _RestoreSave()
{
    debugPrint("\nLOADSAVE: Restoring save file backup...\n");

    _EraseSave();

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);
    strcat(_str0, "SAVE.DAT");
    _strmfe(_str1, _str0, "BAK");
    remove(_str0);

    if (rename(_str1, _str0) != 0) {
        _EraseSave();
        return -1;
    }

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    sprintf(_str0, "%s*.%s", _gmpath, "BAK");

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

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);
        _strmfe(_str1, _str0, "SAV");
        remove(_str1);
        if (rename(_str0, _str1) != 0) {
            // FIXME: Probably leaks fileList.
            _EraseSave();
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    if (!_automap_db_flag) {
        return 0;
    }

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    strcpy(_str0, _gmpath);
    strcat(_str0, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    strcpy(_str1, _gmpath);
    strcat(_str1, v2);

    if (rename(_str0, _str1) != 0) {
        _EraseSave();
        return -1;
    }

    return 0;
}

// 0x480710
int _LoadObjDudeCid(File* stream)
{
    int value;

    if (fileReadInt32(stream, &value) == -1) {
        return -1;
    }

    gDude->cid = value;

    return 0;
}

// 0x480734
int _SaveObjDudeCid(File* stream)
{
    return fileWriteInt32(stream, gDude->cid);
}

// 0x480754
int _EraseSave()
{
    debugPrint("\nLOADSAVE: Erasing save(bad) slot...\n");

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);
    strcat(_str0, "SAVE.DAT");
    remove(_str0);

    sprintf(_gmpath, "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    sprintf(_str0, "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList, 0, 0);
    if (fileListLength == -1) {
        return -1;
    }

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);
        remove(_str0);
    }

    fileNameListFree(&fileList, 0);

    sprintf(_gmpath, "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    strcpy(_str0, _gmpath);
    strcat(_str0, v1);

    remove(_str0);

    return 0;
}
