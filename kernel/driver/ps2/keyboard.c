#include "keyboard.h"
#include "../../panic.h"

#define PS2_KEYBOARD_QUEUE_SIZE 32

static volatile uint8_t KeyboardQueue[PS2_KEYBOARD_QUEUE_SIZE];
static volatile uint8_t KeyboardQueueHead = 0;
static volatile uint8_t KeyboardQueueTail = 0;
static uint32_t shifted = 0;
static uint32_t ctrl = 0;

/* magic nums oh no */
uint8_t keyboard_map[256] = {
  0x00, '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0x80, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x00, '\\', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/', 0x81, '*', 0x81, ' ',
  0x80, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E,
  0x8F, 0x90, '-', 0x91, '5', 0x92, '+', 0x93, 0x94, 0x95, 0x96, 0x97, '\n',
  0x81, '\\', 0x98, 0x99
};

uint8_t keyboard_map_shifted[256] = {
  0x00, '\e', '!', '"', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0x80, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', '`', 0x00, '|', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?', 0x81, '*', 0x81, ' ',
  0x80, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, '/', '7',
  0x8F, 0x90, '-', 0x91, '5', 0x92, '+', 0x93, 0x94, 0x95, 0x96, 0x97, '\n',
  0x81, '\\', 0x98, 0x99
};

static void PS2KeyboardPushScancode(uint8_t scancode) {
  uint8_t next = (uint8_t)((KeyboardQueueTail + 1) % PS2_KEYBOARD_QUEUE_SIZE);
  if (next == KeyboardQueueHead) {
    return;
  }
  KeyboardQueue[KeyboardQueueTail] = scancode;
  KeyboardQueueTail = next;
}

static bool PS2KeyboardPopScancode(uint8_t *scancode) {
  if (KeyboardQueueHead == KeyboardQueueTail) {
    return false;
  }
  *scancode = KeyboardQueue[KeyboardQueueHead];
  KeyboardQueueHead = (uint8_t)((KeyboardQueueHead + 1) % PS2_KEYBOARD_QUEUE_SIZE);
  return true;
}

void PS2KeyboardHandleIRQ(void) {
  while (true) {
    uint8_t status = ps2_read_status();
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
      break;
    }
    if (status & (1 << 5)) {
      break;
    }
    uint8_t data = ps2_read_data();
    PS2KeyboardPushScancode(data);
  }
}

uint8_t PS2KeyboardGetChar(void) {
  bool hit = false;
  char character = 0;
  while (!hit) {
    character = PS2KeyboardFetch(&hit);
  }
  return character;
}

uint8_t PS2KeyboardFetch(volatile bool *hit) {
  asm("cli");
  if (hit) *hit = false;
  uint8_t scancode = 0;
  if (!PS2KeyboardPopScancode(&scancode)) {
    asm("sti");
    return 0;
  }
  asm("sti");
  if (scancode == 0x2a || scancode == 0x36) {
    shifted = 1;
    return 0;
  } else if (scancode == 0xaa || scancode == 0xb6) {
    shifted = 0;
    return 0;
  }
  if (scancode == 0x1d) {
    ctrl = 1;
    return 0;
  } else if (scancode == 0x9d) {
    ctrl = 0;
    return 0;
  }
  if (scancode & 0x80) return 0;
  if (scancode >= 128) return 0;

  uint8_t key = shifted ? keyboard_map_shifted[scancode] : keyboard_map[scancode];
  if (hit) *hit = true;
  if (ctrl) {
    if (key >= 'a' && key <= 'z') {
      return key - 'a' + 1;
    } else if (key >= 'A' && key <= 'Z')  {
      return key - 'A' + 1;
    }
  }
  return key;
}

void PS2InitializeKeyboard(void) {
  if (!ps2_write_command(PS2_CMD_ENABLE_PORT1)) return;
  for (volatile int i = 0; i < 10000; i++) asm volatile("nop");
  if (!ps2_write_data(0xFF)) return;
  for (volatile int i = 0; i < 100000; i++) asm volatile("nop");
  if (!ps2_wait_output()) return;
  uint8_t ack_response = ps2_read_data();
  if (ack_response != 0xFA) Panic("Keyboard reset ACK failed\r\n");
  if (!ps2_wait_output()) Panic("Keyboard reset completion timed out\r\n");
  uint8_t reset_response = ps2_read_data();
  if (reset_response != 0xAA) Panic("Keyboard reset failed\r\n");
  if (!ps2_write_data(0xF0)) return;
  if (!ps2_write_data(0x02)) return;
  for (volatile int i = 0; i < 10000; i++) asm volatile("nop");
  if (!ps2_write_data(0xF4)) return;
  for (volatile int i = 0; i < 10000; i++) asm volatile("nop");
  if (!ps2_write_data(0xF3)) return;
  if (!ps2_write_data(0x20)) return;
  for (volatile int i = 0; i < 10000; i++) asm volatile("nop");
}

bool PS2KeyboardPresent(void) {
        ps2_drain_output();

        if (!ps2_write_data(0xEE) || !ps2_wait_output())
        {
                return false;
        }

        uint8_t response = ps2_read_data();
        if (response != 0xEE)
        {
                return false;
        }

        if (!ps2_write_data(0xFF) || !ps2_wait_output())
        {
                return false;
        }

        uint8_t ack = ps2_read_data();
        if (ack != 0xFA)
        {
                return false;
        }

        if (!ps2_wait_output())
        {
                return false;
        }

        uint8_t reset_code = ps2_read_data();
        if (reset_code != 0xAA)
        {
                return false;
        }

        return true;
}

int PS2KeyboardInit(void) {
  int present = 0;
  asm("cli");
  if (PS2KeyboardPresent()) {
    PS2InitializeKeyboard();
    present = 1;
  }
  asm("sti");
  return present;
}
