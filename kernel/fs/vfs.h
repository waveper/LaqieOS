#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct VFSRootFile_t VFSRootFile_t; // forward declarations
typedef struct VFSFileDescriptor_t VFSFileDescriptor_t;

typedef struct VFSFileNode_t {
  char *name;
  bool directory; // If it is true, then this descriptor was a directory
  VFSRootFile_t *parent;
  VFSRootFile_t *child;
  uint32_t size;
  void *data;
} VFSFileNode_t;

typedef struct VFSFileOperations_t {
  // TODO: This function will be a compatibility pointers to all filesystems
  int (*FileSize)(const char *path);
  int (*ReadFile)(const char *path, char **buffer);
  int (*WriteFile)(const char *path, const char *buffer, int bytes);
  int (*DeleteFile)(const char *path);
} VFSFileOperations;

typedef union {
  struct {
    uint8_t read : 1;
    uint8_t write : 1;
    uint8_t async : 1;
  } __attribute__((packed));
  uint8_t value;
} VFSFileDescriptorFlags_t;
typedef uint8_t VFSFileDescriptorFlags_t;

struct VFSFileDescriptor_t {
  VFSFileNode_t *node;
  uint32_t file_pointer;
  VFSFileDescriptorFlags_t flags;
};

struct VFSRootFile_t {
  VFSFileOperations operations;
  VFSFileNode_t *file_descriptors;
  uint32_t entries_array_size;
  uint32_t entries;
};

#endif
