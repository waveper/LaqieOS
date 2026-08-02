#include "sched.h"
#include "../fs/api.h"
#include "../page/kalloc.h"
#include "../page/paging.h"
#include "../../stdlib/stdmem.h"
#include "../../stdlib/string.h"
#include <stdint.h>
#include "../layout.h"

#define LQE32_HEADER_SIZE 8
#define LQE32_VERSION_FLAT 0x01
#define LQE32_VERSION_RELOC 0x02
#define LQE32_RELOC_HEADER_SIZE 28
/*
 * LQE32 v2 keeps the original leading jump and signature, then stores:
 * load_size, preferred_base, entry_offset, reloc_table_offset, reloc_count.
 * Relocation entries are uint32_t offsets to absolute 32-bit words in image.
 */
#define LQE32_RELOC_IMAGE_SIZE_OFFSET 8
#define LQE32_RELOC_BASE_OFFSET 12
#define LQE32_RELOC_ENTRY_OFFSET 16
#define LQE32_RELOC_TABLE_OFFSET 20
#define LQE32_RELOC_COUNT_OFFSET 24

typedef struct ExecutableImageInfo
{
  uint32_t load_size;
  uint32_t preferred_base;
  uint32_t entry_offset;
  uint32_t reloc_table_offset;
  uint32_t reloc_count;
  int has_relocations;
} ExecutableImageInfo;

static uint32_t ReadU32(const char *buffer, uint32_t offset) {
  const uint8_t *bytes = (const uint8_t *)buffer + offset;
  return (uint32_t)bytes[0] |
      ((uint32_t)bytes[1] << 8) |
      ((uint32_t)bytes[2] << 16) |
      ((uint32_t)bytes[3] << 24);
}

static int RangeInFile(uint32_t offset, uint32_t size, uint32_t file_size) {
  if (offset > file_size) return 0;
  if (size > file_size - offset) return 0;
  return 1;
}

static int ValidateRelocations(const char *buffer, uint32_t size,
    const ExecutableImageInfo *info) {
  uint32_t table_size = info->reloc_count * sizeof(uint32_t);

  if (info->reloc_count != 0 &&
      table_size / sizeof(uint32_t) != info->reloc_count) {
    return -1;
  }
  if (!RangeInFile(info->reloc_table_offset, table_size, size)) return -1;

  for (uint32_t i = 0; i < info->reloc_count; ++i) {
    uint32_t reloc_offset =
        ReadU32(buffer, info->reloc_table_offset + (i * sizeof(uint32_t)));
    if (!RangeInFile(reloc_offset, sizeof(uint32_t), info->load_size)) {
      return -1;
    }
  }

  return 0;
}

static int ValidateExecutableImage(const char *buffer, int size,
    ExecutableImageInfo *info) {
  if (!buffer || size < LQE32_HEADER_SIZE) return -1;
  if (!info) return -1;
  if ((uint8_t)buffer[0] != 0xEB && (uint8_t)buffer[0] != 0xE9) return -1;
  if (strncmp(buffer + 2, "LQE32", 5) != 0) return -1;

  memset(info, 0, sizeof(ExecutableImageInfo));
  info->load_size = (uint32_t)size;
  info->preferred_base = USER_EXEC_LOAD_ADDR;
  info->entry_offset = 0;

  uint8_t version = (uint8_t)buffer[7];
  if (version == LQE32_VERSION_FLAT) {
    if (size > USER_EXEC_MAX_SIZE) return -1;
    return 0;
  }

  if (version != LQE32_VERSION_RELOC) return -1;
  if (size < LQE32_RELOC_HEADER_SIZE) return -1;

  info->load_size = ReadU32(buffer, LQE32_RELOC_IMAGE_SIZE_OFFSET);
  info->preferred_base = ReadU32(buffer, LQE32_RELOC_BASE_OFFSET);
  info->entry_offset = ReadU32(buffer, LQE32_RELOC_ENTRY_OFFSET);
  info->reloc_table_offset = ReadU32(buffer, LQE32_RELOC_TABLE_OFFSET);
  info->reloc_count = ReadU32(buffer, LQE32_RELOC_COUNT_OFFSET);
  info->has_relocations = 1;

  if (info->load_size < LQE32_RELOC_HEADER_SIZE) return -1;
  if (info->load_size > USER_EXEC_MAX_SIZE) return -1;
  if (info->load_size > (uint32_t)size) return -1;
  if (info->entry_offset >= info->load_size) return -1;
  if (ValidateRelocations(buffer, (uint32_t)size, info) != 0) return -1;

  return 0;
}

