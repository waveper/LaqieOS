#ifndef PS2_GENERIC_H
#define PS2_GENERIC_H

#include <stdint.h>
#include <stdbool.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64
#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL (1 << 1)
#define PS2_STATUS_SYSTEM_FLAG (1 << 2)
#define PS2_STATUS_COMMAND_DATA (1 << 3)
#define PS2_STATUS_TIMEOUT_ERROR (1 << 6)
#define PS2_STATUS_PARITY_ERROR (1 << 7)
#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT2 0xA8
#define PS2_CMD_TEST_PORT2 0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_PORT1 0xAB
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_ENABLE_PORT1 0xAE
#define PS2_CMD_WRITE_PORT2 0xD4
#define PS2_DEV_IDENTIFY 0xF2
#define PS2_DEV_ENABLE_SCAN 0xF4
#define PS2_DEV_DISABLE_SCAN 0xF5
#define PS2_DEV_RESET 0xFF
#define PS2_RESP_ACK 0xFA
#define PS2_RESP_SELF_TEST_PASS 0xAA
#define PS2_RESP_ECHO 0xEE
#define PS2_RESP_RESEND 0xFE

uint8_t ps2_read_status(void);
uint8_t ps2_read_data(void);
bool    ps2_write_data(uint8_t data);
bool    ps2_write_command(uint8_t command);
bool    ps2_send_device_command(uint8_t port, uint8_t command);
bool    ps2_wait_output(void);
bool    ps2_wait_input(void);
void    ps2_drain_output(void);

#endif
