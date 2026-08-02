#include "mouse.h"

static volatile uint8_t MousePacketData[4];
static volatile uint8_t MousePacketIndex = 0;
static volatile uint8_t MouseButtons = 0;
static volatile int MouseDeltaX = 0;
static volatile int MouseDeltaY = 0;
static volatile bool MousePacketReady = false;
static volatile bool MousePresentFlag = false;

static bool PS2MouseWriteConfig(uint8_t config) {
  if (!ps2_write_command(PS2_CMD_WRITE_CONFIG)) {
    return false;
  }
  return ps2_write_data(config);
}

static bool PS2MouseReadConfig(uint8_t *config) {
  if (!ps2_write_command(PS2_CMD_READ_CONFIG) || !ps2_wait_output()) {
    return false;
  }
  *config = ps2_read_data();
  return true;
}

static bool PS2MouseReadResetSequence(void) {
  uint8_t response = 0;

  if (!ps2_wait_output()) {
    return false;
  }
  response = ps2_read_data();
  if (response != PS2_RESP_ACK) {
    return false;
  }

  if (!ps2_wait_output()) {
    return false;
  }
  response = ps2_read_data();
  if (response != PS2_RESP_SELF_TEST_PASS) {
    return false;
  }

  if (ps2_wait_output()) {
    (void)ps2_read_data();
  }

  return true;
}

static void PS2MousePushByte(uint8_t data) {
  if (MousePacketIndex == 0 && !(data & (1 << 3))) {
    return;
  }

  MousePacketData[MousePacketIndex++] = data;
  if (MousePacketIndex < 3) {
    return;
  }

  MousePacketIndex = 0;
  MousePacketReady = true;

  if (!(MousePacketData[0] & (1 << 6))) {
    MouseDeltaX += (int8_t)MousePacketData[1];
  }

  if (!(MousePacketData[0] & (1 << 7))) {
    MouseDeltaY -= (int8_t)MousePacketData[2];
  }

  MouseButtons = 0;
  if (MousePacketData[0] & (1 << 0)) {
    MouseButtons |= MOUSE_LEFT_BUTTON;
  }
  if (MousePacketData[0] & (1 << 1)) {
    MouseButtons |= MOUSE_RIGHT_BUTTON;
  }
  if (MousePacketData[0] & (1 << 2)) {
    MouseButtons |= MOUSE_MIDDLE_BUTTON;
  }
}

void PS2MouseHandleIRQ(void) {
  while (true) {
    uint8_t status = ps2_read_status();
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
      break;
    }
    if (!(status & PS2_STATUS_AUX_OUTPUT_FULL)) {
      break;
    }

    uint8_t data = ps2_read_data();
    if (status & (PS2_STATUS_TIMEOUT_ERROR | PS2_STATUS_PARITY_ERROR)) {
      MousePacketIndex = 0;
      continue;
    }

    PS2MousePushByte(data);
  }
}

void PS2MouseFetch(int *mx, int *my, uint8_t *buttons) {
  asm volatile("cli");
  if (mx) {
    *mx += MouseDeltaX;
    MouseDeltaX = 0;
  }
  if (my) {
    *my += MouseDeltaY;
    MouseDeltaY = 0;
  }
  if (buttons) {
    *buttons = MouseButtons;
  }
  asm volatile("sti");
}

bool ps2_read_mouse_packet(ps2_mouse_packet_t *packet) {
  if (!packet) {
    return false;
  }

  asm volatile("cli");
  if (!MousePacketReady) {
    asm volatile("sti");
    return false;
  }

  packet->flags = MousePacketData[0];
  packet->x_movement = (int8_t)MousePacketData[1];
  packet->y_movement = (int8_t)MousePacketData[2];
  packet->z_movement = 0;
  packet->buttons = MouseButtons;
  MousePacketReady = false;
  asm volatile("sti");
  return true;
}

bool PS2MousePresent(void) {
  uint8_t config = 0;

  ps2_drain_output();

  if (!ps2_write_command(PS2_CMD_ENABLE_PORT2)) {
    return false;
  }

  for (volatile int i = 0; i < 10000; ++i) {
    asm volatile("nop");
  }

  if (!PS2MouseReadConfig(&config)) {
    return false;
  }
  config &= (uint8_t)~PS2_CONFIG_PORT2_CLOCK_DISABLE;
  config |= PS2_CONFIG_PORT2_IRQ;
  if (!PS2MouseWriteConfig(config)) {
    return false;
  }

  if (!ps2_write_command(PS2_CMD_TEST_PORT2) || !ps2_wait_output()) {
    return false;
  }
  if (ps2_read_data() != 0x00) {
    return false;
  }

  if (!ps2_send_device_command(2, PS2_DEV_IDENTIFY) || !ps2_wait_output()) {
    return false;
  }

  uint8_t id = ps2_read_data();
  if (ps2_wait_output()) {
    (void)ps2_read_data();
  }

  return id == MOUSE_STANDARD || id == MOUSE_HAS_SCROLL || id == MOUSE_HAS_5_BUTTONS;
}

void ps2_initialize_mouse(void) {
  uint8_t config = 0;

  ps2_drain_output();
  MousePacketIndex = 0;
  MousePacketReady = false;
  MouseButtons = 0;
  MouseDeltaX = 0;
  MouseDeltaY = 0;

  if (!ps2_write_command(PS2_CMD_ENABLE_PORT2)) {
    return;
  }

  if (!PS2MouseReadConfig(&config)) {
    return;
  }
  config &= (uint8_t)~PS2_CONFIG_PORT2_CLOCK_DISABLE;
  config |= PS2_CONFIG_PORT2_IRQ;
  if (!PS2MouseWriteConfig(config)) {
    return;
  }

  if (!ps2_write_command(PS2_CMD_WRITE_PORT2) || !ps2_write_data(PS2_DEV_RESET)) {
    return;
  }
  if (!PS2MouseReadResetSequence()) {
    return;
  }

  if (!ps2_send_device_command(2, 0xF3)) {
    return;
  }
  if (!ps2_send_device_command(2, 100)) {
    return;
  }
  if (!ps2_send_device_command(2, PS2_DEV_ENABLE_SCAN)) {
    return;
  }

  MousePresentFlag = true;
}

bool PS2MouseReady(void) {
  return MousePresentFlag;
}

int PS2MouseInit(void) {
  int present = 0;

  asm volatile("cli");
  MousePresentFlag = false;
  if (PS2MousePresent()) {
    ps2_initialize_mouse();
    present = MousePresentFlag ? 1 : 0;
  }
  asm volatile("sti");

  return present;
}
