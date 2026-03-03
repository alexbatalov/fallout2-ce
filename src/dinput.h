#ifndef DINPUT_H
#define DINPUT_H

#include <SDL.h>

namespace fallout {

typedef struct MouseData {
    int x;
    int y;
    unsigned char buttons[2];
    int wheelX;
    int wheelY;
} MouseData;

typedef struct KeyboardData {
    int key;
    char down;
} KeyboardData;

bool directInputInit();
void directInputFree();
bool mouseDeviceAcquire();
bool mouseDeviceUnacquire();
bool mouseDeviceGetData(MouseData* mouseData);
bool keyboardDeviceAcquire();
bool keyboardDeviceUnacquire();
bool keyboardDeviceReset();
bool mouseDeviceInit();
void mouseDeviceFree();
bool keyboardDeviceInit();
void keyboardDeviceFree();

void handleMouseEvent(SDL_Event* event);
void handleTouchEvent(SDL_Event* event);

// Cached controller handle (opened/closed via SDL device events)
extern SDL_GameController* gController;

// Mouse-cursor deltas and button state driven by the left stick and A/B buttons
extern int gLeftStickDeltaX;
extern int gLeftStickDeltaY;
extern int gGamepadLeftClick;
extern int gGamepadRightClick;

// SDL controller event handlers — called from _GNW95_process_message()
void HandleControllerDeviceAdded(const SDL_Event& event);
void HandleControllerDeviceRemoved(const SDL_Event& event);
void HandleControllerAxisMotion(const SDL_Event& event);

// Called each frame from _GNW95_process_message() to accumulate cursor deltas
void ProcessLeftStick();

// Fills MouseData from accumulated gamepad state; called by mouseDeviceGetData()
bool GetGamepadMouseState(MouseData* mouseState);

} // namespace fallout

#endif /* DINPUT_H */
