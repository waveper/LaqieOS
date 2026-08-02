org 0x7000
BITS 32

%define REAL_MODE_SEG   0x0700
%define TRAMPOLINE_BASE 0x7000
%define PM16_CODE_SEL   0x08
%define PM16_DATA_SEL   0x10

entry:
  cli
  lidt [real_mode_idt_descriptor]
  lgdt [real_mode_gdt_descriptor]
  jmp PM16_CODE_SEL:protected_mode_16_entry

BITS 16

protected_mode_16_entry:
  mov ax, PM16_DATA_SEL
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov sp, 0x7C00

  mov eax, cr0
  and eax, 0xFFFFFFFE
  mov cr0, eax
  jmp REAL_MODE_SEG:(real_mode_entry - TRAMPOLINE_BASE)

real_mode_entry:
  xor ax, ax
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov sp, 0x7C00

  mov ax, 0x5300
  xor bx, bx
  int 0x15
  jc fallback_poweroff
  cmp bx, 0x504D
  jne fallback_poweroff

  mov ax, 0x5301
  xor bx, bx
  int 0x15
  jc fallback_poweroff

  mov ax, 0x530E
  xor bx, bx
  mov cx, 0x0102
  int 0x15

  mov ax, 0x5307
  mov bx, 0x0001
  mov cx, 0x0003
  int 0x15

fallback_poweroff:
  mov ax, 0x2000
  mov dx, 0xB004
  out dx, ax

  mov dx, 0x0604
  out dx, ax

  mov ax, 0x3400
  mov dx, 0x4004
  out dx, ax

halt:
  cli
  hlt
  jmp halt

ALIGN 4
real_mode_gdt:
  dq 0

  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x9A
  db 0x00
  db 0x00

  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x92
  db 0x00
  db 0x00

real_mode_gdt_end:

real_mode_gdt_descriptor:
  dw real_mode_gdt_end - real_mode_gdt - 1
  dd real_mode_gdt

real_mode_idt_descriptor:
  dw 0x03FF
  dd 0x00000000
