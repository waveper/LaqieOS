#ifndef FAT12_H
#define FAT12_H

int Fat12FileSize(const char *path, int drivenum);
int Fat12ReadFile(const char *path, char **buffer, int drivenum);
int Fat12WriteFile(const char *path, const char *buffer, int bytes, int drivenum);
int Fat12DeleteFile(const char *path, int drivenum);

#endif
