BITS 32

GLOBAL APMRealModeStart
GLOBAL APMRealModeEnd

SECTION .rodata

APMRealModeStart:
  incbin "driver/apm/apm16.bin"
APMRealModeEnd:
