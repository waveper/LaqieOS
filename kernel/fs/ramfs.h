#ifndef RAMFS
#define RAMFS

typedef struct RamFSFile {
  char *name;
  unsigned int size;
  unsigned char *content;
} RamFSFile;

typedef struct RamFileSystem {
  RamFSFile *files;
  unsigned int count;
  unsigned int capacity;
} RamFileSystem;

void InitRamFS(RamFileSystem *fs);
int RamFSCreateFile(RamFileSystem *fs, const char *name, unsigned int size, const unsigned char *content);

#endif
