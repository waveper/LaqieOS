#include "../include/serial/serial.h"
#include "../stdlib/stdmem.h"
#include "../stdlib/string.h"
#include "fs/api.h"
#include "page/bitmap.h"
#include <stdint.h>

#define MAX_SYMBOLS_DUMP 10

typedef struct SymbolTable_t {
  uint32_t symbol_address;
  char *symbol_name;
} SymbolTable_t;

int symbol_loaded = 0;
int symbol_count = 0;
SymbolTable_t *SymbolTable;

static int LineLength(const char *buffer, int size) {
  int len = 0;
  while (len < size && buffer[len] != '\n' && buffer[len] != '\0')
    len++;
  return len;
}

static void FreeParsedSymbols(int count) {
  if (!SymbolTable)
    return;

  for (int i = 0; i < count; i++) {
    if (SymbolTable[i].symbol_name)
      KFree(SymbolTable[i].symbol_name);
  }

  KFree(SymbolTable);
  SymbolTable = 0;
  symbol_count = 0;
  symbol_loaded = 0;
}

static int ParseSymbol(const char *buffer, int size) {
  if (SymbolTable)
    FreeParsedSymbols(symbol_count);

  int new_lines = 0;
  for (int i = 0; i < size;) {
    int line_len = LineLength(buffer + i, size - i);
    if (line_len > 0)
      new_lines++;
    i += line_len + 1;
  }

  if (new_lines == 0)
    return -1;

  SymbolTable = KAlloc(sizeof(SymbolTable_t) * new_lines);
  if (!SymbolTable)
    return -1;
  memset(SymbolTable, 0, sizeof(SymbolTable_t) * new_lines);

  int symbol_idx = 0;
  for (int i = 0; i < size && symbol_idx < new_lines;) {
    int line_len = LineLength(buffer + i, size - i);
    if (line_len == 0) {
      i++;
      continue;
    }

    int name_start = i;
    while (name_start < i + line_len && buffer[name_start] != ' ')
      name_start++;
    while (name_start < i + line_len && buffer[name_start] == ' ')
      name_start++;

    if (name_start >= i + line_len) {
      FreeParsedSymbols(symbol_idx);
      return -1;
    }

    int strsize = i + line_len - name_start;
    SymbolTable[symbol_idx].symbol_address = hex_to_uint32(buffer + i);
    SymbolTable[symbol_idx].symbol_name = KAlloc(strsize + 1);
    if (!SymbolTable[symbol_idx].symbol_name) {
      FreeParsedSymbols(symbol_idx);
      return -1;
    }

    memcpy(SymbolTable[symbol_idx].symbol_name, buffer + name_start, strsize);
    SymbolTable[symbol_idx].symbol_name[strsize] = '\0';
    symbol_idx++;
    i += line_len + 1;
  }

  symbol_count = symbol_idx;
  symbol_loaded = 1;
  return 0;
}

static uint32_t MatchSymbolAddress(uint32_t address) {
  if (!symbol_loaded || !SymbolTable || symbol_count <= 0)
    return 0;

  uint32_t best_match = 0;
  for (int i = 0; i < symbol_count; i++) {
    uint32_t symbol_address = SymbolTable[i].symbol_address;
    if (symbol_address <= address && symbol_address >= best_match) {
      best_match = symbol_address;
    }
  }

  return best_match;
}

int LoadSymbol(void) {
  int filesize = FileSize("FD0:/dump.sym");
  if (filesize < 0) {
    SerialPrintf("Warning: No symbol file located\r\n");
    return -1;
  }
  char *filebuffer = 0;
  if (ReadFile("FD0:/dump.sym", &filebuffer) != filesize) {
    if (filebuffer)
      KFree(filebuffer);
    return -1;
  }
  int res = ParseSymbol(filebuffer, filesize);
  KFree(filebuffer);
  return res;
}

static char *FindSymbolNameByAddress(uint32_t address) {
  if (!symbol_loaded || !SymbolTable)
    return NULL;

  for (int i = 0; i < symbol_count; i++) {
    if (SymbolTable[i].symbol_address == address)
      return SymbolTable[i].symbol_name;
  }
  return NULL;
}

static void PrintSymbolFrame(uint32_t address) {
  uint32_t match = MatchSymbolAddress(address);
  char *name = FindSymbolNameByAddress(match);

  if (match == 0 || !name) {
    SerialPrintf("%x: <unknown>\r\n", address);
    return;
  }

  SerialPrintf("%x: <%s>+%x\r\n", match, name, address - match);
}

void DumpStackTrace(uint32_t eip, uint32_t esp) {
  SerialPrintf("Latest Stack Traces:\r\n");
  if (!symbol_loaded || !SymbolTable || symbol_count <= 0) {
    SerialPrintf("%x: <symbols unavailable>\r\n", eip);
    return;
  }

  uint32_t *stack_pointer = (uint32_t *)esp;
  int dump_count = 0;
  PrintSymbolFrame(eip);

  for (int i = 0; i < 256 && dump_count < MAX_SYMBOLS_DUMP; i++) {
    uint32_t match_output = MatchSymbolAddress(stack_pointer[i]);
    if (match_output != 0) {
      PrintSymbolFrame(stack_pointer[i]);
      dump_count++;
    }
  }
}
