#include "bitmap.h"
#include "../../stdlib/stdmem.h"
#include "../layout.h"

static uint8_t   PageBitmap[TOTAL_BITMAP / 8] = {0};
extern uint8_t KernelEnd;

extern int MAX_ADDR;
static size_t NextFreePageHint = 0;

static uintptr_t MemoryStart(void) {
  uintptr_t base = (uintptr_t)&KernelEnd;
  uintptr_t start = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
  if (start < USER_SPACE_END) {
    start = USER_SPACE_END;
  }
  return start;
}

static size_t AvailablePageCount(void) {
  uintptr_t start = MemoryStart();
  uintptr_t limit = (uintptr_t)(0x100000 + MAX_ADDR);

  if (MAX_ADDR <= 0 || start >= limit) {
    return 0;
  }

  size_t page_count = (limit - start) / PAGE_SIZE;
  if (page_count > TOTAL_BITMAP) {
    page_count = TOTAL_BITMAP;
  }
  return page_count;
}

// Bitmap based Page allocator

int GetFreePage(void) {
  size_t page_count = AvailablePageCount();
  if (page_count == 0) {
    return -1;
  }

  size_t start = NextFreePageHint;
  if (start >= page_count) {
    start = 0;
  }

  for (size_t pass = 0; pass < 2; ++pass) {
    size_t begin = pass == 0 ? start : 0;
    size_t end = pass == 0 ? page_count : start;
    for (size_t i = begin; i < end; ++i) {
      size_t ByteIdx = i / 8;
      size_t BitIdx = i % 8;
      uint8_t Mask = (uint8_t)(1u << BitIdx);
      if (!(PageBitmap[ByteIdx] & Mask)) {
        PageBitmap[ByteIdx] |= Mask;
        NextFreePageHint = i + 1;
        return (int)i;
      }
    }
  }
  return -1;
}

int GetFreePages(size_t Count) {
  if (Count == 0) {
    return -1;
  }

  size_t page_count = AvailablePageCount();
  if (Count > page_count) {
    return -1;
  }

  size_t start = NextFreePageHint;
  if (start >= page_count) {
    start = 0;
  }

  for (size_t pass = 0; pass < 2; ++pass) {
    size_t begin = pass == 0 ? start : 0;
    size_t end = pass == 0 ? (page_count - Count + 1) : (start > (page_count - Count + 1) ? (page_count - Count + 1) : start);
    for (size_t start_idx = begin; start_idx < end; ++start_idx) {
      size_t matched = 0;
      while (matched < Count) {
        size_t page_idx = start_idx + matched;
        size_t byte_idx = page_idx / 8;
        size_t bit_idx = page_idx % 8;
        if (PageBitmap[byte_idx] & (1u << bit_idx)) {
          break;
        }
        ++matched;
      }

      if (matched == Count) {
        for (size_t i = 0; i < Count; ++i) {
          size_t page_idx = start_idx + i;
          size_t byte_idx = page_idx / 8;
          size_t bit_idx = page_idx % 8;
          PageBitmap[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
        NextFreePageHint = start_idx + Count;
        return (int)start_idx;
      }

      start_idx += matched;
    }
  }

  return -1;
}

void FreePage(void *Page) {
  uintptr_t Addr = (uintptr_t)Page;
  uintptr_t start = MemoryStart();
  size_t page_count = AvailablePageCount();
  uintptr_t limit = start + (page_count * PAGE_SIZE);

  if (Addr < start || Addr >= limit) {
    return;
  }

  size_t PageIdx = (Addr - start) / PAGE_SIZE;
  size_t ByteIdx = PageIdx / 8;
  size_t BitIdx = PageIdx % 8;
  PageBitmap[ByteIdx] &= ~(1 << BitIdx);
}

void FreePages(void *Base, size_t Count) {
  if (!Base || Count == 0) {
    return;
  }

  uintptr_t addr = (uintptr_t)Base;
  if ((addr % PAGE_SIZE) != 0) {
    return;
  }

  uintptr_t start = MemoryStart();
  size_t page_count = AvailablePageCount();
  uintptr_t limit = start + (page_count * PAGE_SIZE);
  if (addr < start || addr >= limit) {
    return;
  }

  size_t start_idx = (addr - start) / PAGE_SIZE;
  for (size_t i = 0; i < Count; ++i) {
    size_t page_idx = start_idx + i;
    if (page_idx >= page_count) {
      break;
    }
    size_t byte_idx = page_idx / 8;
    size_t bit_idx = page_idx % 8;
    PageBitmap[byte_idx] &= (uint8_t)~(1u << bit_idx);
  }

  if (start_idx < NextFreePageHint) {
    NextFreePageHint = start_idx;
  }
}

void *AllocatePage(void) {
  int PageIdx = GetFreePage();
  if (PageIdx == -1) return NULL;
  void *Page = (void*)(MemoryStart() + (PageIdx * PAGE_SIZE));
  memset(Page, 0, PAGE_SIZE);
  return Page;
}

void *AllocatePages(size_t Count) {
  int PageIdx = GetFreePages(Count);
  if (PageIdx == -1) {
    return NULL;
  }

  void *Base = (void *)(MemoryStart() + ((size_t)PageIdx * PAGE_SIZE));
  memset(Base, 0, (uint32_t)(Count * PAGE_SIZE));
  return Base;
}

int CalculatePageUsage(void) {
  int ByteCount = 0;
  size_t page_count = AvailablePageCount();
  size_t byte_count = (page_count + 7) / 8;

  for (size_t i = 0; i < byte_count; ++i) {
    uint8_t value = PageBitmap[i];
    if (i == byte_count - 1 && (page_count % 8) != 0) {
      value &= (uint8_t)((1u << (page_count % 8)) - 1u);
    }

    while (value) {
      ByteCount += PAGE_SIZE;
      value &= (uint8_t)(value - 1);
    }
  }
  return ByteCount;
}
