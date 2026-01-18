#ifndef DBOX_H
#define DBOX_H

namespace fallout {

typedef enum DialogBoxOptions {
    DIALOG_BOX_LARGE = 0x01, // sfall calls this NORMAL
    DIALOG_BOX_MEDIUM = 0x02, // sfall: SMALL
    DIALOG_BOX_NO_HORIZONTAL_CENTERING = 0x04, // sfall: ALIGN_LEFT
    DIALOG_BOX_NO_VERTICAL_CENTERING = 0x08, // sfall: ALIGN_TOP
    DIALOG_BOX_YES_NO = 0x10,
    DIALOG_BOX_NO_BUTTONS = 0x20, // sfall: CLEAN
} DialogBoxOptions;

int showDialogBox(const char* title, const char** body, int bodyLength, int x, int y, int titleColor, const char* a8, int bodyColor, int flags);
int showLoadFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);
int showSaveFileDialog(char* title, char** fileList, char* dest, int fileListLength, int x, int y, int flags);

} // namespace fallout

#endif /* DBOX_H */
