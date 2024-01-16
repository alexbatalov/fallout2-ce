#ifndef GRAPH_LIB_H
#define GRAPH_LIB_H

namespace fallout {

unsigned char HighRGB(unsigned char color);
int load_lbm_to_buf(const char* path, unsigned char* buffer, int a3, int a4, int a5, int a6, int a7);
int graphCompress(unsigned char* a1, unsigned char* a2, int a3);
int graphDecompress(unsigned char* a1, unsigned char* a2, int a3);
void grayscalePaletteUpdate(int a1, int a2);
void grayscalePaletteApply(unsigned char* surface, int width, int height, int pitch);

} // namespace fallout

#endif /* GRAPH_LIB_H */
