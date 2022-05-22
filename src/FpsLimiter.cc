#include "FpsLimiter.h"

FpsLimiter::FpsLimiter(size_t fps): 
    _Fps(fps), 
    _Ticks(0)
{
}

void FpsLimiter::Begin()
{
    _Ticks = SDL_GetTicks();
}

void FpsLimiter::End()
{
    if (1000 / _Fps > SDL_GetTicks() - _Ticks) 
        SDL_Delay(1000 / _Fps - (SDL_GetTicks() - _Ticks));
}
