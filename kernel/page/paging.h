#ifndef PAGING_H
#define PAGING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AddressSpace AddressSpace;

void PagingInit(void);
AddressSpace *PagingKernelAddressSpace(void);
AddressSpace *PagingCreateUserAddressSpace(void);
void PagingDestroyAddressSpace(AddressSpace *space);
int PagingMapUserRange(AddressSpace *space, uint32_t virt, uint32_t size);
int PagingMapUserPhysicalRange(AddressSpace *space, uint32_t virt,
                               uint32_t phys, uint32_t size, bool user);
int PagingUnmapUserRange(AddressSpace *space, uint32_t virt, uint32_t size);
int PagingMapKernelRange(uint32_t start, uint32_t size);
void *PagingVirtualToKernel(AddressSpace *space, uint32_t virt);
void PagingSwitchAddressSpace(AddressSpace *space);

#endif
