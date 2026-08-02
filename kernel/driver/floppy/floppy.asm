BITS 32

global FloppyReadRegister
global FloppyWriteRegister
global FloppyReadCMOS

FloppyReadRegister:
  push ebp
  mov ebp, esp
  push ebx

  mov ax, 0x3f0
  mov cx, [ebp+8]
  add ax, cx
  mov dx, ax
  in al, dx
  mov ebx, [ebp+12]
  mov [ebx], al

  pop ebx
  mov esp, ebp
  pop ebp
  xor eax, eax
  ret

FloppyWriteRegister:
  push ebp
  mov ebp, esp

  mov ax, 0x3f0
  mov cx, [ebp+8]
  add ax, cx
  mov dx, ax
  mov eax, [ebp+12]
  out dx, al

  mov esp, ebp
  pop ebp
  xor eax, eax
  ret

FloppyReadCMOS:
  push ebp
  mov ebp, esp
  push ebx

  ; disable interrupts so nothing else can break our CMOS...
  cli
  mov al, 0x10
  out 0x70, al
  xor eax, eax
  in al, 0x71
  sti

  ; store destination byte
  mov ebx, [ebp+8]
  mov [ebx], al

  pop ebx
  mov esp, ebp
  pop ebp
  xor eax, eax
  ret
