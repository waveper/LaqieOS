BITS 32
GLOBAL _start
GLOBAL BootInstalledMemory
EXTERN KMain
EXTERN KernelBssStart
EXTERN KernelBssEnd

BOOT_INFO_ADDR equ 0x0500
VideoPysicalAddress equ 0x0510

SECTION .bss
align 4
BootInstalledMemory:
  resd 1

SECTION .text
_start:
  cld
  mov edi, KernelBssStart
  mov ecx, KernelBssEnd
  sub ecx, edi
  xor eax, eax
  rep stosb

  mov eax, [VideoPysicalAddress]
  push eax
  mov eax, [BOOT_INFO_ADDR]
  mov [BootInstalledMemory], eax
  call KMain

.hang:
  hlt
  jmp .hang
