#include "vfs.h"
#include <stdbool.h>
#include <stdint.h>

VFSFileDescriptor_t *VFSRootFileSystem;

// TODO: VFS
void FSInit(void) {
  VFSRootFileSystem = 0; // Use fat12 as primary
  // mount ramfs to vfs
}
