#ifndef STDMEM_H
#define STDMEM_H

#include <stdint.h>
#include <stddef.h>

int RamCountSize(void);
void *memcpy(void *dest, const void *src, uint32_t size);
void memset(void* des, int value, uint32_t size);

#endif
