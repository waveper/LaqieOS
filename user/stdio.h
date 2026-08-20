#ifndef STDIO
#define STDIO

#include <stdint.h>

void prints(const char *str, int len);
int exec(const char *path);
void putchar(char c);
char getchar(void);
void *kalloc(uint32_t size);
void kfree(void *ptr);
int mmap(uint32_t phys, uint32_t virt, uint32_t size);

#endif
