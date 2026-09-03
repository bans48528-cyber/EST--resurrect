#include "board_motor_pair_regulator.h"

#define PAIR_SPEED_FILTER_MS 20U
#define PAIR_SPEED_TIMER_HZ 128834U
#define PAIR_SPEED_KP_X100 250
#define PAIR_SPEED_KI_X100 8
#define PAIR_SPEED_MAX_INTEGRATION_MS 50U
#define PAIR_SYNC_COUNTS_PER_PERCENT 2
#define PAIR_SYNC_RATE_GAIN 2
#define PAIR_SYNC_DEADBAND_COUNTS 4
#define PAIR_SYNC_MAX_CORRECTION 100
#define PAIR_SYNC_APPLY_SLEW 2
#define PAIR_SYNC_RELEASE_SLEW 4

static int32_t clamp(int64_t value, int32_t low, int32_t high)
{
	return value < low ? low : (value > high ? high : (int32_t)value);
}

void board_motor_pair_regulator_reset(struct board_motor_pair_regulator *control,
	int32_t count, uint32_t now_ms)
{
	control->last_count = count;
	control->last_ms = now_ms;
	control->speed_x100 = 0;
	control->previous_error_x100 = 0;
}

int32_t board_motor_pair_regulator_step(struct board_motor_pair_regulator *control,
	int32_t count, uint32_t now_ms, uint32_t counts_per_speed,
	int8_t target_speed, int32_t previous_pwm_x100, int32_t limit_x100)
{
	uint32_t elapsed = now_ms - control->last_ms;
	uint32_t integration_ms;
	int32_t sample_speed;
	int32_t error;
	int64_t requested;
	int32_t low;
	int32_t high;

	limit_x100 = clamp(limit_x100, 0, 10000);
	if (elapsed == 0U) {
		return clamp(previous_pwm_x100, -limit_x100, limit_x100);
	}
	sample_speed = clamp(((int64_t)count - control->last_count) *
		counts_per_speed * 100000LL /
		((int64_t)elapsed * PAIR_SPEED_TIMER_HZ), -40000, 40000);
	control->last_count = count;
	control->last_ms = now_ms;
	control->speed_x100 += (int32_t)((int64_t)(sample_speed -
		control->speed_x100) * elapsed / ((int64_t)PAIR_SPEED_FILTER_MS + elapsed));
	error = (int32_t)target_speed * 100 - control->speed_x100;
	integration_ms = elapsed > PAIR_SPEED_MAX_INTEGRATION_MS ?
		PAIR_SPEED_MAX_INTEGRATION_MS : elapsed;
	/* Incremental PI uses the applied, limited PWM as its state: no hidden windup. */
	requested = previous_pwm_x100 +
		(int64_t)(error - control->previous_error_x100) * PAIR_SPEED_KP_X100 / 100 +
		(int64_t)error * PAIR_SPEED_KI_X100 * integration_ms / 1000;
	control->previous_error_x100 = error;
	low = target_speed > 0 ? 0 : -limit_x100;
	high = target_speed < 0 ? 0 : limit_x100;
	if (target_speed == 0 && control->speed_x100 > -100 &&
	    control->speed_x100 < 100) {
		return 0;
	}
	return clamp(requested, low, high);
}

int8_t board_motor_pair_sync_correction(int32_t error, int32_t previous_error,
	int8_t current)
{
	int64_t rate = (int64_t)error - previous_error;
	int32_t requested = clamp((int64_t)error / PAIR_SYNC_COUNTS_PER_PERCENT +
		rate * PAIR_SYNC_RATE_GAIN, -PAIR_SYNC_MAX_CORRECTION, PAIR_SYNC_MAX_CORRECTION);
	int32_t step = PAIR_SYNC_APPLY_SLEW;

	if (error > -PAIR_SYNC_DEADBAND_COUNTS && error < PAIR_SYNC_DEADBAND_COUNTS &&
	    rate >= -1 && rate <= 1) {
		requested = 0;
	}
	if ((current > 0 && requested < 0) || (current < 0 && requested > 0)) {
		requested = 0;
	}
	if ((current > 0 && requested < current) || (current < 0 && requested > current)) {
		step = PAIR_SYNC_RELEASE_SLEW;
	}
	return (int8_t)clamp(requested, current - step, current + step);
}

int8_t board_motor_pair_adjust_speed(int8_t target, int32_t reduction)
{
	int32_t magnitude = target < 0 ? -(int32_t)target : target;

	/* Continuous reduction to zero, never a relay to the other motor's speed. */
	magnitude = clamp((int64_t)magnitude - reduction, 0, magnitude);
	return target < 0 ? -(int8_t)magnitude : (int8_t)magnitude;
}
