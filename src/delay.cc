#include "delay.h"

#include <SDL.h>

void delay_ms(int ms)
{
    if (ms <= 0) {
        return;
    }
    SDL_Delay(ms);
}
