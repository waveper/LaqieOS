#include "../driver/floppy/floppy.h"
#include "../page/kalloc.h"
#include "../../stdlib/stdmem.h"
#include "../../stdlib/string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FAT12_SECTOR_SIZE 512u
#define FAT12_ATTR_DIRECTORY 0x10u
#define FAT12_ATTR_VOLUME_ID 0x08u
#define FAT12_ATTR_LFN 0x0Fu
#define FAT12_DIR_ENTRY_SIZE 32u
#define FAT12_FREE_CLUSTER 0x000u
#define FAT12_BAD_CLUSTER 0xFF7u
#define FAT12_EOC_MIN 0xFF8u
#define FAT12_EOC 0xFFFu

typedef struct __attribute__((__packed__)) {
  uint8_t jump[3];
  uint8_t oemName[8];
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectorCount;
  uint8_t fatCount;
  uint16_t rootEntryCount;
  uint16_t totalSectors16;
  uint8_t media;
  uint16_t fatSize16;
  uint16_t sectorsPerTrack;
  uint16_t headCount;
  uint32_t hiddenSectors;
  uint32_t totalSectors32;
} Fat12BootSector;

typedef struct __attribute__((__packed__)) {
  uint8_t name[11];
  uint8_t attr;
  uint8_t ntReserved;
  uint8_t creationTimeTenths;
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessDate;
  uint16_t firstClusterHigh;
  uint16_t writeTime;
  uint16_t writeDate;
  uint16_t firstClusterLow;
  uint32_t fileSize;
} Fat12DirEntry;

typedef struct {
  uint16_t bytesPerSector;
  uint8_t sectorsPerCluster;
  uint16_t reservedSectorCount;
  uint8_t fatCount;
  uint16_t rootEntryCount;
  uint16_t fatSizeSectors;
  uint32_t totalSectors;
  uint32_t rootDirSectors;
  uint32_t firstFatSector;
  uint32_t firstRootDirSector;
  uint32_t firstDataSector;
  uint32_t totalClusters;
} Fat12Info;

/*
NOTE: This FAT 12 Filesystem are only hardcoded to read/write to floppy and has no subdirectories/long file name support
*/

static int Fat12ReadSectors(int drivenum, uint32_t lba, uint32_t count, void *buffer) {
  uint8_t *dest = (uint8_t *)buffer;
  if (!buffer) return -1;

  for (uint32_t i = 0; i < count; ++i) {
    if (FloppyReadSector(drivenum, lba + i, dest + (i * FAT12_SECTOR_SIZE)) != 0) {
      return -1;
    }
  }
  return 0;
}

static int Fat12WriteSectors(int drivenum, uint32_t lba, uint32_t count, const void *buffer) {
  const uint8_t *src = (const uint8_t *)buffer;
  if (!buffer) return -1;

  for (uint32_t i = 0; i < count; ++i) {
    if (FloppyWriteSector(drivenum, lba + i, src + (i * FAT12_SECTOR_SIZE)) != 0) {
      return -1;
    }
  }
  return 0;
}

static int Fat12LoadInfo(int drivenum, Fat12Info *info) {
  uint8_t sector[FAT12_SECTOR_SIZE];
  const Fat12BootSector *bootSector = 0;
  uint32_t rootDirSectors = 0;
  uint32_t dataSectors = 0;
  uint32_t metadataSectors = 0;
  uint32_t totalSectors = 0;

  if (!info) return -1;
  if (Fat12ReadSectors(drivenum, 0, 1, sector) != 0) return -1;
  bootSector = (const Fat12BootSector *)sector;
  if (bootSector->bytesPerSector != FAT12_SECTOR_SIZE) return -1;
  if (bootSector->sectorsPerCluster == 0 || bootSector->fatCount == 0) return -1;
  if ((bootSector->sectorsPerCluster & (bootSector->sectorsPerCluster - 1)) != 0) return -1;
  if (bootSector->reservedSectorCount == 0 || bootSector->rootEntryCount == 0) return -1;
  if (bootSector->fatSize16 == 0) return -1;

  totalSectors = bootSector->totalSectors16 ? bootSector->totalSectors16 : bootSector->totalSectors32;
  if (totalSectors == 0) return -1;

  rootDirSectors = ((uint32_t)bootSector->rootEntryCount * FAT12_DIR_ENTRY_SIZE +
                    (bootSector->bytesPerSector - 1)) /
                   bootSector->bytesPerSector;
  if (rootDirSectors == 0) return -1;

  metadataSectors = (uint32_t)bootSector->reservedSectorCount +
                    ((uint32_t)bootSector->fatCount * bootSector->fatSize16) +
                    rootDirSectors;
  if (metadataSectors >= totalSectors) return -1;
  dataSectors = totalSectors - metadataSectors;
  if (dataSectors < bootSector->sectorsPerCluster) return -1;

  info->bytesPerSector = bootSector->bytesPerSector;
  info->sectorsPerCluster = bootSector->sectorsPerCluster;
  info->reservedSectorCount = bootSector->reservedSectorCount;
  info->fatCount = bootSector->fatCount;
  info->rootEntryCount = bootSector->rootEntryCount;
  info->fatSizeSectors = bootSector->fatSize16;
  info->totalSectors = totalSectors;
  info->rootDirSectors = rootDirSectors;
  info->firstFatSector = bootSector->reservedSectorCount;
  info->firstRootDirSector = info->firstFatSector + ((uint32_t)info->fatCount * info->fatSizeSectors);
  info->firstDataSector = info->firstRootDirSector + rootDirSectors;
  info->totalClusters = dataSectors / bootSector->sectorsPerCluster;
  if (info->totalClusters == 0) return -1;
  return 0;
}

static uint32_t Fat12ClusterSizeBytes(const Fat12Info *info) {
  return (uint32_t)info->bytesPerSector * info->sectorsPerCluster;
}

static uint32_t Fat12ClusterToLba(const Fat12Info *info, uint16_t cluster) {
  return info->firstDataSector + ((uint32_t)(cluster - 2) * info->sectorsPerCluster);
}

static int Fat12LoadFat(const Fat12Info *info, int drivenum, uint8_t **fatBuffer) {
  uint32_t fatBytes = 0;
  uint8_t *fat = 0;

  if (!info || !fatBuffer) return -1;
  fatBytes = (uint32_t)info->fatSizeSectors * info->bytesPerSector;
  fat = kalloc(fatBytes);
  if (!fat) return -1;

  if (Fat12ReadSectors(drivenum, info->firstFatSector, info->fatSizeSectors, fat) != 0) {
    kfree(fat);
    return -1;
  }

  *fatBuffer = fat;
  return 0;
}

static int Fat12FlushFat(const Fat12Info *info, int drivenum, const uint8_t *fatBuffer) {
  uint32_t fatBytes = 0;

  if (!info || !fatBuffer) return -1;
  fatBytes = (uint32_t)info->fatSizeSectors * info->bytesPerSector;

  for (uint8_t fatIndex = 0; fatIndex < info->fatCount; ++fatIndex) {
    uint32_t lba = info->firstFatSector + ((uint32_t)fatIndex * info->fatSizeSectors);
    if (Fat12WriteSectors(drivenum, lba, info->fatSizeSectors, fatBuffer) != 0) {
      return -1;
    }
  }

  (void)fatBytes;
  return 0;
}

static int Fat12LoadRootDirectory(const Fat12Info *info, int drivenum, Fat12DirEntry **entries) {
  uint32_t bytes = 0;
  Fat12DirEntry *root = 0;

  if (!info || !entries) return -1;
  bytes = info->rootDirSectors * info->bytesPerSector;
  root = kalloc(bytes);
  if (!root) return -1;

  if (Fat12ReadSectors(drivenum, info->firstRootDirSector, info->rootDirSectors, root) != 0) {
    kfree(root);
    return -1;
  }

  *entries = root;
  return 0;
}

static int Fat12FlushRootDirectory(const Fat12Info *info, int drivenum, const Fat12DirEntry *entries) {
  if (!info || !entries) return -1;
  return Fat12WriteSectors(drivenum, info->firstRootDirSector, info->rootDirSectors, entries);
}

static uint8_t *Fat12DuplicateBuffer(const void *source, uint32_t bytes) {
  uint8_t *copy = 0;

  if (bytes == 0) return 0;
  copy = kalloc(bytes);
  if (!copy) return 0;
  memcpy(copy, source, bytes);
  return copy;
}

static char Fat12ToUpper(char c) {
  if (c >= 'a' && c <= 'z') return (char)(c - ('a' - 'A'));
  return c;
}

static int Fat12NormalizeName(const char *path, char outName[11]) {
  const char *name = path;
  int baseLen = 0;
  int extLen = 0;
  bool seenDot = false;

  if (!path || !outName) return -1;

  if (strncmp(path, "FD", 2) == 0 || strncmp(path, "HD", 2) == 0) {
    if (path[2] < '0' || path[2] > '9' || path[3] != ':' || path[4] != '/') {
      return -1;
    }
    name = path + 5;
  } else if (*name == '/') {
    ++name;
  }

  if (*name == '\0') return -1;

  for (int i = 0; i < 11; ++i) outName[i] = ' ';

  while (*name) {
    char c = *name++;
    if (c == '/') {
      return -1;
    }
    if (c == '.') {
      if (seenDot || baseLen == 0) return -1;
      seenDot = true;
      continue;
    }
    c = Fat12ToUpper(c);
    if (seenDot) {
      if (extLen >= 3) return -1;
      outName[8 + extLen++] = c;
    } else {
      if (baseLen >= 8) return -1;
      outName[baseLen++] = c;
    }
  }

  return baseLen > 0 ? 0 : -1;
}

static bool Fat12EntryIsEnd(const Fat12DirEntry *entry) {
  return entry->name[0] == 0x00;
}

static bool Fat12EntryIsDeleted(const Fat12DirEntry *entry) {
  return entry->name[0] == 0xE5;
}

static bool Fat12EntryIsLFN(const Fat12DirEntry *entry) {
  return (entry->attr & FAT12_ATTR_LFN) == FAT12_ATTR_LFN;
}

static bool Fat12EntryIsRegularFile(const Fat12DirEntry *entry) {
  if (Fat12EntryIsDeleted(entry) || Fat12EntryIsLFN(entry)) return false;
  if ((entry->attr & FAT12_ATTR_DIRECTORY) != 0) return false;
  if ((entry->attr & FAT12_ATTR_VOLUME_ID) != 0) return false;
  return true;
}

static int Fat12FindEntry(Fat12DirEntry *entries, uint16_t entryCount, const char name[11]) {
  for (uint16_t i = 0; i < entryCount; ++i) {
    Fat12DirEntry *entry = &entries[i];
    if (Fat12EntryIsEnd(entry)) break;
    if (!Fat12EntryIsRegularFile(entry)) continue;

    bool matched = true;
    for (int j = 0; j < 11; ++j) {
      if ((char)entry->name[j] != name[j]) {
        matched = false;
        break;
      }
    }
    if (matched) return (int)i;
  }

  return -1;
}

static int Fat12FindFreeEntry(Fat12DirEntry *entries, uint16_t entryCount) {
  for (uint16_t i = 0; i < entryCount; ++i) {
    if (Fat12EntryIsEnd(&entries[i]) || Fat12EntryIsDeleted(&entries[i])) {
      return (int)i;
    }
  }
  return -1;
}

static uint16_t Fat12ReadEntry(const uint8_t *fat, uint16_t cluster) {
  uint32_t offset = ((uint32_t)cluster * 3u) / 2u;
  uint16_t value = (uint16_t)(fat[offset] | ((uint16_t)fat[offset + 1] << 8));

  if ((cluster & 1u) == 0) {
    return value & 0x0FFFu;
  }
  return value >> 4;
}

static void Fat12WriteEntry(uint8_t *fat, uint16_t cluster, uint16_t value) {
  uint32_t offset = ((uint32_t)cluster * 3u) / 2u;

  value &= 0x0FFFu;
  if ((cluster & 1u) == 0) {
    fat[offset] = (uint8_t)(value & 0xFFu);
    fat[offset + 1] = (uint8_t)((fat[offset + 1] & 0xF0u) | ((value >> 8) & 0x0Fu));
  } else {
    fat[offset] = (uint8_t)((fat[offset] & 0x0Fu) | ((value << 4) & 0xF0u));
    fat[offset + 1] = (uint8_t)((value >> 4) & 0xFFu);
  }
}

static bool Fat12IsDataCluster(const Fat12Info *info, uint16_t cluster) {
  return cluster >= 2 && ((uint32_t)cluster - 2u) < info->totalClusters;
}

static void Fat12FreeChain(const Fat12Info *info, uint8_t *fat, uint16_t startCluster) {
  uint16_t current = startCluster;
  uint32_t visited = 0;

  while (Fat12IsDataCluster(info, current) && visited < info->totalClusters) {
    uint16_t next = Fat12ReadEntry(fat, current);
    Fat12WriteEntry(fat, current, FAT12_FREE_CLUSTER);
    if (next >= FAT12_EOC_MIN || next == FAT12_BAD_CLUSTER) {
      break;
    }
    current = next;
    ++visited;
  }
}

