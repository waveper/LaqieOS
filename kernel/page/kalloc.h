
#ifndef ALLOC_H
#define ALLOC_H

#include "bitmap.h"

typedef struct BlockHeader
{
        size_t Size;
        struct BlockHeader *Next;
} BlockHeader;

void *kalloc(size_t Size);
void kfree(void *Base);

#endif
