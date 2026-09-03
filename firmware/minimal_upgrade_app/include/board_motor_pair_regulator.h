#ifndef BOARD_MOTOR_PAIR_REGULATOR_H
#define BOARD_MOTOR_PAIR_REGULATOR_H

#include <stdint.h>

struct board_motor_pair_regulator {
	int32_t last_count;
	uint32_t last_ms;
	int32_t speed_x100;
	int32_t previous_error_x100;
};

void board_motor_pair_regulator_reset(struct board_motor_pair_regulator *control,
	int32_t count, uint32_t now_ms);
int32_t board_motor_pair_regulator_step(struct board_motor_pair_regulator *control,
	int32_t count, uint32_t now_ms, uint32_t counts_per_speed,
	int8_t target_speed, int32_t previous_pwm_x100, int32_t limit_x100);
int8_t board_motor_pair_sync_correction(int32_t error, int32_t previous_error,
	int8_t current);
int8_t board_motor_pair_adjust_speed(int8_t target, int32_t reduction);

#endif
