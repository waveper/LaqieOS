#include "ramfs.h"
#include "../page/kalloc.h"
#include "../panic.h"
#include "../../stdlib/stdmem.h"
#include "../../stdlib/string.h"

// WARN: This implementation is work in progress, do not link it to kernel binary

void InitRamFS(RamFileSystem *fs) {
  if (!fs) {
    Panic("Cannot initialize null Ram filesystem");
  }
  fs->count = 0;
  fs->capacity = 4; // Start small, little bytes
  fs->files = kalloc(fs->capacity * sizeof(RamFSFile));
  if (!fs->files) {
    Panic("Cannot Create Ram filesystem");
  }
}

int RamFSCreateFile(RamFileSystem *fs, const char *name, unsigned int size, const unsigned char *content) {
  if (!fs || !name || (size > 0 && !content)) return -1;

  if (fs->count >= fs->capacity) {
    unsigned int new_capacity = fs->capacity ? fs->capacity * 2 : 4;
    RamFSFile *temp = kalloc(new_capacity * sizeof(RamFSFile));
    if (!temp) {
      return -1;
    }
    memcpy(temp, fs->files, fs->count * sizeof(RamFSFile));
    kfree(fs->files);
    fs->files = temp;
    fs->capacity = new_capacity;
  }

  unsigned int name_len = strlen(name);
  char *name_copy = kalloc(name_len + 1);
  if (!name_copy) return -1;
  memcpy(name_copy, name, name_len + 1);

  unsigned char *content_copy = NULL;
  if (size > 0) {
    content_copy = kalloc(size);
    if (!content_copy) {
      kfree(name_copy);
      return -1;
    }
    memcpy(content_copy, content, size);
  }

  fs->files[fs->count].name = name_copy;
  fs->files[fs->count].size = size;
  fs->files[fs->count].content = content_copy;
  fs->count++;
  return 0;
}