static int Fat12ReadCluster(const Fat12Info *info, int drivenum, uint16_t cluster, void *buffer) {
  if (!Fat12IsDataCluster(info, cluster) || !buffer) return -1;
  return Fat12ReadSectors(drivenum, Fat12ClusterToLba(info, cluster), info->sectorsPerCluster, buffer);
}

static int Fat12WriteCluster(const Fat12Info *info, int drivenum, uint16_t cluster, const void *buffer) {
  if (!Fat12IsDataCluster(info, cluster) || !buffer) return -1;
  return Fat12WriteSectors(drivenum, Fat12ClusterToLba(info, cluster), info->sectorsPerCluster, buffer);
}

static int Fat12CollectChain(const Fat12Info *info, const uint8_t *fat, uint16_t startCluster,
                             uint16_t **outChain, uint32_t *outCount) {
  uint16_t *chain = 0;
  uint32_t count = 0;
  uint16_t current = startCluster;

  if (!info || !fat || !outChain || !outCount) return -1;
  *outChain = 0;
  *outCount = 0;

  if (startCluster == 0) return 0;
  if (!Fat12IsDataCluster(info, startCluster)) return -1;

  chain = kalloc(info->totalClusters * sizeof(uint16_t));
  if (!chain) return -1;

  while (Fat12IsDataCluster(info, current) && count < info->totalClusters) {
    uint16_t next = Fat12ReadEntry(fat, current);
    chain[count++] = current;
    if (next >= FAT12_EOC_MIN) {
      *outChain = chain;
      *outCount = count;
      return 0;
    }
    if (next < 2 || next == FAT12_BAD_CLUSTER) {
      break;
    }
    current = next;
  }

  kfree(chain);
  return -1;
}

static int Fat12AllocateChain(const Fat12Info *info, uint8_t *fat, uint32_t clustersNeeded,
                              uint16_t **outChain) {
  uint16_t *chain = 0;
  uint32_t found = 0;

  if (!info || !fat || !outChain) return -1;
  *outChain = 0;

  if (clustersNeeded == 0) return 0;

  chain = kalloc(clustersNeeded * sizeof(uint16_t));
  if (!chain) return -1;

  for (uint16_t cluster = 2; ((uint32_t)cluster - 2u) < info->totalClusters; ++cluster) {
    if (Fat12ReadEntry(fat, cluster) == FAT12_FREE_CLUSTER) {
      chain[found++] = cluster;
      if (found == clustersNeeded) {
        for (uint32_t i = 0; i < clustersNeeded; ++i) {
          uint16_t next = (i + 1 < clustersNeeded) ? chain[i + 1] : FAT12_EOC;
          Fat12WriteEntry(fat, chain[i], next);
        }
        *outChain = chain;
        return 0;
      }
    }
  }

  kfree(chain);
  return -1;
}

int Fat12FileSize(const char *path, int drivenum) {
  Fat12Info info;
  Fat12DirEntry *root = 0;
  char fatName[11];
  int entryIndex = -1;
  int result = -1;

  if (!path) return -1;
  if (Fat12NormalizeName(path, fatName) != 0) return -1;
  if (Fat12LoadInfo(drivenum, &info) != 0) return -1;
  if (Fat12LoadRootDirectory(&info, drivenum, &root) != 0) return -1;

  entryIndex = Fat12FindEntry(root, info.rootEntryCount, fatName);
  if (entryIndex >= 0) {
    result = (int)root[entryIndex].fileSize;
  }

  kfree(root);
  return result;
}

