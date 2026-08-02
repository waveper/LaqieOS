BITS 16

VideoPysicalAddress equ 0x0510

; this assembly file set video mode to 640 * 480
SetVideo:
  
  ; fetch info
  mov ax, 0x4F01
  mov cx, 0x0111 ; 640 * 480
  mov di, mode_info_block
  int 0x10

  cmp ax, 0x004F
  jne VBE_error_fetch

  ; set
  mov ax, 0x4F02
  mov bx, 0x4111
  int 0x10

  cmp ax, 0x004F
  jne VBE_error_set

  mov eax, [mode_info_block + 0x28]
  mov [VideoPysicalAddress], eax

  ret

VBE_error_set:
  mov si, VBE_es_msg
  call [print_fp]
  cli
.hlt:
  hlt
  jmp .hlt

VBE_error_fetch:
  mov si, VBE_ef_msg
  call [print_fp]
  cli
.hlt:
  hlt
  jmp .hlt

VBE_es_msg: db "Cannot Set Video to 640 * 480 16-bit", 0x0D, 0x0A, 0
VBE_ef_msg: db "Cannot fetch supported VBE video modes", 0x0D, 0x0A, 0

mode_info_block: times 256 db 0
