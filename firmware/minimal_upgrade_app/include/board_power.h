#ifndef BOARD_POWER_H
#define BOARD_POWER_H

void board_power_init(void);
void board_power_off(void) __attribute__((noreturn));

#endif
