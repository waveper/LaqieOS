#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdint.h>

int LoadSymbol(void);
void DumpStackTrace(uint32_t eip, uint32_t esp);

#endif
