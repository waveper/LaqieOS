#ifndef STDIO
#define STDIO

#include <stdint.h>

_Noreturn void exit(void);
void prints(const char *str, int len);
int exec(const char *path);
void putchar(char c);
char getchar(void);
void *malloc(uint32_t size);
void free(void *ptr);
int mmap(uint32_t phys, uint32_t virt, uint32_t size);
void yield(void);

#endif
