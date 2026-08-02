#include "floppy.h"
#include "../../../stdlib/stdmem.h"
#include "../../../include/io.h"
#include <stdint.h>

#define FDC_IRQ_FLAG_ADDR ((volatile uint8_t *)0x505)
#define FLOPPY_DMA_BUFFER ((uint8_t *)0x8000)
#define FLOPPY_DMA_BUFFER_PHYS 0x8000u

#define DMA_SINGLE_CHANNEL_MASK 0x0A
#define DMA_SINGLE_CHANNEL_MODE 0x0B
#define DMA_CLEAR_FF           0x0C
#define DMA_CH2_ADDR           0x04
#define DMA_CH2_COUNT          0x05
#define DMA_CH2_PAGE           0x81

#define FLOPPY_DOR_DMA_IRQ     0x0C
#define FLOPPY_CMD_MFM         0x40

#define FLOPPY_SECTOR_SIZE     512
#define FLOPPY_GAP3_RW         0x1B
#define FLOPPY_TIMEOUT         1000000u

typedef struct {
  uint8_t st0;
  uint8_t st1;
  uint8_t st2;
  uint8_t cylinder;
  uint8_t head;
  uint8_t sector;
  uint8_t size;
} FloppyResult;

static FloppyDriveList g_floppy_drives;
static bool g_floppy_initialized = false;
static int g_default_drive = -1;
static int g_floppy_last_error = 0;
static uint32_t g_floppy_last_result = 0;

static void FloppySpinDelay(uint32_t count) {
  while (count-- > 0) {
    asm volatile("" ::: "memory");
  }
}

static void FloppySelectDataRate(uint8_t rate) {
  FloppyWriteRegister(FDC_CCR, rate & 0x03);
}

static void FloppySetDor(uint8_t drive, bool motorOn) {
  uint8_t dor = FLOPPY_DOR_DMA_IRQ | (drive & 0x03);
  if (motorOn) {
    dor |= (uint8_t)(0x10u << drive);
  }
  FloppyWriteRegister(FDC_DOR, dor);
}

static int FloppyWaitFIFO(uint8_t mask, uint8_t expected) {
  uint8_t msr = 0;
  for (uint32_t i = 0; i < FLOPPY_TIMEOUT; ++i) {
    if (FloppyReadRegister(FDC_MSR, &msr) != 0) {
      return -1;
    }
    if ((msr & mask) == expected) {
      return 0;
    }
  }
  return -1;
}

static int FloppyReadFIFO(uint8_t *dest) {
  if (FloppyWaitFIFO(0xD0, 0xD0) != 0) return -1;
  if (FloppyReadRegister(FDC_FIFO, dest) != 0) return -1;
  return 0;
}

static int FloppyWriteFIFO(uint8_t Byte) {
  if (FloppyWaitFIFO(0xC0, 0x80) != 0) return -1;
  return FloppyWriteRegister(FDC_FIFO, Byte);
}

static int FloppyWaitIRQ(void) {
  volatile uint8_t *hasIRQ = FDC_IRQ_FLAG_ADDR;
  for (uint32_t i = 0; i < FLOPPY_TIMEOUT; ++i) {
    if (*hasIRQ != 0) {
      *hasIRQ = 0;
      return 0;
    }
  }
  return -1;
}

static int FloppySenseInterrupt(uint8_t *st0, uint8_t *cylinder) {
  if (FloppyWriteFIFO(FDC_CMD_SENSE_INTERRUPT) != 0) return -1;
  if (FloppyReadFIFO(st0) != 0) return -1;
  if (FloppyReadFIFO(cylinder) != 0) return -1;
  return 0;
}

static int FloppySpecify(void) {
  if (FloppyWriteFIFO(FDC_CMD_SPECIFY) != 0) return -1;
  if (FloppyWriteFIFO(0xDF) != 0) return -1;
  if (FloppyWriteFIFO(0x02) != 0) return -1;
  return 0;
}

