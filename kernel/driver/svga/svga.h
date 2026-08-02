#ifndef SVGA_H
#define SVGA_H

void SVGAInit(uint32_t VBE_ADRESS);
void SVGAReset(void);
void VGASetC(unsigned short color, int row, int column);

#endif
