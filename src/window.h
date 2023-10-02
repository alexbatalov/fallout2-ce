#ifndef WINDOW_H
#define WINDOW_H

#include "geometry.h"
#include "interpreter.h"
#include "region.h"
#include "window_manager.h"

namespace fallout {

typedef void (*WINDOWDRAWINGPROC)(unsigned char* src, int src_pitch, int a3, int src_x, int src_y, int src_width, int src_height, int dest_x, int dest_y);
typedef void WindowDrawingProc2(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, unsigned char a10);
typedef bool(WindowInputHandler)(int key);
typedef void(WindowDeleteCallback)(int windowIndex, const char* windowName);
typedef void(DisplayInWindowCallback)(int windowIndex, const char* windowName, unsigned char* data, int width, int height);
typedef void(ManagedButtonMouseEventCallback)(void* userData, int eventType);
typedef void(ManagedWindowCreateCallback)(int windowIndex, const char* windowName, int* flagsPtr);

typedef enum TextAlignment {
    TEXT_ALIGNMENT_LEFT,
    TEXT_ALIGNMENT_RIGHT,
    TEXT_ALIGNMENT_CENTER,
} TextAlignment;

typedef enum ManagedButtonMouseEvent {
    MANAGED_BUTTON_MOUSE_EVENT_BUTTON_DOWN,
    MANAGED_BUTTON_MOUSE_EVENT_BUTTON_UP,
    MANAGED_BUTTON_MOUSE_EVENT_ENTER,
    MANAGED_BUTTON_MOUSE_EVENT_EXIT,
    MANAGED_BUTTON_MOUSE_EVENT_COUNT,
} ManagedButtonMouseEvent;

typedef enum ManagedButtonRightMouseEvent {
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_BUTTON_DOWN,
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_BUTTON_UP,
    MANAGED_BUTTON_RIGHT_MOUSE_EVENT_COUNT,
} ManagedButtonRightMouseEvent;

int windowGetFont();
int windowSetFont(int a1);
void windowResetTextAttributes();
int windowGetTextFlags();
int windowSetTextFlags(int a1);
unsigned char windowGetTextColor();
unsigned char windowGetHighlightColor();
int windowSetTextColor(float a1, float a2, float a3);
int windowSetHighlightColor(float a1, float a2, float a3);
bool _checkRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool _windowCheckRegion(int windowIndex, int mouseX, int mouseY, int mouseEvent);
bool _windowRefreshRegions();
bool _checkAllRegions();
void _windowAddInputFunc(WindowInputHandler* handler);
void _doRegionRightFunc(Region* region, int a2);
void _doRegionFunc(Region* region, int a2);
bool _windowActivateRegion(const char* regionName, int a2);
int _getInput();
void _doButtonOn(int btn, int keyCode);
void sub_4B6F68(int btn, int mouseEvent);
void _doButtonOff(int btn, int keyCode);
void _doButtonPress(int btn, int keyCode);
void _doButtonRelease(int btn, int keyCode);
void _doRightButtonPress(int btn, int keyCode);
void sub_4B704C(int btn, int mouseEvent);
void _doRightButtonRelease(int btn, int keyCode);
void _setButtonGFX(int width, int height, unsigned char* normal, unsigned char* pressed, unsigned char* a5);
bool _windowHide();
bool _windowShow();
int _windowWidth();
int _windowHeight();
bool _windowDraw();
bool _deleteWindow(const char* windowName);
int sub_4B7AC4(const char* windowName, int x, int y, int width, int height);
int sub_4B7E7C(const char* windowName, int x, int y, int width, int height);
int _createWindow(const char* windowName, int x, int y, int width, int height, int a6, int flags);
int _windowOutput(char* string);
bool _windowGotoXY(int x, int y);
bool _selectWindowID(int index);
int _selectWindow(const char* windowName);
unsigned char* _windowGetBuffer();
int _pushWindow(const char* windowName);
int _popWindow();
void _windowPrintBuf(int win, char* string, int stringLength, int width, int maxY, int x, int y, int flags, int textAlignment);
char** _windowWordWrap(char* string, int maxLength, int a3, int* substringListLengthPtr);
void _windowFreeWordList(char** substringList, int substringListLength);
void _windowWrapLineWithSpacing(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment, int a9);
void _windowWrapLine(int win, char* string, int width, int height, int x, int y, int flags, int textAlignment);
bool _windowPrintRect(char* string, int a2, int textAlignment);
bool _windowFormatMessage(char* string, int x, int y, int width, int height, int textAlignment);
bool _windowPrint(char* string, int a2, int x, int y, int a5);
void _displayInWindow(unsigned char* data, int width, int height, int pitch);
void _displayFile(char* fileName);
void _displayFileRaw(char* fileName);
bool _windowDisplay(char* fileName, int x, int y, int width, int height);
bool _windowDisplayBuf(unsigned char* src, int srcWidth, int srcHeight, int destX, int destY, int destWidth, int destHeight);
int _windowGetXres();
int _windowGetYres();
void _removeProgramReferences_3(Program* program);
void _initWindow(int resolution, int a2);
void _windowClose();
bool _windowDeleteButton(const char* buttonName);
bool _windowSetButtonFlag(const char* buttonName, int value);
bool _windowAddButton(const char* buttonName, int x, int y, int width, int height, int flags);
bool _windowAddButtonGfx(const char* buttonName, char* pressedFileName, char* normalFileName, char* hoverFileName);
bool _windowAddButtonProc(const char* buttonName, Program* program, int mouseEnterProc, int mouseExitProc, int mouseDownProc, int mouseUpProc);
bool _windowAddButtonRightProc(const char* buttonName, Program* program, int rightMouseDownProc, int rightMouseUpProc);
bool _windowAddButtonCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool _windowAddButtonRightCfunc(const char* buttonName, ManagedButtonMouseEventCallback* callback, void* userData);
bool _windowAddButtonText(const char* buttonName, const char* text);
bool _windowAddButtonTextWithOffsets(const char* buttonName, const char* text, int pressedImageOffsetX, int pressedImageOffsetY, int normalImageOffsetX, int normalImageOffsetY);
bool _windowFill(float r, float g, float b);
bool _windowFillRect(int x, int y, int width, int height, float r, float g, float b);
void _windowEndRegion();
bool _windowCheckRegionExists(const char* regionName);
bool _windowStartRegion(int initialCapacity);
bool _windowAddRegionPoint(int x, int y, bool a3);
bool _windowAddRegionProc(const char* regionName, Program* program, int a3, int a4, int a5, int a6);
bool _windowAddRegionRightProc(const char* regionName, Program* program, int a3, int a4);
bool _windowSetRegionFlag(const char* regionName, int value);
bool _windowAddRegionName(const char* regionName);
bool _windowDeleteRegion(const char* regionName);
void _updateWindows();
int _windowMoviePlaying();
bool _windowSetMovieFlags(int flags);
bool _windowPlayMovie(char* filePath);
bool _windowPlayMovieRect(char* filePath, int a2, int a3, int a4, int a5);
void _windowStopMovie();
void _drawScaled(unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* src, int srcWidth, int srcHeight, int srcPitch);
void _drawScaledBuf(unsigned char* dest, int destWidth, int destHeight, unsigned char* src, int srcWidth, int srcHeight);
void _alphaBltBuf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* alphaWindowBuffer, unsigned char* alphaBuffer, unsigned char* dest, int destPitch);
void _fillBuf3x3(unsigned char* src, int srcWidth, int srcHeight, unsigned char* dest, int destWidth, int destHeight);

bool _windowShowNamed(const char* name);

} // namespace fallout

#endif /* WINDOW_H */