static int FloppyConfigure(bool impliedSeek, bool disableFifo, bool dpm, uint8_t threshold) {
  uint8_t cmdByte = (uint8_t)((impliedSeek << 6) | (disableFifo << 5) | (dpm << 4) | ((threshold - 1) & 0x0F));
  if (FloppyWriteFIFO(FDC_CMD_CONFIGURE) != 0) return -1;
  if (FloppyWriteFIFO(0) != 0) return -1;
  if (FloppyWriteFIFO(cmdByte) != 0) return -1;
  if (FloppyWriteFIFO(0) != 0) return -1;
  return 0;
}

static int FloppyLock(void) {
  uint8_t lockByte = 0;
  if (FloppyWriteFIFO(FDC_CMD_LOCK | 0x80) != 0) return -1;
  if (FloppyReadFIFO(&lockByte) != 0) return -1;
  if ((lockByte & 0x10) == 0) {
    return -1;
  }
  return 0;
}

static int FloppyResetController(void) {
  volatile uint8_t *hasIRQ = FDC_IRQ_FLAG_ADDR;
  *hasIRQ = 0;

  FloppyWriteRegister(FDC_DOR, 0x00);
  FloppySpinDelay(10000);
  FloppyWriteRegister(FDC_DOR, FLOPPY_DOR_DMA_IRQ);
  if (FloppyWaitIRQ() != 0) {
    return -1;
  }

  for (int i = 0; i < 4; ++i) {
    uint8_t st0 = 0;
    uint8_t cyl = 0;
    if (FloppySenseInterrupt(&st0, &cyl) != 0) return -1;
  }

  return 0;
}

static int FloppyRecalibrate(int drive) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    FloppySetDor((uint8_t)drive, true);
    if (FloppyWriteFIFO(FDC_CMD_RECALIBRATE) != 0) return -1;
    if (FloppyWriteFIFO((uint8_t)drive) != 0) return -1;
    if (FloppyWaitIRQ() != 0) continue;

    uint8_t st0 = 0;
    uint8_t cyl = 0xFF;
    if (FloppySenseInterrupt(&st0, &cyl) != 0) continue;
    if ((st0 & 0xC0) == 0 && cyl == 0) {
      return 0;
    }
  }
  return -1;
}

static int FloppySeek(int drive, uint8_t cylinder, uint8_t head) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    if (FloppyWriteFIFO(FDC_CMD_SEEK) != 0) return -1;
    if (FloppyWriteFIFO((uint8_t)((head << 2) | (drive & 0x03))) != 0) return -1;
    if (FloppyWriteFIFO(cylinder) != 0) return -1;
    if (FloppyWaitIRQ() != 0) continue;

    uint8_t st0 = 0;
    uint8_t sensedCylinder = 0xFF;
    if (FloppySenseInterrupt(&st0, &sensedCylinder) != 0) continue;
    if ((st0 & 0xC0) == 0 && sensedCylinder == cylinder) {
      return 0;
    }
  }
  return -1;
}

static int FloppyReadResult(FloppyResult *result) {
  uint8_t *bytes = (uint8_t *)result;
  for (int i = 0; i < (int)sizeof(FloppyResult); ++i) {
    if (FloppyReadFIFO(&bytes[i]) != 0) {
      return -1;
    }
  }
  return 0;
}

static int FloppySetupDMA(bool isWrite) {
  uint16_t offset = (uint16_t)(FLOPPY_DMA_BUFFER_PHYS & 0xFFFF);
  uint8_t page = (uint8_t)((FLOPPY_DMA_BUFFER_PHYS >> 16) & 0xFF);
  uint16_t count = FLOPPY_SECTOR_SIZE - 1;

  outb(DMA_SINGLE_CHANNEL_MASK, 0x06);
  outb(DMA_CLEAR_FF, 0xFF);
  outb(DMA_SINGLE_CHANNEL_MODE, isWrite ? 0x4A : 0x46);
  outb(DMA_CH2_ADDR, offset & 0xFF);
  outb(DMA_CH2_ADDR, (offset >> 8) & 0xFF);
  outb(DMA_CH2_PAGE, page);
  outb(DMA_CLEAR_FF, 0xFF);
  outb(DMA_CH2_COUNT, count & 0xFF);
  outb(DMA_CH2_COUNT, (count >> 8) & 0xFF);
  outb(DMA_SINGLE_CHANNEL_MASK, 0x02);
  return 0;
}

