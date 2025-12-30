#include "include/pit.h"

uint32_t pit_frequency;

int pit_init(uint32_t frequency) {
    if (frequency <= 0) {
        return -1;
    }

    uint32_t uint_divisor = PIT_INPUT_HZ / frequency;
    if (uint_divisor < 1)        uint_divisor = 1;          
    if (uint_divisor > 65536)    uint_divisor = 65536;
    uint8_t cmd = PIT_CMD_CH0 | PIT_CMD_LOHI | PIT_CMD_MODE3; // Command byte

    //sending command
    outb(cmd, PIT_CMD);
    io_wait();
    // setting the divisor
    outb(uint_divisor & 0xFF, PIT_CH0);
    outb((uint_divisor >> 8) & 0xFF, PIT_CH0);
    pit_frequency = frequency;
    return 0;
}
