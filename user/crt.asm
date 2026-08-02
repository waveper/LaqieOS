BITS 32
global _start
global exit
global prints
global getchar
global putchar
global exec

_start:
  jmp pre_main

db "LQE32" ; signature "LaQie Executable 32"
db 0x01 ; version 1

extern main

pre_main:
  call main

exit:
  mov eax, 1
  int 0x80
.hang:
  jmp .hang

prints:
  push ebp
  mov ebp, esp
  mov eax, 3
  mov ebx, [ebp+8]
  mov ecx, [ebp+12]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

exec:
  push ebp
  mov ebp, esp
  mov eax, 4
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

getchar:
  push ebp
  mov ebp, esp
  mov eax, 6
  int 0x80
  mov esp, ebp
  pop ebp
  ret

putchar:
  push ebp
  mov ebp, esp
  mov eax, 2
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret
