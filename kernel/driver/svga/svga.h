#ifndef SVGA_H
#define SVGA_H

#include <stdint.h>

void SVGAInit(uint32_t VBE_ADRESS);
void SVGAReset(void);
void SVGASetPixel(uint16_t x, uint16_t y, uint16_t color);

#endif
