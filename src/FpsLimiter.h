#ifndef Fallout2_RE_FpsLimiter_h_
#define Fallout2_RE_FpsLimiter_h_

#include <SDL.h>

class FpsLimiter 
{
public:
    FpsLimiter(size_t fps = 60);
    void Begin();
    void End();

private:
    size_t _Fps;
    size_t _Ticks;
};

#endif
