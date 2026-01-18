#ifndef FALLOUT_TOUCH_H_
#define FALLOUT_TOUCH_H_

#include <SDL.h>

namespace fallout {

enum GestureType {
    kUnrecognized,
    kTap,
    kLongPress,
    kPan,
};

enum GestureState {
    kPossible,
    kBegan,
    kChanged,
    kEnded,
};

struct Gesture {
    GestureType type;
    GestureState state;
    int numberOfTouches;
    int x;
    int y;
};

void touch_handle_start(SDL_TouchFingerEvent* event);
void touch_handle_move(SDL_TouchFingerEvent* event);
void touch_handle_end(SDL_TouchFingerEvent* event);
void touch_process_gesture();
bool touch_get_gesture(Gesture* gesture);
void touch_set_touchscreen_mode(const bool value);
void touch_set_pan_mode(const bool value);
bool touch_get_pan_mode();

} // namespace fallout

#endif /* FALLOUT_TOUCH_H_ */
