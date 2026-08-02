#include "generic.h"

#define PS2_WAIT_LIMIT 100000

uint8_t ps2_read_status(void)
{
        uint8_t status;
        __asm volatile("inb %1, %0" : "=a"(status) : "Nd"(PS2_STATUS_PORT));
        return status;
}

uint8_t ps2_read_data(void)
{
        if (!ps2_wait_output())
        {
                return 0;
        }

        uint8_t data;
        __asm volatile("inb %1, %0" : "=a"(data) : "Nd"(PS2_DATA_PORT));
        return data;
}

bool ps2_write_data(uint8_t data)
{
        if (!ps2_wait_input())
        {
                return false;
        }

        __asm volatile("outb %0, %1" : : "a"(data), "Nd"(PS2_DATA_PORT));
        return true;
}

bool ps2_write_command(uint8_t command)
{
        if (!ps2_wait_input())
        {
                return false;
        }

        __asm volatile("outb %0, %1" : : "a"(command), "Nd"(PS2_COMMAND_PORT));
        return true;
}

bool ps2_wait_output(void)
{
        for (int i = 0; i < PS2_WAIT_LIMIT; ++i)
        {
                if (ps2_read_status() & PS2_STATUS_OUTPUT_FULL)
                {
                        return true;
                }
                __asm volatile("nop");
        }
        return false;
}

bool ps2_wait_input(void)
{
        for (int i = 0; i < PS2_WAIT_LIMIT; ++i)
        {
                if (!(ps2_read_status() & PS2_STATUS_INPUT_FULL))
                {
                        return true;
                }
                __asm volatile("nop");
        }
        return false;
}

void ps2_drain_output(void)
{
        for (int i = 0; i < PS2_WAIT_LIMIT; ++i)
        {
                if (!(ps2_read_status() & PS2_STATUS_OUTPUT_FULL))
                {
                        return;
                }
                (void)ps2_read_data();
        }
}

bool ps2_send_device_command(uint8_t port, uint8_t command)
{
        if (port == 2 && !ps2_write_command(PS2_CMD_WRITE_PORT2))
        {
                return false;
        }

        if (!ps2_write_data(command) || !ps2_wait_output())
        {
                return false;
        }

        uint8_t response = ps2_read_data();
        return (response == PS2_RESP_ACK);
}
