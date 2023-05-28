#include "fps_limiter.h"

#include <SDL.h>

namespace fallout {

FpsLimiter::FpsLimiter(unsigned int fps)
    : _fps(fps)
    , _ticks(0)
{
}

void FpsLimiter::mark()
{
    _ticks = SDL_GetTicks();
}

void FpsLimiter::throttle() const
{
    int delay_ms = 1000 / _fps - (SDL_GetTicks() - _ticks);
    if (delay_ms > 0) {
#ifndef EMSCRIPTEN
        SDL_Delay(delay_ms);
#else
        // Browsers need some more time so let's delay a little bit smaller time
        SDL_Delay(delay_ms > 4 ? delay_ms - 4 : 0);
#endif
    } else {
#ifdef EMSCRIPTEN
// In theory we might have a long loop but let's hope this never happens.
// Calling renderPresent calls SDL_RenderPresent which
//   actually calls emscripten_sleep so we give a chance for browser to process event loop.
// So, if we have some very busy loop which calls renderPresent then we are safe.
//
// SDL_Delay(0);
#endif
    }
}

} // namespace fallout
