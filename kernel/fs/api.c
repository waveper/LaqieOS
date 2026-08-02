#include <stdbool.h>
#include "../../stdlib/string.h"
#include "fat12.h"

/*
Example Filesystem API will be

______File("FD1:/foo/bar.txt", etc);
*/

// NOTE: HD are HardDisk, it will implement later as the "ide" driver, also implememt Fat 32 after

static int CharNumToInt(char c) {
  if (c < '0' || c > '9') {
    return -1;
  }
  return c - '0';
}

static int ValidateDrivePath(const char *path) {
  if (!path) return -1;
  if (path[0] == '\0' || path[1] == '\0' || path[2] == '\0' || path[3] == '\0' || path[4] == '\0') {
    return -1;
  }
  if (strncmp(path, "FD", 2) != 0 && strncmp(path, "HD", 2) != 0) {
    return -1;
  }
  if (path[3] != ':' || path[4] != '/') {
    return -1;
  }
  return CharNumToInt(path[2]);
}

int FileSize(const char *path) {
  int drive_number = ValidateDrivePath(path);
  if (drive_number < 0) return -1;

  if (strncmp(path, "FD", 2) == 0) {
    return Fat12FileSize(path, drive_number);
  } else if (strncmp(path, "HD", 2) == 0) {
    return -1;
  }
  return -1;
}

int WriteFile(const char *path, const char *buffer, int bytes) {
  int drive_number = ValidateDrivePath(path);
  if (drive_number < 0) return -1;

  if (strncmp(path, "FD", 2) == 0) { // Floppy disk
    return Fat12WriteFile(path, buffer, bytes, drive_number);
  } else if (strncmp(path, "HD", 2) == 0) { // harddisk
    return -1;
  }
  return -1;
}

int ReadFile(const char *path, char **buffer) {
  int drive_number = ValidateDrivePath(path);
  if (drive_number < 0) return -1;

  if (strncmp(path, "FD", 2) == 0) {
    return Fat12ReadFile(path, buffer, drive_number);
  } else if (strncmp(path, "HD", 2) == 0) {
    return -1;
  }
  return -1;
}

int DeleteFile(const char *path) {
  int drive_number = ValidateDrivePath(path);
  if (drive_number < 0) return -1;

  if (strncmp(path, "FD", 2) == 0) {
    return Fat12DeleteFile(path, drive_number);
  } else if (strncmp(path, "HD", 2) == 0) {
    return -1;
  }

  return -1;
}