static int CopyToAddressSpace(AddressSpace *space, uint32_t dest,
    const char *src, uint32_t size) {
  uint32_t copied = 0;

  while (copied < size) {
    uint32_t virt = dest + copied;
    uint32_t page_offset = virt & (PAGE_SIZE - 1);
    uint32_t chunk = PAGE_SIZE - page_offset;
    void *kernel_dest;

    if (chunk > size - copied) chunk = size - copied;
    kernel_dest = PagingVirtualToKernel(space, virt);
    if (!kernel_dest) return -1;

    memcpy(kernel_dest, src + copied, chunk);
    copied += chunk;
  }

  return 0;
}

static int ReadAddressSpaceU32(AddressSpace *space, uint32_t virt,
    uint32_t *value) {
  uint8_t bytes[sizeof(uint32_t)];

  if (!value) return -1;
  for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
    uint8_t *src = PagingVirtualToKernel(space, virt + i);
    if (!src) return -1;
    bytes[i] = *src;
  }

  *value = (uint32_t)bytes[0] |
      ((uint32_t)bytes[1] << 8) |
      ((uint32_t)bytes[2] << 16) |
      ((uint32_t)bytes[3] << 24);
  return 0;
}

static int WriteAddressSpaceU32(AddressSpace *space, uint32_t virt,
    uint32_t value) {
  for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
    uint8_t *dest = PagingVirtualToKernel(space, virt + i);
    if (!dest) return -1;
    *dest = (uint8_t)(value >> (i * 8));
  }

  return 0;
}

static int RelocateExecutableImage(AddressSpace *space, const char *buffer,
    const ExecutableImageInfo *info) {
  if (!space || !buffer || !info || !info->has_relocations) return 0;

  uint32_t runtime_base = USER_EXEC_LOAD_ADDR;
  uint32_t delta = runtime_base - info->preferred_base;

  for (uint32_t i = 0; i < info->reloc_count; ++i) {
    uint32_t reloc_offset =
        ReadU32(buffer, info->reloc_table_offset + (i * sizeof(uint32_t)));
    uint32_t target_value;
    if (ReadAddressSpaceU32(space, runtime_base + reloc_offset,
        &target_value) != 0) {
      return -1;
    }
    if (WriteAddressSpaceU32(space, runtime_base + reloc_offset,
        target_value + delta) != 0) {
      return -1;
    }
  }

  return 0;
}

int execute(const char *path) {
  int pid = -1;
  AddressSpace *address_space = NULL;
  ExecutableImageInfo image_info;

  if (!path) return -1;

  int size = FileSize(path);
  char *fbuffer = 0;
  if (size <= 0) return -1;

  if (ReadFile(path, &fbuffer) != size) {
    if (fbuffer) kfree(fbuffer);
    return -1;
  }

  if (ValidateExecutableImage(fbuffer, size, &image_info) != 0) {
    kfree(fbuffer);
    return -1;
  }

  address_space = PagingCreateUserAddressSpace();
  if (!address_space) {
    kfree(fbuffer);
    return -1;
  }

  if (PagingMapUserRange(address_space, USER_EXEC_LOAD_ADDR,
      USER_EXEC_MAX_SIZE) != 0 ||
      PagingMapUserRange(address_space, USER_STACK_BASE,
      USER_STACK_SIZE) != 0) {
    PagingDestroyAddressSpace(address_space);
    kfree(fbuffer);
    return -1;
  }

  if (CopyToAddressSpace(address_space, USER_EXEC_LOAD_ADDR, fbuffer,
      image_info.load_size) != 0) {
    PagingDestroyAddressSpace(address_space);
    kfree(fbuffer);
    return -1;
  }

  if (RelocateExecutableImage(address_space, fbuffer, &image_info) != 0) {
    PagingDestroyAddressSpace(address_space);
    kfree(fbuffer);
    return -1;
  }

  pid = AppendTaskWithAddressSpace((char *)path,
      (void (*)(void))(USER_EXEC_LOAD_ADDR + image_info.entry_offset),
      address_space);
  kfree(fbuffer);
  if (pid < 0) {
    PagingDestroyAddressSpace(address_space);
    return -1;
  }

  return pid;
}
