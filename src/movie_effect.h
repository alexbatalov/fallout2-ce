#ifndef MOVIE_EFFECT_H
#define MOVIE_EFFECT_H

namespace fallout {

int movieEffectsInit();
void movieEffectsReset();
void movieEffectsExit();
int movieEffectsLoad(const char* fileName);
void _moviefx_stop();

} // namespace fallout

#endif /* MOVIE_EFFECT_H */
