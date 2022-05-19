#ifndef DINPUT_H
#define DINPUT_H

#include "win32.h"

#define KEYBOARD_DEVICE_DATA_CAPACITY (32)

typedef struct MouseData {
    int x;
    int y;
    unsigned char buttons[2];
} MouseData;

typedef struct KeyboardData {
    char key;
    char down;
} KeyboardData;

extern LPDIRECTINPUTA gDirectInput;
extern LPDIRECTINPUTDEVICEA gMouseDevice;
extern LPDIRECTINPUTDEVICEA gKeyboardDevice;
extern int gKeyboardDeviceDataIndex;
extern int gKeyboardDeviceDataLength;
extern DIDEVICEOBJECTDATA gKeyboardDeviceData[KEYBOARD_DEVICE_DATA_CAPACITY];

bool directInputInit();
void directInputFree();
bool mouseDeviceAcquire();
bool mouseDeviceUnacquire();
bool mouseDeviceGetData(MouseData* mouseData);
bool keyboardDeviceAcquire();
bool keyboardDeviceUnacquire();
bool keyboardDeviceReset();
bool keyboardDeviceGetData(KeyboardData* keyboardData);
bool mouseDeviceInit();
void mouseDeviceFree();
bool keyboardDeviceInit();
void keyboardDeviceFree();

#endif /* DINPUT_H */
