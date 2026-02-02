#ifndef __PIT_H__
#define __PIT_H__
#include <stdint.h>
#include "io.h"
#include "int.h"

#define PIT_CH0       0x40
#define PIT_CH1       0x41
#define PIT_CH2       0x42  
#define PIT_CMD       0x43
#define PIT_INPUT_HZ  1193182


// Command byte: channel 0, lobyte/hibyte, mode 3 (square wave), binary
#define PIT_CMD_CH0   0x00
#define PIT_CMD_LOHI  0x30
#define PIT_CMD_MODE3 0x06

extern uint32_t pit_frequency;

int pit_init(uint32_t frequency);
#endif