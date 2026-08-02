#ifndef FLOPPY_H
#define FLOPPY_H

#include <stdbool.h>
#include <stdint.h>

/* Constants */
enum {
  FDC_SRA = 0x0,
  FDC_SRB = 0x1,
  FDC_DOR = 0x2,
  FDC_TDR = 0x3,
  FDC_MSR = 0x4, // read-only
  FDC_DSR = 0x4, // write-only
  FDC_FIFO = 0x5,
  FDC_DIR = 0x7, // read-only
  FDC_CCR = 0x7 // write-only
};

typedef enum {
  FDC_CMD_READ_TRACK = 2,
  FDC_CMD_SPECIFY = 3,
  FDC_CMD_SENSE_DRIVE_STATUS = 4,
  FDC_CMD_WRITE_DATA = 5,
  FDC_CMD_READ_DATA = 6,
  FDC_CMD_RECALIBRATE = 7,
  FDC_CMD_SENSE_INTERRUPT = 8,
  FDC_CMD_WRITE_DELETED_DATA = 9,
  FDC_CMD_READ_ID = 10,
  FDC_CMD_READ_DELETED_DATA = 12,
  FDC_CMD_READ_FORMAT_TRACK = 13,
  FDC_CMD_SEEK = 15,
  FDC_CMD_VERSION = 16,
  FDC_CMD_SCAN_EQUAL = 17,
  FDC_CMD_PERPENDICULAR_MODE = 18,
  FDC_CMD_CONFIGURE = 19,
  FDC_CMD_LOCK = 20,
  FDC_CMD_VERIFY = 22,
  FDC_CMD_SCAN_LOW_OR_EQUAL = 25,
  FDC_CMD_SCAN_HIGH_OR_EQUAL = 29,
} FloppyCommand;

struct _FloppyDrive {
  unsigned char sectorsPerTrack;
  unsigned char tracks;
  unsigned char heads;
  unsigned short motorDelay;
  unsigned char cmosType;
  unsigned char datarate; // 0 = 500Kbps, 3 = 1Mbps (2.88M)
  bool exists;
  bool supported;
} __attribute__((__packed__));

typedef struct _FloppyDrive FloppyDrive;

typedef struct {
    FloppyDrive drives[2]; // In PC Standard 2 Floppy disk will be enogh
} FloppyDriveList;

// from floppy.asm
int FloppyReadRegister (int registerNum, unsigned char * result);
int FloppyWriteRegister (int registerNum, int byte);
int FloppyReadCMOS(unsigned char *result);

void FloppyHandleIRQ(void);
int FloppyInit(void);
int FloppyReadSector(int drive, uint32_t lba, void *buffer);
int FloppyWriteSector(int drive, uint32_t lba, const void *buffer);
int FloppyGetLastError(void);
uint32_t FloppyGetLastResult(void);

#endif
