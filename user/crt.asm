BITS 32
global _start
global exit
global prints
global getchar
global putchar
global exec
global sys_kalloc
global sys_kfree
global mmap
global yield
global pidalive
global waitpid

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
  mov eax, 6
  int 0x80
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

; Raw page-granular allocation syscall used by the user-space heap allocator.
sys_kalloc:
  push ebp
  mov ebp, esp
  mov eax, 7
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

; Raw page-granular free syscall. The libc heap keeps freed blocks in a free
; list for reuse and only relies on the kernel to reclaim everything on exit,
; but this is provided for completeness.
sys_kfree:
  push ebp
  mov ebp, esp
  mov eax, 8
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

mmap:
  push ebp
  mov ebp, esp
  mov eax, 9
  mov ebx, [ebp+8]
  mov ecx, [ebp+12]
  mov edx, [ebp+16]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

yield:
  mov eax, 15
  int 0x80
  ret

pidalive:
  push ebp
  mov ebp, esp
  mov eax, 16
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret

waitpid:
  push ebp
  mov ebp, esp
  mov eax, 17
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret
