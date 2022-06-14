#ifndef DBOX_H
#define DBOX_H

typedef enum DialogBoxOptions {
    DIALOG_BOX_LARGE = 0x01,
    DIALOG_BOX_MEDIUM = 0x02,
    DIALOG_BOX_NO_HORIZONTAL_CENTERING = 0x04,
    DIALOG_BOX_NO_VERTICAL_CENTERING = 0x08,
    DIALOG_BOX_YES_NO = 0x10,
    DIALOG_BOX_0x20 = 0x20,
} DialogBoxOptions;

typedef enum DialogType {
    DIALOG_TYPE_MEDIUM,
    DIALOG_TYPE_LARGE,
    DIALOG_TYPE_COUNT,
} DialogType;

typedef enum FileDialogFrm {
    FILE_DIALOG_FRM_BACKGROUND,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_NORMAL,
    FILE_DIALOG_FRM_LITTLE_RED_BUTTON_PRESSED,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_DOWN_ARROW_PRESSED,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_NORMAL,
    FILE_DIALOG_FRM_SCROLL_UP_ARROW_PRESSED,
    FILE_DIALOG_FRM_COUNT,
} FileDialogFrm;

typedef enum FileDialogScrollDirection {
    FILE_DIALOG_SCROLL_DIRECTION_NONE,
    FILE_DIALOG_SCROLL_DIRECTION_UP,
    FILE_DIALOG_SCROLL_DIRECTION_DOWN,
} FileDialogScrollDirection;

extern const int gDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT];
extern const int _ytable[DIALOG_TYPE_COUNT];
extern const int _xtable[DIALOG_TYPE_COUNT];
extern const int _doneY[DIALOG_TYPE_COUNT];
extern const int _doneX[DIALOG_TYPE_COUNT];
extern const int _dblines[DIALOG_TYPE_COUNT];
extern int gLoadFileDialogFrmIds[FILE_DIALOG_FRM_COUNT];
extern int gSaveFileDialogFrmIds[FILE_DIALOG_FRM_COUNT];

int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags);
int showLoadFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);
void fileDialogRenderFileList(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

#endif /* DBOX_H */
