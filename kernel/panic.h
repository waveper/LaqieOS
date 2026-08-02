#ifndef PANIC_H
#define PANIC_H

#define Panic(string) PanicImpl(__FILE__, __LINE__, string)

void PanicImpl(const char *const file, long line, const char *string) __attribute__((noreturn));

#endif