int Fat12ReadFile(const char *path, char **buffer, int drivenum) {
  Fat12Info info;
  Fat12DirEntry *root = 0;
  uint8_t *fat = 0;
  uint16_t *chain = 0;
  uint8_t *fileBuffer = 0;
  uint8_t *clusterBuffer = 0;
  uint32_t chainCount = 0;
  uint32_t remaining = 0;
  uint32_t clusterBytes = 0;
  uint32_t maxReadableBytes = 0;
  char fatName[11];
  int entryIndex = -1;
  int result = -1;

  if (!path || !buffer) return -1;
  *buffer = 0;
  if (Fat12NormalizeName(path, fatName) != 0) return -1;
  if (Fat12LoadInfo(drivenum, &info) != 0) return -1;
  if (Fat12LoadRootDirectory(&info, drivenum, &root) != 0) return -1;

  entryIndex = Fat12FindEntry(root, info.rootEntryCount, fatName);
  if (entryIndex < 0) goto cleanup;

  if (Fat12LoadFat(&info, drivenum, &fat) != 0) goto cleanup;

  remaining = root[entryIndex].fileSize;
  if (remaining == 0) {
    fileBuffer = kalloc(1);
    if (!fileBuffer) goto cleanup;
    fileBuffer[0] = '\0';
    *buffer = (char *)fileBuffer;
    result = 0;
    fileBuffer = 0;
    goto cleanup;
  }

  if (Fat12CollectChain(&info, fat, root[entryIndex].firstClusterLow, &chain, &chainCount) != 0) {
    goto cleanup;
  }

  clusterBytes = Fat12ClusterSizeBytes(&info);
  maxReadableBytes = chainCount * clusterBytes;
  if (remaining > maxReadableBytes || remaining == 0xFFFFFFFFu) goto cleanup;

  fileBuffer = kalloc(remaining + 1);
  if (!fileBuffer) goto cleanup;
  fileBuffer[remaining] = '\0';

  clusterBuffer = kalloc(clusterBytes);
  if (!clusterBuffer) goto cleanup;

  uint32_t copied = 0;
  for (uint32_t i = 0; i < chainCount && remaining > 0; ++i) {
    uint32_t toCopy = remaining < clusterBytes ? remaining : clusterBytes;
    if (Fat12ReadCluster(&info, drivenum, chain[i], clusterBuffer) != 0) goto cleanup;
    memcpy(fileBuffer + copied, clusterBuffer, toCopy);
    copied += toCopy;
    remaining -= toCopy;
  }

  if (remaining != 0) goto cleanup;

  *buffer = (char *)fileBuffer;
  result = (int)root[entryIndex].fileSize;
  fileBuffer = 0;

cleanup:
  if (clusterBuffer) kfree(clusterBuffer);
  if (chain) kfree(chain);
  if (fat) kfree(fat);
  if (root) kfree(root);
  if (fileBuffer) kfree(fileBuffer);
  return result;
}

