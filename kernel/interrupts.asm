BITS 32

GLOBAL PageFaultISR
GLOBAL GeneralProtectionFaultISR
GLOBAL DivideByZeroISR
GLOBAL TimerISR
GLOBAL KeyboardISR
GLOBAL MouseISR
GLOBAL FloppyISR
GLOBAL SyscallISR
GLOBAL UnhandledInterruptISR
GLOBAL UnhandledErrorCodeISR

EXTERN DivideByZeroHandler
EXTERN TimerInterruptHandler
EXTERN KeyBoardInterruptHandler
EXTERN MouseInterruptHandler
EXTERN FloppyInterruptHandler
EXTERN SyscallHandler
EXTERN UnhandledInterruptHandler
EXTERN UnhandledErrorCodeHandler
EXTERN PageFaultHandler
EXTERN GeneralProtectionFaultHandler

SECTION .text

PageFaultISR:
  push ds ; save data segment descriptors
  push es
  push fs
  push gs

  mov dx, 0x10 ; Load Kernel Data Segment descriptor
  mov ds, dx
  mov es, dx

  pushad

  push esp ; pass pointer to saved registers as an argument
  call PageFaultHandler
  add esp, 4

  cmp eax, esp
  mov esp, eax
  popad
  pop gs ; restore data segment
  pop fs
  pop es
  pop ds
  jne .done
  add esp, 4
.done:
  iret

GeneralProtectionFaultISR:
  push ds ; save data segment descriptors
  push es
  push fs
  push gs

  mov dx, 0x10 ; Load Kernel Data Segment descriptor
  mov ds, dx
  mov es, dx

  pushad

  push esp ; pass pointer to saved registers as an argument
  call GeneralProtectionFaultHandler
  add esp, 4

  cmp eax, esp
  mov esp, eax
  popad
  pop gs ; restore data segment
  pop fs
  pop es
  pop ds
  jne .done
  add esp, 4
.done:
  iret

DivideByZeroISR:
  push dword 0
  pushad
  call DivideByZeroHandler
  popad
  add esp, 4
  iretd

UnhandledInterruptISR:
  push dword 0
  pushad
  call UnhandledInterruptHandler
  popad
  add esp, 4
  iretd

TimerISR:
  push ds
  push es
  push fs
  push gs
  pushad
  push esp
  call TimerInterruptHandler
  add esp, 4
  mov esp, eax
  popad
  pop gs
  pop fs
  pop es
  pop ds
  iretd

KeyboardISR:
  pushad
  call KeyBoardInterruptHandler
  popad
  iretd

MouseISR:
  pushad
  call MouseInterruptHandler
  popad
  iretd

FloppyISR:
  pushad
  call FloppyInterruptHandler
  popad
  iretd

SyscallISR:
  push ds
  push es
  push fs
  push gs

  mov dx, 0x10
  mov ds, dx
  mov es, dx

  sti
  pushad
  push esp
  call SyscallHandler
  add esp, 4
  mov esp, eax
  popad
  pop gs
  pop fs
  pop es
  pop ds
  iretd

UnhandledErrorCodeISR:
  pushad
  call UnhandledErrorCodeHandler
  popad
  add esp, 4
  iretd