static int FloppyLbaToChs(const FloppyDrive *drive, uint32_t lba,
                          uint8_t *cylinder, uint8_t *head, uint8_t *sector) {
  if (drive == 0 || drive->sectorsPerTrack == 0 || drive->heads == 0) {
    return -1;
  }

  uint32_t sectorsPerCylinder = (uint32_t)drive->sectorsPerTrack * drive->heads;
  *cylinder = (uint8_t)(lba / sectorsPerCylinder);
  uint32_t tmp = lba % sectorsPerCylinder;
  *head = (uint8_t)(tmp / drive->sectorsPerTrack);
  *sector = (uint8_t)((tmp % drive->sectorsPerTrack) + 1);
  if (*cylinder >= drive->tracks) {
    return -1;
  }
  return 0;
}

static int FloppyDetectType(uint8_t cmosType, FloppyDrive *drive) {
  drive->cmosType = cmosType;
  drive->exists = (cmosType != 0);
  drive->supported = false;
  drive->motorDelay = 0;

  switch (cmosType) {
  case 1:
    drive->sectorsPerTrack = 9;
    drive->tracks = 40;
    drive->heads = 2;
    drive->datarate = 0;
    drive->supported = true;
    return 0;
  case 2:
    drive->sectorsPerTrack = 15;
    drive->tracks = 80;
    drive->heads = 2;
    drive->datarate = 0;
    drive->supported = true;
    return 0;
  case 3:
    drive->sectorsPerTrack = 9;
    drive->tracks = 80;
    drive->heads = 2;
    drive->datarate = 0;
    drive->supported = true;
    return 0;
  case 4:
    drive->sectorsPerTrack = 18;
    drive->tracks = 80;
    drive->heads = 2;
    drive->datarate = 0;
    drive->supported = true;
    return 0;
  case 5:
    drive->sectorsPerTrack = 36;
    drive->tracks = 80;
    drive->heads = 2;
    drive->datarate = 3;
    drive->supported = true;
    return 0;
  default:
    return -1;
  }
}

int FloppyGetList(FloppyDriveList *dest) {
  uint8_t cmosReading = 0;
  memset(dest, 0, sizeof(FloppyDriveList));
  if (FloppyReadCMOS(&cmosReading) != 0) {
    return -1;
  }

  FloppyDetectType((uint8_t)((cmosReading >> 4) & 0x0F), &dest->drives[0]);
  FloppyDetectType((uint8_t)(cmosReading & 0x0F), &dest->drives[1]);
  return 0;
}

void FloppyHandleIRQ(void) {
  *FDC_IRQ_FLAG_ADDR = 1;
}

