#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

void platform_prepare_from_bootloader(void);
void platform_init(void);
void platform_enable_interrupts(void);
uint32_t platform_millis(void);
void platform_diag_set(uint8_t phase);
void platform_diag_checkpoint(uint8_t code);
void platform_watchdog_kick(void);
void platform_power_off(void) __attribute__((noreturn));

#endif
