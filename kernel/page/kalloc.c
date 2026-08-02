#include "kalloc.h"
#include "../panic.h"

void *kalloc(size_t Size) {
  if (Size == 0) return NULL;
  size_t TotalSize = Size + sizeof(BlockHeader);
  size_t PagesNeeded = (TotalSize + PAGE_SIZE - 1) / PAGE_SIZE;
  void *Base = AllocatePages(PagesNeeded);
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
  size_t TotalSize = Header->Size;
  size_t PagesUsed = (TotalSize + PAGE_SIZE - 1) / PAGE_SIZE;
  void *StartPage = (void *)Header;
  FreePages(StartPage, PagesUsed);
}
