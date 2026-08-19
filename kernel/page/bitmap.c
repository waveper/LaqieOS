#include <stdint.h>
#include <stdbool.h>
#include "bitmap.h"
#include "../layout.h"

extern int MAX_ADDR;
extern uint8_t KernelEnd;
#define TOTAL_PAGE_ENTRIES 262144

uint32_t HowManyPagesCouldFitInRAM(void);

// Fixed pointer
static uint32_t* EntriesBitmap = (uint32_t*)0x00200000;

// Physical memory starts at 1MB
#define MEMORY_BASE 0x100000

// Bit shifts
#define SHIFT_STATUS        0
#define SHIFT_CHILD_COUNT   3
#define SHIFT_RESERVED      19

// Bit masks
#define MASK_STATUS         0x7       // 3 bits
#define MASK_CHILD_COUNT    0xFFFF    // 16 bits
#define MASK_RESERVED       0x1FFF    // 13 bits

// Status Flag Bits
#define FLAG_AVAILABLE      (1 << 0)  // Bit 0: 0=unusable, 1=available
#define FLAG_ALLOCATED      (1 << 1)  // Bit 1: 0=free, 1=allocated
#define FLAG_CHILD          (1 << 2)  // Bit 2: 0=parent, 1=child

// Retrieve status flags (3 bits)
static uint8_t metadata_get_status(uint32_t entry) {
  return (entry >> SHIFT_STATUS) & MASK_STATUS;
}

// Retrieve child page count (16 bits)
static uint16_t metadata_get_child_count(uint32_t entry) {
  return (entry >> SHIFT_CHILD_COUNT) & MASK_CHILD_COUNT;
}

// Check if page is usable
static bool metadata_is_available(uint32_t entry) {
  return (entry & FLAG_AVAILABLE) != 0;
}

static bool metadata_is_allocated(uint32_t entry) {
  return (entry & FLAG_ALLOCATED) != 0;
}

static uint8_t metadata_create_status(bool available, bool allocated, bool child) {
  return available | (allocated << 1) | (child << 2);
}

static uint32_t create_metadata_entry(uint8_t status, uint16_t child_count) {
  uint32_t entry = 0;
  entry |= ((uint32_t)(status & MASK_STATUS) << SHIFT_STATUS);
  entry |= ((uint32_t)(child_count & MASK_CHILD_COUNT) << SHIFT_CHILD_COUNT);
  return entry;
}

// Set a specific page entry in memory
static void set_page_metadata(uint32_t page_index, uint8_t status, uint16_t child_count) {
  if (page_index < TOTAL_PAGE_ENTRIES) {
    EntriesBitmap[page_index] = create_metadata_entry(status, child_count);
  }
}

static uint32_t PageAlignUp(uint32_t value) {
  return (value + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);
}

static uint32_t PageIndexToAddress(uint32_t address) {
  return (uintptr_t)(4096 * address);
}

// Convert a page index to its physical address
static uintptr_t PageIndexToAddr(uint32_t page_index) {
  return (uintptr_t)MEMORY_BASE + PageIndexToAddress(page_index);
}

// Convert a physical address to its page index
static uint32_t AddrToPageIndex(uintptr_t address) {
  return (uint32_t)((address - MEMORY_BASE) / PAGE_SIZE);
}

// First page usable for allocation (page aligned end of kernel image)
static uintptr_t MemoryStart(void) {
  uintptr_t base = (uintptr_t)&KernelEnd;
  uintptr_t start = (base + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
  if (start < USER_SPACE_END) {
    start = USER_SPACE_END;
  }
  return start;
}

// Check if a page is both usable and not allocated
static bool PageIsFree(uint32_t page_index) {
  uint32_t entry = EntriesBitmap[page_index];
  uint8_t status = metadata_get_status(entry);
  return metadata_is_available(entry) && !(status & FLAG_ALLOCATED);
}

static uint32_t NextFreePageHint = 0;
static bool BitmapInitialized = false;

// Mark every page as reserved, then open the usable region above the kernel,
// keeping the metadata region itself out of reach.
static void InitBitmap(void) {
  uint32_t total = HowManyPagesCouldFitInRAM();
  uint8_t reserved = metadata_create_status(false, false, false);

  for (uint32_t i = 0; i < total; ++i) {
    set_page_metadata(i, reserved, 0);
  }

  uint32_t start_index = AddrToPageIndex(MemoryStart());
  uint8_t free_status = metadata_create_status(true, false, false);
  for (uint32_t i = start_index; i < total; ++i) {
    set_page_metadata(i, free_status, 0);
  }

  uint32_t metadata_start = AddrToPageIndex(0x00200000);
  uint32_t metadata_end = AddrToPageIndex(0x00300000 - 1);
  for (uint32_t i = metadata_start; i <= metadata_end && i < total; ++i) {
    set_page_metadata(i, reserved, 0);
  }

  NextFreePageHint = start_index;
}

static void EnsureInitialized(void) {
  if (!BitmapInitialized) {
    InitBitmap();
    BitmapInitialized = true;
  }
}

// Find a run of `count` consecutive free pages
static int FindFreeRun(uint32_t count) {
  uint32_t total = HowManyPagesCouldFitInRAM();
  if (count == 0 || count > total) {
    return -1;
  }

  uint32_t start = NextFreePageHint;
  if (start >= total) {
    start = 0;
  }

  for (uint32_t pass = 0; pass < 2; ++pass) {
    uint32_t begin = (pass == 0) ? start : 0;
    uint32_t end = (pass == 0) ? (total - count + 1)
                               : (start > (total - count + 1) ? (total - count + 1) : start);
    for (uint32_t i = begin; i < end; ++i) {
      uint32_t matched = 0;
      while (matched < count) {
        if (!PageIsFree(i + matched)) {
          break;
        }
        ++matched;
      }

      if (matched == count) {
        NextFreePageHint = i + count;
        return (int)i;
      }

      i += matched;
    }
  }

  return -1;
}

// Mark the first page as parent and the rest as children
static void MarkRunAllocated(uint32_t start, uint32_t count) {
  uint8_t parent_status = metadata_create_status(true, true, false);
  uint8_t child_status = metadata_create_status(true, true, true);

  set_page_metadata(start, parent_status, (uint16_t)(count - 1));
  for (uint32_t i = 1; i < count; ++i) {
    set_page_metadata(start + i, child_status, 0);
  }
}

static void MarkRunFree(uint32_t start, uint32_t count) {
  uint32_t total = HowManyPagesCouldFitInRAM();
  uint8_t free_status = metadata_create_status(true, false, false);

  for (uint32_t i = 0; i < count && (start + i) < total; ++i) {
    set_page_metadata(start + i, free_status, 0);
  }

  if (start < NextFreePageHint) {
    NextFreePageHint = start;
  }
}

void* KAlloc(uint32_t bytes) {
  EnsureInitialized();
  if (bytes == 0) {
    return NULL;
  }

  uint32_t count = PageAlignUp(bytes) / PAGE_SIZE;
  int start_index = FindFreeRun(count);
  if (start_index < 0) {
    return NULL;
  }

  MarkRunAllocated((uint32_t)start_index, count);
  return (void*)PageIndexToAddr((uint32_t)start_index);
}

int KFree(void *address) {
  EnsureInitialized();
  if (!address) {
    return -1;
  }

  uintptr_t addr = (uintptr_t)address;
  if (addr < MEMORY_BASE || (addr % PAGE_SIZE) != 0) {
    return -1;
  }

  uint32_t index = AddrToPageIndex(addr);
  if (index >= HowManyPagesCouldFitInRAM()) {
    return -1;
  }

  uint32_t entry = EntriesBitmap[index];
  if (!metadata_is_allocated(entry)) {
    return -1;
  }

  uint8_t status = metadata_get_status(entry);
  if (status & FLAG_CHILD) {
    return -1;
  }

  uint32_t count = (uint32_t)metadata_get_child_count(entry) + 1;
  MarkRunFree(index, count);
  return 0;
}

// Like, how much usable RAM left
uint32_t CountAvailablePage(void) {
  EnsureInitialized();
  uint32_t total = HowManyPagesCouldFitInRAM();
  uint32_t count = 0;

  for (uint32_t i = 0; i < total; ++i) {
    if (PageIsFree(i)) {
      ++count;
    }
  }

  return count;
}

uint32_t HowManyPagesCouldFitInRAM(void) {
  if (MAX_ADDR <= 0) return 0;
  uint32_t count = MAX_ADDR / PAGE_SIZE;
  if (count > TOTAL_PAGE_ENTRIES) count = TOTAL_PAGE_ENTRIES;
  return count;
}
