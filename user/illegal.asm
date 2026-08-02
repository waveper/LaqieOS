BITS 32

  jmp test

db "LQE32"
db 0x01

; test if the kernel handle illegal opcode (privileged intruction usage)
test:
  cli ; diable interrupts
