#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stddef.h>

#define TOTAL_BITMAP (1024 * 1024)
#define PAGE_SIZE (4096)

int CalculatePageUsage(void);
int GetFreePage(void);
int GetFreePages(size_t Count);
void FreePage(void *Page);
void FreePages(void *Base, size_t Count);
void *AllocatePage(void);
void *AllocatePages(size_t Count);

#endif
