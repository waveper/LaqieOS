#include "paging.h"
#include "../../stdlib/stdmem.h"
#include "../layout.h"
#include "../panic.h"
#include "bitmap.h"
#include "kalloc.h"
#include <stdbool.h>

#define PAGE_PRESENT 0x001
#define PAGE_RW 0x002
#define PAGE_USER 0x004
#define PAGE_ADDR 0xFFFFF000
#define PAGE_ENTRIES 1024

extern int MAX_ADDR;

struct AddressSpace {
  uint32_t *directory;
  uint8_t owned_tables[PAGE_ENTRIES];
};

static AddressSpace KernelSpace;
static int PagingEnabled = 0;

static inline uint32_t ReadCR0(void) {
  uint32_t value;
  asm volatile("mov %%cr0, %0" : "=r"(value));
  return value;
}

static inline void WriteCR0(uint32_t value) {
  asm volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

static inline void WriteCR3(uint32_t value) {
  asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

static uint32_t PageAlignUp(uint32_t value) {
  return (value + PAGE_SIZE - 1) & ~(uint32_t)(PAGE_SIZE - 1);
}

static uint32_t PageDirectoryIndex(uint32_t virt) { return virt >> 22; }

static uint32_t PageTableIndex(uint32_t virt) { return (virt >> 12) & 0x3FF; }

static uint32_t *PageTableFromEntry(uint32_t entry) {
  return (uint32_t *)(entry & PAGE_ADDR);
}

static int EnsurePrivatePageTable(AddressSpace *space, uint32_t dir_index) {
  if (!space || dir_index >= PAGE_ENTRIES)
    return -1;
  if (space->owned_tables[dir_index])
    return 0;

  uint32_t *table = KAlloc(PAGE_SIZE);
  if (!table)
    return -1;

  if (space->directory[dir_index] & PAGE_PRESENT) {
    memcpy(table, PageTableFromEntry(space->directory[dir_index]), PAGE_SIZE);
  } else {
    memset(table, 0, PAGE_SIZE);
  }

  space->directory[dir_index] =
      ((uint32_t)table & PAGE_ADDR) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
  space->owned_tables[dir_index] = 1;
  return 0;
}

static int MapPage(AddressSpace *space, uint32_t virt, uint32_t phys,
                   uint32_t flags) {
  uint32_t dir_index = PageDirectoryIndex(virt);
  uint32_t table_index = PageTableIndex(virt);

  if (EnsurePrivatePageTable(space, dir_index) != 0)
    return -1;
  uint32_t *table = PageTableFromEntry(space->directory[dir_index]);
  table[table_index] = (phys & PAGE_ADDR) | flags | PAGE_PRESENT;
  return 0;
}

static void FreeUserRange(AddressSpace *space, uint32_t virt, uint32_t size) {
  uint32_t start = virt & ~(uint32_t)(PAGE_SIZE - 1);
  uint32_t end = PageAlignUp(virt + size);

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    uint32_t dir_index = PageDirectoryIndex(addr);
    uint32_t table_index = PageTableIndex(addr);
    if (!space->owned_tables[dir_index])
      continue;

    uint32_t *table = PageTableFromEntry(space->directory[dir_index]);
    uint32_t entry = table[table_index];
    if ((entry & (PAGE_PRESENT | PAGE_USER)) == (PAGE_PRESENT | PAGE_USER)) {
      KFree((void *)(entry & PAGE_ADDR));
      table[table_index] = 0;
    }
  }
}

static void IdentityMapRange(AddressSpace *space, uint32_t start,
                             uint32_t size) {
  uint32_t end = PageAlignUp(start + size);

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    uint32_t dir_index = PageDirectoryIndex(addr);
    uint32_t table_index = PageTableIndex(addr);
    uint32_t *table;

    if (!(space->directory[dir_index] & PAGE_PRESENT)) {
      table = KAlloc(PAGE_SIZE);
      if (!table)
        Panic("Cannot allocate kernel page table");
      memset(table, 0, PAGE_SIZE);
      space->directory[dir_index] =
          ((uint32_t)table & PAGE_ADDR) | PAGE_PRESENT | PAGE_RW;
      space->owned_tables[dir_index] = 1;
    } else {
      table = PageTableFromEntry(space->directory[dir_index]);
    }

    table[table_index] = (addr & PAGE_ADDR) | PAGE_PRESENT | PAGE_RW;
  }
}

void PagingInit(void) {
  uint32_t map_size = (uint32_t)(0x100000 + MAX_ADDR);

  memset(&KernelSpace, 0, sizeof(KernelSpace));
  KernelSpace.directory = KAlloc(PAGE_SIZE);
  if (!KernelSpace.directory)
    Panic("Cannot allocate kernel page directory");
  memset(KernelSpace.directory, 0, PAGE_SIZE);

  IdentityMapRange(&KernelSpace, 0, map_size);
  WriteCR3((uint32_t)KernelSpace.directory);
  WriteCR0(ReadCR0() | 0x80000000);
  PagingEnabled = 1;
}

AddressSpace *PagingKernelAddressSpace(void) { return &KernelSpace; }

AddressSpace *PagingCreateUserAddressSpace(void) {
  AddressSpace *space = kalloc(sizeof(AddressSpace));
  if (!space)
    return NULL;
  memset(space, 0, sizeof(AddressSpace));

  space->directory = KAlloc(PAGE_SIZE);
  if (!space->directory) {
    kfree(space);
    return NULL;
  }

  memcpy(space->directory, KernelSpace.directory, PAGE_SIZE);
  memset(space->owned_tables, 0, sizeof(space->owned_tables));
  return space;
}

void PagingDestroyAddressSpace(AddressSpace *space) {
  if (!space || space == &KernelSpace)
    return;

  FreeUserRange(space, USER_EXEC_LOAD_ADDR, USER_EXEC_MAX_SIZE);
  FreeUserRange(space, USER_STACK_BASE, USER_STACK_SIZE);

  for (uint32_t i = 0; i < PAGE_ENTRIES; ++i) {
    if (space->owned_tables[i] && (space->directory[i] & PAGE_PRESENT)) {
      KFree(PageTableFromEntry(space->directory[i]));
    }
  }

  KFree(space->directory);
  kfree(space);
}

int PagingMapUserRange(AddressSpace *space, uint32_t virt, uint32_t size) {
  uint32_t start = virt & ~(uint32_t)(PAGE_SIZE - 1);
  uint32_t end = PageAlignUp(virt + size);

  if (!space || space == &KernelSpace || end < start)
    return -1;

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    void *page = KAlloc(PAGE_SIZE);
    if (!page) {
      FreeUserRange(space, start, addr - start);
      return -1;
    }
    memset(page, 0, PAGE_SIZE);

    if (MapPage(space, addr, (uint32_t)page, PAGE_RW | PAGE_USER) != 0) {
      KFree(page);
      FreeUserRange(space, start, addr - start);
      return -1;
    }
  }

  return 0;
}

// Maps physical address to virtual
// if bool user is 1, then map it as user acessible
// if not, then it was kernel space
int PagingMapUserPhysicalRange(AddressSpace *space, uint32_t virt,
                               uint32_t phys, uint32_t size, bool user) {
  if (!space || space == &KernelSpace || size == 0)
    return -1;
  if ((virt & (PAGE_SIZE - 1)) != (phys & (PAGE_SIZE - 1)))
    return -1;

  uint32_t start = virt & ~(uint32_t)(PAGE_SIZE - 1);
  uint32_t end = PageAlignUp(virt + size);
  if (end < start)
    return -1;

  uint32_t page_phys = phys & ~(uint32_t)(PAGE_SIZE - 1);
  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    if (user) {
      if (MapPage(space, addr, page_phys, PAGE_RW | PAGE_USER) != 0) {
        return -1;
      }
    } else {
      if (MapPage(space, addr, page_phys, PAGE_RW) != 0) {
        return -1;
      }
    }
    page_phys += PAGE_SIZE;
  }

  return 0;
}

/*
 * Clears the page-table entries for a user range without freeing the physical
 * pages behind them. Used to revoke a task's access to allocations that live
 * in kernel-owned memory.
 */
int PagingUnmapUserRange(AddressSpace *space, uint32_t virt, uint32_t size) {
  if (!space || space == &KernelSpace || size == 0)
    return -1;

  uint32_t start = virt & ~(uint32_t)(PAGE_SIZE - 1);
  uint32_t end = PageAlignUp(virt + size);
  if (end < start)
    return -1;

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    uint32_t dir_index = PageDirectoryIndex(addr);
    if (!space->owned_tables[dir_index])
      continue;
    uint32_t *table = PageTableFromEntry(space->directory[dir_index]);
    table[PageTableIndex(addr)] = 0;
  }
  return 0;
}

int PagingMapKernelRange(uint32_t start, uint32_t size) {
  if (size == 0)
    return -1;

  uint32_t end = PageAlignUp(start + size);
  if (end < start)
    return -1;

  for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
    uint32_t dir_index = PageDirectoryIndex(addr);
    uint32_t table_index = PageTableIndex(addr);
    uint32_t *table;

    if (!(KernelSpace.directory[dir_index] & PAGE_PRESENT)) {
      table = KAlloc(PAGE_SIZE);
      if (!table)
        return -1;
      memset(table, 0, PAGE_SIZE);
      KernelSpace.directory[dir_index] =
          ((uint32_t)table & PAGE_ADDR) | PAGE_PRESENT | PAGE_RW;
      KernelSpace.owned_tables[dir_index] = 1;
    } else {
      table = PageTableFromEntry(KernelSpace.directory[dir_index]);
    }

    table[table_index] = (addr & PAGE_ADDR) | PAGE_PRESENT | PAGE_RW;
  }

  return 0;
}

void *PagingVirtualToKernel(AddressSpace *space, uint32_t virt) {
  uint32_t dir_index = PageDirectoryIndex(virt);
  uint32_t table_index = PageTableIndex(virt);

  if (!space || !(space->directory[dir_index] & PAGE_PRESENT))
    return NULL;

  uint32_t *table = PageTableFromEntry(space->directory[dir_index]);
  uint32_t entry = table[table_index];
  if (!(entry & PAGE_PRESENT))
    return NULL;

  return (void *)((entry & PAGE_ADDR) | (virt & (PAGE_SIZE - 1)));
}

void PagingSwitchAddressSpace(AddressSpace *space) {
  if (!PagingEnabled)
    return;
  if (!space)
    space = &KernelSpace;
  WriteCR3((uint32_t)space->directory);
}
