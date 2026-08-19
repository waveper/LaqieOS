BITS 32
global GUIInit

GUIInit:
  push ebp
  mov ebp, esp
  mov eax, 5
  mov ebx, [ebp+8]
  int 0x80
  mov esp, ebp
  pop ebp
  ret