int Fat12WriteFile(const char *path, const char *buffer, int bytes, int drivenum) {
  Fat12Info info;
  Fat12DirEntry *root = 0;
  uint8_t *fat = 0;
  uint8_t *fatBackup = 0;
  Fat12DirEntry *rootBackup = 0;
  uint16_t *newChain = 0;
  uint8_t *clusterBuffer = 0;
  uint32_t clusterBytes = 0;
  uint32_t clustersNeeded = 0;
  uint32_t fatBytes = 0;
  uint32_t rootBytes = 0;
  uint16_t startCluster = 0;
  char fatName[11];
  int entryIndex = -1;
  int result = -1;

  if (!path || bytes < 0 || (bytes > 0 && !buffer)) return -1;
  if (Fat12NormalizeName(path, fatName) != 0) return -1;
  if (Fat12LoadInfo(drivenum, &info) != 0) return -1;
  if (Fat12LoadRootDirectory(&info, drivenum, &root) != 0) return -1;
  if (Fat12LoadFat(&info, drivenum, &fat) != 0) goto cleanup;

  fatBytes = (uint32_t)info.fatSizeSectors * info.bytesPerSector;
  rootBytes = info.rootDirSectors * info.bytesPerSector;
  fatBackup = Fat12DuplicateBuffer(fat, fatBytes);
  rootBackup = (Fat12DirEntry *)Fat12DuplicateBuffer(root, rootBytes);
  if (!fatBackup || !rootBackup) goto cleanup;

  entryIndex = Fat12FindEntry(root, info.rootEntryCount, fatName);
  if (entryIndex < 0) {
    entryIndex = Fat12FindFreeEntry(root, info.rootEntryCount);
    if (entryIndex < 0) goto cleanup;
    memset(&root[entryIndex], 0, sizeof(Fat12DirEntry));
  } else {
    Fat12FreeChain(&info, fat, root[entryIndex].firstClusterLow);
  }

  clusterBytes = Fat12ClusterSizeBytes(&info);
  clustersNeeded = (bytes == 0) ? 0 : ((uint32_t)bytes + clusterBytes - 1) / clusterBytes;
  if (Fat12AllocateChain(&info, fat, clustersNeeded, &newChain) != 0) goto cleanup;

  if (clustersNeeded > 0) {
    clusterBuffer = kalloc(clusterBytes);
    if (!clusterBuffer) goto cleanup;

    uint32_t written = 0;
    startCluster = newChain[0];
    for (uint32_t i = 0; i < clustersNeeded; ++i) {
      uint32_t remaining = (uint32_t)bytes - written;
      uint32_t toCopy = remaining < clusterBytes ? remaining : clusterBytes;
      memset(clusterBuffer, 0, clusterBytes);
      memcpy(clusterBuffer, buffer + written, toCopy);
      if (Fat12WriteCluster(&info, drivenum, newChain[i], clusterBuffer) != 0) goto cleanup;
      written += toCopy;
    }
  }

  memcpy(root[entryIndex].name, fatName, 11);
  root[entryIndex].attr = 0x20;
  root[entryIndex].ntReserved = 0;
  root[entryIndex].creationTimeTenths = 0;
  root[entryIndex].creationTime = 0;
  root[entryIndex].creationDate = 0;
  root[entryIndex].lastAccessDate = 0;
  root[entryIndex].firstClusterHigh = 0;
  root[entryIndex].writeTime = 0;
  root[entryIndex].writeDate = 0;
  root[entryIndex].firstClusterLow = startCluster;
  root[entryIndex].fileSize = (uint32_t)bytes;

  if (Fat12FlushRootDirectory(&info, drivenum, root) != 0) goto rollback;
  if (Fat12FlushFat(&info, drivenum, fat) != 0) goto rollback;

  result = bytes;
  goto cleanup;

rollback:
  if (rootBackup) {
    Fat12FlushRootDirectory(&info, drivenum, rootBackup);
  }
  if (fatBackup) {
    Fat12FlushFat(&info, drivenum, fatBackup);
  }

cleanup:
  if (clusterBuffer) kfree(clusterBuffer);
  if (newChain) kfree(newChain);
  if (rootBackup) kfree(rootBackup);
  if (fatBackup) kfree(fatBackup);
  if (fat) kfree(fat);
  if (root) kfree(root);
  return result;
}

int Fat12DeleteFile(const char *path, int drivenum) {
  Fat12Info info;
  Fat12DirEntry *root = 0;
  uint8_t *fat = 0;
  uint8_t *fatBackup = 0;
  Fat12DirEntry *rootBackup = 0;
  uint32_t fatBytes = 0;
  uint32_t rootBytes = 0;
  char fatName[11];
  int entryIndex = -1;
  int result = -1;

  if (!path) return -1;
  if (Fat12NormalizeName(path, fatName) != 0) return -1;
  if (Fat12LoadInfo(drivenum, &info) != 0) return -1;
  if (Fat12LoadRootDirectory(&info, drivenum, &root) != 0) return -1;
  if (Fat12LoadFat(&info, drivenum, &fat) != 0) goto cleanup;

  fatBytes = (uint32_t)info.fatSizeSectors * info.bytesPerSector;
  rootBytes = info.rootDirSectors * info.bytesPerSector;
  fatBackup = Fat12DuplicateBuffer(fat, fatBytes);
  rootBackup = (Fat12DirEntry *)Fat12DuplicateBuffer(root, rootBytes);
  if (!fatBackup || !rootBackup) goto cleanup;

  entryIndex = Fat12FindEntry(root, info.rootEntryCount, fatName);
  if (entryIndex < 0) goto cleanup;

  Fat12FreeChain(&info, fat, root[entryIndex].firstClusterLow);
  memset(&root[entryIndex], 0, sizeof(Fat12DirEntry));
  root[entryIndex].name[0] = 0xE5;

  if (Fat12FlushRootDirectory(&info, drivenum, root) != 0) goto rollback;
  if (Fat12FlushFat(&info, drivenum, fat) != 0) goto rollback;

  result = 0;
  goto cleanup;

rollback:
  if (rootBackup) {
    Fat12FlushRootDirectory(&info, drivenum, rootBackup);
  }
  if (fatBackup) {
    Fat12FlushFat(&info, drivenum, fatBackup);
  }

cleanup:
  if (rootBackup) kfree(rootBackup);
  if (fatBackup) kfree(fatBackup);
  if (fat) kfree(fat);
  if (root) kfree(root);
  return result;
}