static int FloppyTransferSector(int drive, uint32_t lba, void *buffer, bool isWrite) {
  int status = -1;
  g_floppy_last_error = 0;
  g_floppy_last_result = 0;

  if (!g_floppy_initialized || drive < 0 || drive >= 2 || buffer == 0) {
    g_floppy_last_error = 1;
    return -1;
  }

  FloppyDrive *floppy = &g_floppy_drives.drives[drive];
  if (!floppy->exists || !floppy->supported) {
    g_floppy_last_error = 2;
    return -1;
  }

  uint8_t cylinder = 0;
  uint8_t head = 0;
  uint8_t sector = 0;
  if (FloppyLbaToChs(floppy, lba, &cylinder, &head, &sector) != 0) {
    g_floppy_last_error = 3;
    return -1;
  }

  FloppySelectDataRate(floppy->datarate);
  FloppySetDor((uint8_t)drive, true);
  FloppySpinDelay(50000);
  if (FloppySeek(drive, cylinder, head) != 0) {
    g_floppy_last_error = 4;
    goto cleanup;
  }

  if (isWrite) {
    memcpy(FLOPPY_DMA_BUFFER, buffer, FLOPPY_SECTOR_SIZE);
  }
  FloppySetupDMA(isWrite);

  uint8_t command = (uint8_t)((isWrite ? FDC_CMD_WRITE_DATA : FDC_CMD_READ_DATA) |
                              FLOPPY_CMD_MFM);
  if (FloppyWriteFIFO(command) != 0) {
    g_floppy_last_error = 5;
    goto cleanup;
  }
  if (FloppyWriteFIFO((uint8_t)((head << 2) | (drive & 0x03))) != 0) {
    g_floppy_last_error = 6;
    goto cleanup;
  }
  if (FloppyWriteFIFO(cylinder) != 0) {
    g_floppy_last_error = 7;
    goto cleanup;
  }
  if (FloppyWriteFIFO(head) != 0) {
    g_floppy_last_error = 8;
    goto cleanup;
  }
  if (FloppyWriteFIFO(sector) != 0) {
    g_floppy_last_error = 9;
    goto cleanup;
  }
  if (FloppyWriteFIFO(2) != 0) {
    g_floppy_last_error = 10;
    goto cleanup;
  }
  if (FloppyWriteFIFO(floppy->sectorsPerTrack) != 0) {
    g_floppy_last_error = 11;
    goto cleanup;
  }
  if (FloppyWriteFIFO(FLOPPY_GAP3_RW) != 0) {
    g_floppy_last_error = 12;
    goto cleanup;
  }
  if (FloppyWriteFIFO(0xFF) != 0) {
    g_floppy_last_error = 13;
    goto cleanup;
  }

  if (FloppyWaitIRQ() != 0) {
    g_floppy_last_error = 14;
    goto cleanup;
  }

  FloppyResult result;
  if (FloppyReadResult(&result) != 0) {
    g_floppy_last_error = 15;
    goto cleanup;
  }
  g_floppy_last_result = (uint32_t)result.st0 |
                         ((uint32_t)result.st1 << 8) |
                         ((uint32_t)result.st2 << 16) |
                         ((uint32_t)result.sector << 24);

  if ((result.st0 & 0xC0) != 0 || result.st1 != 0 || result.st2 != 0 || result.size != 2) {
    g_floppy_last_error = 16;
    goto cleanup;
  }

  if (!isWrite) {
    memcpy(buffer, FLOPPY_DMA_BUFFER, FLOPPY_SECTOR_SIZE);
  }
  status = 0;

cleanup:
  FloppySetDor((uint8_t)drive, false);
  return status;
}

int FloppyReadSector(int drive, uint32_t lba, void *buffer) {
  return FloppyTransferSector(drive, lba, buffer, false);
}

int FloppyWriteSector(int drive, uint32_t lba, const void *buffer) {
  return FloppyTransferSector(drive, lba, (void *)buffer, true);
}

int FloppyGetLastError(void) {
  return g_floppy_last_error;
}

uint32_t FloppyGetLastResult(void) {
  return g_floppy_last_result;
}

int FloppyInit(void) {
  if (FloppyGetList(&g_floppy_drives) != 0) return -1;
  if (!g_floppy_drives.drives[0].exists && !g_floppy_drives.drives[1].exists) {
    return -1;
  }

  if (FloppyResetController() != 0) return -1;
  if (FloppyConfigure(true, false, false, 8) != 0) return -1;
  if (FloppyLock() != 0) return -1;
  if (FloppySpecify() != 0) return -1;

  g_default_drive = -1;
  for (int i = 0; i < 2; ++i) {
    FloppyDrive *drive = &g_floppy_drives.drives[i];
    if (!drive->exists || !drive->supported) {
      continue;
    }
    FloppySelectDataRate(drive->datarate);
    if (FloppyRecalibrate(i) == 0) {
      if (g_default_drive < 0) {
        g_default_drive = i;
      }
    } else {
      drive->supported = false;
    }
  }

  g_floppy_initialized = (g_default_drive >= 0);
  return g_floppy_initialized ? 0 : -1;
}
