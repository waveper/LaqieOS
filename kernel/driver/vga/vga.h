#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

void VGAInit(void);
void VGAReset(void);
void VGAPut(char c);
void VGASetC(char c, int row, int collumn);
void VGAPrint(const char *string);
void VGAPrintHex(int value);
void VGAPrintPointerAddress(uintptr_t address);
void VGAPrintNum(int num);

#endif
