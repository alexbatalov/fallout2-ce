#ifndef WORD_WRAP_H
#define WORD_WRAP_H

#define WORD_WRAP_MAX_COUNT (64)

int wordWrap(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr);

#endif /* WORD_WRAP_H */
