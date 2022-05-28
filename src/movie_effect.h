#ifndef MOVIE_EFFECT_H
#define MOVIE_EFFECT_H

typedef enum MovieEffectType {
    MOVIE_EFFECT_TYPE_NONE = 0,
    MOVIE_EFFECT_TYPE_FADE_IN = 1,
    MOVIE_EFFECT_TYPE_FADE_OUT = 2,
} MovieEffectFadeType;

typedef struct MovieEffect {
    int startFrame;
    int endFrame;
    int steps;
    unsigned char fadeType;
    // range 0-63
    unsigned char r;
    // range 0-63
    unsigned char g;
    // range 0-63
    unsigned char b;
    struct MovieEffect* next;
} MovieEffect;

extern bool gMovieEffectsInitialized;
extern MovieEffect* gMovieEffectHead;

extern unsigned char _source_palette[768];
extern bool _inside_fade;

int movieEffectsInit();
void movieEffectsReset();
void movieEffectsExit();
int movieEffectsLoad(const char* fileName);
void _moviefx_stop();
void _moviefx_callback_func(int frame);
void _moviefx_palette_func(unsigned char* palette, int start, int end);
void movieEffectsClear();

#endif /* MOVIE_EFFECT_H */
