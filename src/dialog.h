#ifndef DIALOG_H
#define DIALOG_H

#include "interpreter.h"

namespace fallout {

typedef void DialogFunc1(int win);
typedef void DialogFunc2(int win);

typedef struct STRUCT_56DAE0_FIELD_4_FIELD_C {
    char* field_0;
    union {
        int proc;
        char* string;
    };
    int kind;
    int field_C;
    int field_10;
    int field_14;
    short field_18;
    short field_1A;
} STRUCT_56DAE0_FIELD_4_FIELD_C;

typedef struct STRUCT_56DAE0_FIELD_4 {
    void* field_0;
    char* field_4;
    void* field_8;
    STRUCT_56DAE0_FIELD_4_FIELD_C* field_C;
    int field_10;
    int field_14;
    int field_18; // probably font number
} STRUCT_56DAE0_FIELD_4;

typedef struct STRUCT_56DAE0 {
    Program* field_0;
    STRUCT_56DAE0_FIELD_4* field_4;
    int field_8;
    int field_C;
    int field_10;
    int field_14;
    int field_18;
} STRUCT_56DAE0;

extern const float flt_501623;
extern const float flt_501627;

extern int _tods;
extern int _topDialogLine;
extern int _topDialogReply;
extern DialogFunc1* _replyWinDrawCallback;
extern DialogFunc2* _optionsWinDrawCallback;
extern int gDialogBorderX;
extern int gDialogBorderY;
extern int gDialogOptionSpacing;
extern int _replyRGBset;
extern int _optionRGBset;
extern int _exitDialog;
extern int _inDialog;
extern int _mediaFlag;

extern STRUCT_56DAE0 _dialog[4];
extern short word_56DB60;
extern int dword_56DB64;
extern int dword_56DB68;
extern int dword_56DB6C;
extern int dword_56DB70;
extern char* off_56DB74;
extern int dword_56DB7C;
extern int dword_56DB80;
extern int dword_56DB84;
extern int dword_56DB88;
extern char* off_56DB8C;
extern int _replyPlaying;
extern int _replyWin;
extern int gDialogReplyColorG;
extern int gDialogReplyColorB;
extern int gDialogOptionColorG;
extern int gDialogReplyColorR;
extern int gDialogOptionColorB;
extern int gDialogOptionColorR;
extern int _downButton;
extern int dword_56DBB8;
extern int dword_56DBBC;
extern char* off_56DBC0;
extern char* off_56DBC4;
extern char* off_56DBC8;
extern char* off_56DBCC;
extern char* gDialogReplyTitle;
extern int _upButton;
extern int dword_56DBD8;
extern int dword_56DBDC;
extern char* off_56DBE0;
extern char* off_56DBE4;
extern char* off_56DBE8;
extern char* off_56DBEC;

STRUCT_56DAE0_FIELD_4* _getReply();
void _replyAddOption(const char* a1, const char* a2, int a3);
void _replyAddOptionProc(const char* a1, int a2, int a3);
void _optionFree(STRUCT_56DAE0_FIELD_4_FIELD_C* a1);
void _replyFree();
int _endDialog();
void _printLine(int win, char** strings, int strings_num, int a4, int a5, int a6, int a7, int a8, int a9);
void _printStr(int win, char* a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
int _abortReply(int a1);
void _endReply();
void _drawStr(int win, char* a2, int font, int width, int height, int left, int top, int a8, int a9, int a10);
int _dialogStart(Program* a1);
int _dialogRestart();
int _dialogGotoReply(const char* a1);
int dialogSetReplyTitle(const char* a1);
int _dialogReply(const char* a1, const char* a2);
int _dialogOption(const char* a1, const char* a2);
int _dialogOptionProc(const char* a1, int a2);
int sub_430FD4(const char* a1, const char* a2, int timeout);
int sub_431088(int a1);
int _dialogGetExitPoint();
int _dialogQuit();
int dialogSetOptionWindow(int a1, int a2, int a3, int a4, char* a5);
int dialogSetReplyWindow(int a1, int a2, int a3, int a4, char* a5);
int dialogSetBorder(int a1, int a2);
int _dialogSetScrollUp(int a1, int a2, char* a3, char* a4, char* a5, char* a6, int a7);
int _dialogSetScrollDown(int a1, int a2, char* a3, char* a4, char* a5, char* a6, int a7);
int dialogSetOptionSpacing(int value);
int dialogSetOptionColor(float a1, float a2, float a3);
int dialogSetReplyColor(float a1, float a2, float a3);
int _dialogSetOptionFlags(int flags);
void dialogInit();
void _dialogClose();
int _dialogGetDialogDepth();
void _dialogRegisterWinDrawCallbacks(DialogFunc1* a1, DialogFunc2* a2);
int _dialogToggleMediaFlag(int a1);
int _dialogGetMediaFlag();

} // namespace fallout

#endif /* DIALOG_H */
