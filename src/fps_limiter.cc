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
    if (1000 / _fps > SDL_GetTicks() - _ticks) {
        SDL_Delay(1000 / _fps - (SDL_GetTicks() - _ticks));
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


