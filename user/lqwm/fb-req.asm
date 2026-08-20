BITS 32
global RequestFrameBuffer

RequestFrameBuffer:
  push ebp
  mov ebp, esp
  mov eax, 5
  int 0x80
  mov esp, ebp
  pop ebp
  ret
