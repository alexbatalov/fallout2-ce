#ifndef LOAD_SAVE_GAME_H
#define LOAD_SAVE_GAME_H

#include "art.h"
#include "db.h"
#include "geometry.h"
#include "message.h"
#include "platform_compat.h"

#define LOAD_SAVE_DESCRIPTION_LENGTH (30)
#define LOAD_SAVE_HANDLER_COUNT (27)

typedef enum LoadSaveMode {
    // Special case - loading game from main menu.
    LOAD_SAVE_MODE_FROM_MAIN_MENU,

    // Normal (full-screen) save/load screen.
    LOAD_SAVE_MODE_NORMAL,

    // Quick load/save.
    LOAD_SAVE_MODE_QUICK,
} LoadSaveMode;

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

typedef int LoadGameHandler(File* stream);
typedef int SaveGameHandler(File* stream);

#define LSGAME_MSG_NAME ("LSGAME.MSG")

typedef struct STRUCT_613D30 {
    char field_0[24];
    short field_18;
    short field_1A;
    // TODO: The type is probably char, but it's read with the same function as
    // reading unsigned chars, which in turn probably result of collapsing
    // reading functions.
    unsigned char field_1C;
    char character_name[32];
    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    short field_5C;
    short field_5E;
    short field_60;
    int field_64;
    short field_68;
    short field_6A;
    short field_6C;
    int field_70;
    short field_74;
    short field_76;
    char file_name[16];
} STRUCT_613D30;

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

extern const int gLoadSaveFrmIds[LOAD_SAVE_FRM_COUNT];

extern int _slot_cursor;
extern bool _quick_done;
extern bool gLoadSaveWindowIsoWasEnabled;
extern int _map_backup_count;
extern int _automap_db_flag;
extern char* _patches;
extern char _emgpath[];
extern SaveGameHandler* _master_save_list[LOAD_SAVE_HANDLER_COUNT];
extern LoadGameHandler* _master_load_list[LOAD_SAVE_HANDLER_COUNT];
extern int _loadingGame;

extern Size gLoadSaveFrmSizes[LOAD_SAVE_FRM_COUNT];
extern MessageList gLoadSaveMessageList;
extern STRUCT_613D30 _LSData[10];
extern int _LSstatus[10];
extern unsigned char* _thumbnail_image;
extern unsigned char* _snapshotBuf;
extern MessageListItem gLoadSaveMessageListItem;
extern int _dbleclkcntr;
extern int gLoadSaveWindow;
extern unsigned char* gLoadSaveFrmData[LOAD_SAVE_FRM_COUNT];
extern unsigned char* _snapshot;
extern char _str2[COMPAT_MAX_PATH];
extern char _str0[COMPAT_MAX_PATH];
extern char _str1[COMPAT_MAX_PATH];
extern char _str[COMPAT_MAX_PATH];
extern unsigned char* gLoadSaveWindowBuffer;
extern char _gmpath[COMPAT_MAX_PATH];
extern File* _flptr;
extern int _ls_error_code;
extern int gLoadSaveWindowOldFont;
extern CacheEntry* gLoadSaveFrmHandles[LOAD_SAVE_FRM_COUNT];

void _InitLoadSave();
void _ResetLoadSave();
int lsgSaveGame(int mode);
int _QuickSnapShot();
int lsgLoadGame(int mode);
int lsgWindowInit(int windowType);
int lsgWindowFree(int windowType);
int lsgPerformSaveGame();
int _isLoadingGame();
int lsgLoadGameInSlot(int slot);
int lsgSaveHeaderInSlot(int slot);
int lsgLoadHeaderInSlot(int slot);
int _GetSlotList();
void _ShowSlotList(int a1);
void _DrawInfoBox(int a1);
int _LoadTumbSlot(int a1);
int _GetComment(int a1);
int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
int _DummyFunc(File* stream);
int _PrepLoad(File* stream);
int _EndLoad(File* stream);
int _GameMap2Slot(File* stream);
int _SlotMap2Game(File* stream);
int _mygets(char* dest, File* stream);
int _copy_file(const char* a1, const char* a2);
void lsgInit();
int _MapDirErase(const char* path, const char* a2);
int _MapDirEraseFile_(const char* a1, const char* a2);
int _SaveBackup();
int _RestoreSave();
int _LoadObjDudeCid(File* stream);
int _SaveObjDudeCid(File* stream);
int _EraseSave();

#endif /* LOAD_SAVE_GAME_H */
