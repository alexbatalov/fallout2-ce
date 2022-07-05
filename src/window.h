#ifndef WINDOW_H
#define WINDOW_H

#include "region.h"

typedef void (*WINDOWDRAWINGPROC)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
typedef void WindowDrawingProc2(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, unsigned char a10);

typedef enum TextAlignment {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_RIGHT,
    TEXT_ALIGNMENT_CENTER,
} TextAlignment;

extern int _currentHighlightColorR;
extern int gWidgetFont;
extern int _currentTextColorG;
extern int _currentTextColorB;
extern int gWidgetTextFlags;
extern int _currentTextColorR;
extern int _currentHighlightColorG;
extern int _currentHighlightColorB;

bool _windowDraw();
bool _selectWindowID(int index);
void _windowPrintBuf(int win, char* string, int stringLength, int width, int maxY, int x, int y, int flags, int textAlignment);
char** _windowWordWrap(char* string, int maxLength, int a3, int* substringListLengthPtr);
void _windowFreeWordList(char** substringList, int substringListLength);
void _windowWrapLine(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment);
bool _windowPrintRect(char* string, int a2, int textAlignment);
bool _windowFormatMessage(char* string, int x, int y, int width, int height, int textAlignment);
int _windowGetYres();
void _initWindow(int resolution, int a2);
void _windowClose();
bool _windowDeleteButton(const char* buttonName);
bool _windowSetButtonFlag(const char* buttonName, int value);
bool _windowAddButtonProc(const char* buttonName, Program* program, int a3, int a4, int a5, int a6);
bool _windowCheckRegionExists(const char* regionName);
bool _windowAddRegionProc(const char* regionName, Program* program, int a3, int a4, int a5, int a6);
bool _windowAddRegionRightProc(const char* regionName, Program* program, int a3, int a4);
bool _windowSetRegionFlag(const char* regionName, int value);
bool _windowDeleteRegion(const char* regionName);
void _updateWindows();
bool _windowSetMovieFlags(int flags);
void _windowStopMovie();

#endif /* WINDOW_H */
