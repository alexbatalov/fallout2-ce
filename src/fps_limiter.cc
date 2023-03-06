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
        SDL_Delay(0);
        #endif
    }
}




FpsThrottler::FpsThrottler():
    last_time_tick(SDL_GetTicks())
{}

void FpsThrottler::operator()(int target_fps){
    unsigned int now_ticks = SDL_GetTicks();
    int to_wait_ms = (1000/target_fps) - (now_ticks - last_time_tick);
    SDL_Delay(std::max(0, to_wait_ms));
    last_time_tick = now_ticks;
}

} // namespace fallout


