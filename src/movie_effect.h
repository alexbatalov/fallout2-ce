#ifndef MOVIE_EFFECT_H
#define MOVIE_EFFECT_H

int movieEffectsInit();
void movieEffectsReset();
void movieEffectsExit();
int movieEffectsLoad(const char* fileName);
void _moviefx_stop();

#endif /* MOVIE_EFFECT_H */
