#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE (4096)

void *KAlloc(uint32_t bytes);
int KFree(void *address);
uint32_t CountAvailablePage(void);
uint32_t HowManyPagesCouldFitInRAM(void);

#endif