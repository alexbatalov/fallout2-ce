#ifndef DRAW_H
#define DRAW_H

namespace fallout {

void bufferDrawLine(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void bufferDrawRect(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7);
void bufferDrawRectShadowed(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8);
void blitBufferToBufferStretch(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBufferStretchTrans(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void blitBufferToBuffer(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void blitBufferToBufferTrans(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void bufferFill(unsigned char* buf, int width, int height, int pitch, int a5);
void _buf_texture(unsigned char* buf, int width, int height, int pitch, void* a5, int a6, int a7);
void _lighten_buf(unsigned char* buf, int width, int height, int pitch);
void _swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void bufferOutline(unsigned char* buf, int width, int height, int pitch, int a5);
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* DRAW_H */
