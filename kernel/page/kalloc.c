#include "kalloc.h"
#include "../panic.h"

void *kalloc(size_t Size) {
  if (Size == 0) return NULL;
  size_t TotalSize = Size + sizeof(BlockHeader);
  void *Base = KAlloc((uint32_t)TotalSize);
  if (!Base) {
    Panic("Cannot allocate memory, Ran out of Memory");
  }

  BlockHeader *Header = (BlockHeader *)Base;
  Header->Size = TotalSize;
  Header->Next = NULL;
  return (char *)Base + sizeof(BlockHeader);
}

void kfree(void *Base) {
  if (!Base) {
    Panic("Cannot free non-existense allocated memory");
  }
  BlockHeader *Header = (BlockHeader *)((char *)Base - sizeof(BlockHeader));
  KFree((void *)Header);
}
