BITS 32
global RequestFrameBuffer
global RequestMousePointer

RequestFrameBuffer:
  push ebp
  mov ebp, esp
  mov eax, 5
  int 0x80
  mov esp, ebp
  pop ebp
  ret

RequestMousePointer:
  push ebp
  mov ebp, esp
  mov eax, 14
  int 0x80
  mov esp, ebp
  pop ebp
  ret
