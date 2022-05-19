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

extern const int gDialogBoxBackgroundFrmIds[DIALOG_TYPE_COUNT];
extern const int _ytable[DIALOG_TYPE_COUNT];
extern const int _xtable[DIALOG_TYPE_COUNT];
extern const int _doneY[DIALOG_TYPE_COUNT];
extern const int _doneX[DIALOG_TYPE_COUNT];
extern const int _dblines[DIALOG_TYPE_COUNT];
extern int _flgids[7];
extern int _flgids2[7];

int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags);
int _save_file_dialog(char* a1, char** fileList, char* fileName, int fileListLength, int x, int y, int flags);
void _PrntFlist(unsigned char* buffer, char** fileList, int pageOffset, int fileListLength, int selectedIndex, int pitch);

#endif /* DBOX_H */
