#include "../../../stdlib/stdmem.h"
#include "apm.h"

extern const unsigned char APMRealModeStart[];
extern const unsigned char APMRealModeEnd[];

_Noreturn void APMShutdown(void) {
  void (*trampoline)(void) = (void (*)(void))0x7000;
  uint32_t trampoline_size = (uint32_t)(APMRealModeEnd - APMRealModeStart);
  memcpy((void *)0x7000, APMRealModeStart, trampoline_size);
  trampoline();
  while (1) {
    asm volatile("cli; hlt");
  }
}
