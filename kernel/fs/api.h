#ifndef FS_API_H
#define FS_API_H

int FileSize(const char *path);
int WriteFile(const char *path, const char *buffer, int bytes);
int DeleteFile(const char *path);
int ReadFile(const char *path, char **buffer);

#endif
