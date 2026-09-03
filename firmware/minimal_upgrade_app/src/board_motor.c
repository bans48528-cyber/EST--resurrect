#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "board_motor.h"
#include "board_motor_pair_regulator.h"

/* V5 schematic and legacy EST app: outputs A/B/C/D share TIM4. */
#define MOTOR_A_PWM_PORT GPIOB
#define MOTOR_A_PWM_PIN GPIO9
#define MOTOR_A_DIRECTION_PORT GPIOG
#define MOTOR_A_DIRECTION_0_PIN GPIO10
#define MOTOR_A_DIRECTION_1_PIN GPIO11
#define MOTOR_A_TACHO_PORT GPIOE
#define MOTOR_A_TACHO_PHASE_PIN GPIO5
#define MOTOR_A_TACHO_DIRECTION_PIN GPIO6
#define MOTOR_A_ID_ADC_PORT GPIOC
#define MOTOR_A_ID_ADC_PIN GPIO4
#define MOTOR_A_ID_ADC ADC1
#define MOTOR_A_ID_ADC_CHANNEL ADC_CHANNEL14

#define MOTOR_B_PWM_PORT GPIOB
#define MOTOR_B_PWM_PIN GPIO8
#define MOTOR_B_DIRECTION_PORT GPIOD
#define MOTOR_B_DIRECTION_0_PIN GPIO0
#define MOTOR_B_DIRECTION_1_PIN GPIO1
#define MOTOR_B_TACHO_PORT GPIOE
#define MOTOR_B_TACHO_PHASE_PIN GPIO13
#define MOTOR_B_TACHO_DIRECTION_PIN GPIO14
#define MOTOR_B_ID_ADC_PORT GPIOC
#define MOTOR_B_ID_ADC_PIN GPIO5
#define MOTOR_B_ID_ADC ADC1
#define MOTOR_B_ID_ADC_CHANNEL ADC_CHANNEL15

#define MOTOR_C_PWM_PORT GPIOD
#define MOTOR_C_PWM_PIN GPIO13
#define MOTOR_C_DIRECTION_PORT GPIOG
#define MOTOR_C_DIRECTION_0_PIN GPIO12
#define MOTOR_C_DIRECTION_1_PIN GPIO13
#define MOTOR_C_TACHO_PORT GPIOC
#define MOTOR_C_TACHO_PHASE_PIN GPIO7
#define MOTOR_C_TACHO_DIRECTION_PIN GPIO6
#define MOTOR_C_ID_ADC_PORT GPIOF
#define MOTOR_C_ID_ADC_PIN GPIO8
#define MOTOR_C_ID_ADC ADC3
#define MOTOR_C_ID_ADC_CHANNEL ADC_CHANNEL6

#define MOTOR_D_PWM_PORT GPIOD
#define MOTOR_D_PWM_PIN GPIO12
#define MOTOR_D_DIRECTION_PORT GPIOA
#define MOTOR_D_DIRECTION_0_PIN GPIO10
#define MOTOR_D_DIRECTION_1_PIN GPIO9
#define MOTOR_D_TACHO_PORT GPIOC
#define MOTOR_D_TACHO_PHASE_PIN GPIO9
#define MOTOR_D_TACHO_DIRECTION_PIN GPIO8
#define MOTOR_D_ID_ADC_PORT GPIOF
#define MOTOR_D_ID_ADC_PIN GPIO9
#define MOTOR_D_ID_ADC ADC3
#define MOTOR_D_ID_ADC_CHANNEL ADC_CHANNEL7

#define MOTOR_PWM_PERIOD 99U
#define MOTOR_PWM_OFF_COMPARE 100U
#define MOTOR_TEST_POWER_PERCENT 30U
#define MOTOR_TEST_RUN_MS 700U
#define MOTOR_TEST_PAUSE_MS 400U
#define MOTOR_HIGH_POWER_THRESHOLD_PERCENT 80U
#define MOTOR_HIGH_POWER_RUN_MS 500U
#define MOTOR_HIGH_POWER_PAUSE_MS 800U
#define MOTOR_STOP_TEST_RUN_MS 600U
#define MOTOR_STOP_TEST_MEASURE_MS 1500U
#define MOTOR_DUAL_TEST_RUN_MS 700U
#define MOTOR_DUAL_TEST_BRAKE_MS 400U
#define MOTOR_ID_ADC_AVERAGE_SAMPLES 10U
#define MOTOR_ID_REFRESH_PHASE_MS 20U
#define MOTOR_ID_AUTO_SCAN_INTERVAL_MS 100U
#define MOTOR_ID_SCALE_MV 5000U
#define MOTOR_ID_ADC_FULL_SCALE 4096U
#define MOTOR_ID_IMPEDANCE_PHASE_MAX_MV 300U
#define MOTOR_ID_PULLUP_LARGE_LOW_MV 250U
#define MOTOR_ID_PULLUP_LARGE_HIGH_MV 360U
#define MOTOR_ID_PULLUP_MEDIUM_LOW_MV 380U
#define MOTOR_ID_PULLUP_MEDIUM_HIGH_MV 500U
#define MOTOR_ID_PULLUP_NONE_LOW_MV 520U
#define MOTOR_ID_PULLUP_NONE_HIGH_MV 1000U
#define MOTOR_SPEED_CONTROL_INTERVAL_MS 10U
#define MOTOR_SPEED_SAMPLE_TIMEOUT_MS 150U
#define MOTOR_SPEED_MIN_PERCENT 1
#define MOTOR_SPEED_TIMER_HZ 128834U
#define MOTOR_LARGE_COUNTS_PER_SPEED 12800U
#define MOTOR_MEDIUM_COUNTS_PER_SPEED 8100U
#define MOTOR_POSITION_MIN_SPEED_PERCENT 1U
#define MOTOR_POSITION_MAX_DEGREES 3600
#define MOTOR_POSITION_BRAKE_MS 300U
#define MOTOR_POSITION_STALL_TRACKING_PWM_X100 9500
#define MOTOR_POSITION_STALL_LARGE_SPEED_PERCENT 2
#define MOTOR_POSITION_STALL_MEDIUM_SPEED_PERCENT 3
#define MOTOR_POSITION_STALL_LARGE_MS 1000U
#define MOTOR_POSITION_STALL_MEDIUM_MS 700U
#define MOTOR_POSITION_SETTLING_LARGE_KP_X100 70
#define MOTOR_POSITION_SETTLING_LARGE_KD_X100 160
#define MOTOR_POSITION_SETTLING_MEDIUM_KP_X100 100
#define MOTOR_POSITION_SETTLING_MEDIUM_KD_X100 220
#define MOTOR_POSITION_SETTLING_LARGE_MAX_PWM_X100 2000
#define MOTOR_POSITION_SETTLING_MEDIUM_MAX_PWM_X100 3000
#define MOTOR_POSITION_SETTLING_LARGE_REVERSE_MAX_PWM_X100 800
#define MOTOR_POSITION_SETTLING_MEDIUM_REVERSE_MAX_PWM_X100 2000
#define MOTOR_POSITION_SETTLING_LARGE_PWM_SLEW_X100 400
#define MOTOR_POSITION_SETTLING_MEDIUM_PWM_SLEW_X100 500
#define MOTOR_POSITION_SETTLING_MEDIUM_RELEASE_SLEW_X100 3500
#define MOTOR_POSITION_SETTLING_LARGE_MIN_RECOVERY_PWM_X100 700
#define MOTOR_POSITION_SETTLING_MEDIUM_MIN_RECOVERY_PWM_X100 3000
#define MOTOR_POSITION_SETTLING_MEDIUM_STRONG_RECOVERY_PWM_X100 3500
#define MOTOR_POSITION_SETTLING_MEDIUM_STRONG_RECOVERY_THRESHOLD_COUNTS 8
#define MOTOR_POSITION_SETTLING_LARGE_RECOVERY_THRESHOLD_COUNTS 3
#define MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_THRESHOLD_COUNTS 6
#define MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_DRIVE_SAMPLES 3U
#define MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_COOLDOWN_SAMPLES 4U
#define MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_RAMP_X100 1750
#define MOTOR_POSITION_SETTLING_LARGE_POSITION_TOLERANCE_COUNTS 2
#define MOTOR_POSITION_SETTLING_MEDIUM_POSITION_TOLERANCE_COUNTS 5
#define MOTOR_POSITION_SETTLING_LARGE_SPEED_TOLERANCE_PERCENT 2
#define MOTOR_POSITION_SETTLING_MEDIUM_SPEED_TOLERANCE_PERCENT 2
#define MOTOR_POSITION_SETTLING_RECOVERY_VELOCITY_COUNTS 1
#define MOTOR_POSITION_SETTLING_VELOCITY_TOLERANCE_COUNTS 0
#define MOTOR_POSITION_SETTLING_STABLE_SAMPLES 8U
#define MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS 20
#define MOTOR_PAIR_SYNC_OFFSET_RELEASE_SAMPLES 5U
#define MOTOR_PAIR_STALL_PWM_X100 7500
#define MOTOR_PAIR_STALL_ENTER_SAMPLES 20U
#define MOTOR_PAIR_RECOVERY_HOLDOFF_SAMPLES 30U
#define MOTOR_PAIR_RECOVERY_LARGE_PWM_MARGIN_X100 1500
#define MOTOR_PAIR_RECOVERY_MEDIUM_PWM_MARGIN_X100 2500
#define MOTOR_PAIR_RECOVERY_PEER_PWM_MARGIN_X100 500
#define MOTOR_TIMED_MAX_DURATION_MS 600000U
#define MOTOR_PAIR_TIMED_MAX_DURATION_MS 600000U
#define MOTOR_HOLD_LARGE_KP_X100 70
#define MOTOR_HOLD_LARGE_KD_X100 110
#define MOTOR_HOLD_LARGE_MAX_PWM_X100 3500
#define MOTOR_HOLD_LARGE_ENTER_DEADBAND_COUNTS 2
#define MOTOR_HOLD_LARGE_EXIT_DEADBAND_COUNTS 5
#define MOTOR_HOLD_MEDIUM_KP_X100 100
#define MOTOR_HOLD_MEDIUM_KD_X100 180
#define MOTOR_HOLD_MEDIUM_MAX_PWM_X100 4000
#define MOTOR_HOLD_MEDIUM_MIN_RECOVERY_PWM_X100 1500
#define MOTOR_HOLD_MEDIUM_RECOVERY_THRESHOLD_COUNTS 12
#define MOTOR_HOLD_MEDIUM_ENTER_DEADBAND_COUNTS 3
#define MOTOR_HOLD_MEDIUM_EXIT_DEADBAND_COUNTS 6
#define MOTOR_HOLD_SETTLED_SPEED_COUNTS 1
#define MOTOR_HOLD_LARGE_PWM_SLEW_X100 250
#define MOTOR_HOLD_MEDIUM_PWM_SLEW_X100 350
#define MOTOR_HOLD_DISCONNECT_CHECK_MS 20U
#define MOTOR_HOLD_DISCONNECT_SAMPLES 3U

struct motor_port_config {
	uint32_t pwm_port;
	uint16_t pwm_pin;
	enum tim_oc_id pwm_channel;
	uint32_t direction_port;
	uint16_t direction_0_pin;
	uint16_t direction_1_pin;
	uint32_t tacho_port;
	uint16_t tacho_phase_pin;
	uint16_t tacho_direction_pin;
	uint32_t id_adc_port;
	uint16_t id_adc_pin;
	uint32_t id_adc;
	uint8_t id_adc_channel;
};

enum motor_identification_refresh_state {
	MOTOR_IDENTIFICATION_REFRESH_IDLE = 0,
	MOTOR_IDENTIFICATION_REFRESH_FLOAT = 1,
	MOTOR_IDENTIFICATION_REFRESH_DRIVE_LOW = 2,
	MOTOR_IDENTIFICATION_REFRESH_PIN5_PULLUP = 3
};

enum motor_position_control_phase {
	MOTOR_POSITION_PHASE_TRACKING = 0,
	MOTOR_POSITION_PHASE_SETTLING = 1
};

static const struct motor_port_config motor_ports[BOARD_MOTOR_PORT_COUNT] = {
	[BOARD_MOTOR_PORT_A] = {
		MOTOR_A_PWM_PORT, MOTOR_A_PWM_PIN, TIM_OC4,
		MOTOR_A_DIRECTION_PORT, MOTOR_A_DIRECTION_0_PIN,
		MOTOR_A_DIRECTION_1_PIN, MOTOR_A_TACHO_PORT,
		MOTOR_A_TACHO_PHASE_PIN, MOTOR_A_TACHO_DIRECTION_PIN,
		MOTOR_A_ID_ADC_PORT, MOTOR_A_ID_ADC_PIN,
		MOTOR_A_ID_ADC, MOTOR_A_ID_ADC_CHANNEL
	},
	[BOARD_MOTOR_PORT_B] = {
		MOTOR_B_PWM_PORT, MOTOR_B_PWM_PIN, TIM_OC3,
		MOTOR_B_DIRECTION_PORT, MOTOR_B_DIRECTION_0_PIN,
		MOTOR_B_DIRECTION_1_PIN, MOTOR_B_TACHO_PORT,
		MOTOR_B_TACHO_PHASE_PIN, MOTOR_B_TACHO_DIRECTION_PIN,
		MOTOR_B_ID_ADC_PORT, MOTOR_B_ID_ADC_PIN,
		MOTOR_B_ID_ADC, MOTOR_B_ID_ADC_CHANNEL
	},
	[BOARD_MOTOR_PORT_C] = {
		MOTOR_C_PWM_PORT, MOTOR_C_PWM_PIN, TIM_OC2,
		MOTOR_C_DIRECTION_PORT, MOTOR_C_DIRECTION_0_PIN,
		MOTOR_C_DIRECTION_1_PIN, MOTOR_C_TACHO_PORT,
		MOTOR_C_TACHO_PHASE_PIN, MOTOR_C_TACHO_DIRECTION_PIN,
		MOTOR_C_ID_ADC_PORT, MOTOR_C_ID_ADC_PIN,
		MOTOR_C_ID_ADC, MOTOR_C_ID_ADC_CHANNEL
	},
	[BOARD_MOTOR_PORT_D] = {
		MOTOR_D_PWM_PORT, MOTOR_D_PWM_PIN, TIM_OC1,
		MOTOR_D_DIRECTION_PORT, MOTOR_D_DIRECTION_0_PIN,
		MOTOR_D_DIRECTION_1_PIN, MOTOR_D_TACHO_PORT,
		MOTOR_D_TACHO_PHASE_PIN, MOTOR_D_TACHO_DIRECTION_PIN,
		MOTOR_D_ID_ADC_PORT, MOTOR_D_ID_ADC_PIN,
		MOTOR_D_ID_ADC, MOTOR_D_ID_ADC_CHANNEL
	}
};

static enum board_motor_test_state test_state;
static enum board_motor_port test_port;
static uint32_t phase_started_ms;
static volatile int32_t tacho_count[BOARD_MOTOR_PORT_COUNT];
static enum board_motor_output_state output_state[BOARD_MOTOR_PORT_COUNT];
static int8_t output_power[BOARD_MOTOR_PORT_COUNT];
static int32_t forward_count;
static int32_t reverse_count;
static int32_t reverse_started_count;
static uint32_t test_run_ms;
static uint32_t test_pause_ms;
static uint32_t test_compare;
static enum board_motor_stop_test_state stop_test_state;
static enum board_motor_stop_mode stop_test_mode;
static int32_t stop_test_powered_count;
static int32_t stop_test_started_count;
static int32_t stop_test_stopped_count;
static enum board_motor_dual_test_state dual_test_state;
static int32_t dual_a_forward_count;
static int32_t dual_b_forward_count;
static int32_t dual_a_reverse_count;
static int32_t dual_b_reverse_count;
static int32_t dual_a_reverse_started_count;
static int32_t dual_b_reverse_started_count;
static enum board_motor_type motor_type[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_adc_raw[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_mv[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_pin6_low_adc_raw[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_pin6_low_mv[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_pin5_pullup_adc_raw[BOARD_MOTOR_PORT_COUNT];
static uint16_t motor_id_pin5_pullup_mv[BOARD_MOTOR_PORT_COUNT];
static uint8_t motor_id_pin5_pullup_high[BOARD_MOTOR_PORT_COUNT];
static uint32_t last_automatic_identification_ms;
static uint8_t automatic_identification_next_port;
static enum motor_identification_refresh_state identification_refresh_state;
static bool identification_refresh_automatic;
static enum board_motor_port identification_refresh_port;
static uint32_t identification_refresh_started_ms;
static uint16_t identification_refresh_float_raw;
static uint16_t identification_refresh_float_mv;
static int32_t identification_refresh_tacho_count;
static int8_t measured_speed_percent[BOARD_MOTOR_PORT_COUNT];
static int32_t speed_sample_count[BOARD_MOTOR_PORT_COUNT];
static uint32_t speed_sample_ms[BOARD_MOTOR_PORT_COUNT];
static bool speed_measurement_valid[BOARD_MOTOR_PORT_COUNT];
static struct board_motor_pair_regulator pair_regulators[BOARD_MOTOR_PORT_COUNT];

struct motor_position_control {
	enum board_motor_position_state state;
	enum motor_position_control_phase phase;
	enum board_motor_type type;
	int8_t requested_speed;
	int32_t start_count;
	int32_t target_count;
	uint32_t finished_ms;
	uint32_t last_control_ms;
	int32_t last_count;
	int32_t pwm_x100;
	uint8_t stable_samples;
	uint8_t recovery_drive_samples;
	uint8_t recovery_cooldown_samples;
	int8_t recovery_direction;
	int16_t recovery_pwm_x100;
	uint32_t stall_candidate_ms;
	bool stall_candidate;
	bool stalled;
	uint8_t pair_recovery_samples;
	int16_t pair_recovery_pwm_limit_x100;
	enum board_motor_stop_mode completion_stop_mode;
};

struct motor_speed_control {
	enum board_motor_speed_state state;
	enum board_motor_type type;
	int8_t requested_speed;
	uint32_t started_ms;
	uint32_t duration_ms;
	uint32_t elapsed_ms;
	enum board_motor_stop_mode completion_stop_mode;
	uint32_t last_control_ms;
	int32_t pwm_x100;
	int32_t pair_last_count;
	int16_t pair_tick_progress;
	uint8_t pair_stall_samples;
	uint8_t pair_recovery_samples;
	int16_t pair_recovery_pwm_limit_x100;
	bool pair_stalled;
};

struct motor_hold_control {
	enum board_motor_hold_state state;
	enum board_motor_type type;
	int32_t target_count;
	int32_t last_count;
	uint32_t last_control_ms;
	uint32_t last_disconnect_check_ms;
	int32_t pwm_x100;
	uint8_t disconnect_samples;
	bool settled;
};

struct motor_pair_position_control {
	enum board_motor_pair_position_state state;
	enum board_motor_port left_port;
	enum board_motor_port right_port;
	int32_t synchronization_error;
	int32_t maximum_synchronization_error;
	int32_t synchronization_offset;
	int32_t previous_control_error;
	int8_t correction_percent;
	enum board_motor_port leader_port;
	uint32_t last_sync_ms;
	uint8_t offset_release_samples;
};

struct motor_pair_speed_control {
	enum board_motor_pair_speed_state state;
	enum board_motor_port left_port;
	enum board_motor_port right_port;
	int32_t left_start_count;
	int32_t right_start_count;
	int32_t synchronization_error;
	int32_t maximum_synchronization_error;
	int32_t synchronization_offset;
	int32_t previous_control_error;
	int8_t correction_percent;
	enum board_motor_port leader_port;
	int8_t left_requested_speed;
	int8_t right_requested_speed;
	uint32_t started_ms;
	uint32_t duration_ms;
	uint32_t elapsed_ms;
	uint32_t last_sync_ms;
	uint8_t offset_release_samples;
	enum board_motor_stop_mode completion_stop_mode;
};

static struct motor_position_control
	position_controls[BOARD_MOTOR_PORT_COUNT];
static struct motor_speed_control speed_controls[BOARD_MOTOR_PORT_COUNT];
static struct motor_hold_control hold_controls[BOARD_MOTOR_PORT_COUNT];
static struct motor_pair_position_control pair_position_control;
static struct motor_pair_speed_control pair_speed_control;
static enum board_motor_port last_position_port;
static enum board_motor_port last_speed_port;

static const struct motor_port_config *motor_config(enum board_motor_port port);
static void motor_output_off(enum board_motor_port port);
static void motor_output_high_push_pull_stop(enum board_motor_port port);
static bool normal_test_active(void);
static bool stop_test_active(void);
static bool dual_test_active(void);
static bool speed_control_active(enum board_motor_port port);
static bool position_control_active(enum board_motor_port port);
static bool hold_control_active(enum board_motor_port port);
static bool any_speed_control_active(void);
static bool any_position_control_active(void);
static bool any_hold_control_active(void);
static bool legacy_diagnostic_active(void);
static bool pair_position_owns_port(enum board_motor_port port);
static bool pair_speed_owns_port(enum board_motor_port port);
static void record_pair_speed_error(void);

static void motor_tacho_direction_drive_low(enum board_motor_port port)
{
	const struct motor_port_config *config = motor_config(port);

	gpio_clear(config->tacho_port, config->tacho_direction_pin);
	gpio_mode_setup(config->tacho_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		config->tacho_direction_pin);
	gpio_set_output_options(config->tacho_port, GPIO_OTYPE_PP,
		GPIO_OSPEED_2MHZ, config->tacho_direction_pin);
}

static void motor_tacho_direction_float(enum board_motor_port port)
{
	const struct motor_port_config *config = motor_config(port);

	gpio_mode_setup(config->tacho_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		config->tacho_direction_pin);
}

static void motor_tacho_phase_pullup(enum board_motor_port port, bool enabled)
{
	const struct motor_port_config *config = motor_config(port);

	gpio_mode_setup(config->tacho_port, GPIO_MODE_INPUT,
		enabled ? GPIO_PUPD_PULLUP : GPIO_PUPD_NONE,
		config->tacho_phase_pin);
}

static bool motor_port_valid(enum board_motor_port port)
{
	return (uint8_t)port < BOARD_MOTOR_PORT_COUNT;
}

static uint16_t read_motor_id_adc(const struct motor_port_config *config)
{
	uint8_t sequence[1] = {config->id_adc_channel};
	uint32_t sum = 0U;
	uint8_t sample;

	adc_set_sample_time(config->id_adc, config->id_adc_channel,
		ADC_SMPR_SMP_144CYC);
	adc_set_regular_sequence(config->id_adc, 1U, sequence);
	for (sample = 0U; sample <= MOTOR_ID_ADC_AVERAGE_SAMPLES; sample++) {
		adc_start_conversion_regular(config->id_adc);
		while (!adc_eoc(config->id_adc)) {
			/* The conversion completes in far less than one millisecond. */
		}
		if (sample != 0U) {
			sum += adc_read_regular(config->id_adc);
		} else {
			(void)adc_read_regular(config->id_adc);
		}
	}
	return (uint16_t)(sum / MOTOR_ID_ADC_AVERAGE_SAMPLES);
}

static enum board_motor_type motor_type_from_mv(uint16_t millivolts)
{
	if ((millivolts > 300U && millivolts < 380U) ||
	    (millivolts > 2400U && millivolts < 3000U)) {
		return BOARD_MOTOR_TYPE_LARGE;
	}
	if ((millivolts > 400U && millivolts < 480U) ||
	    (millivolts > 1550U && millivolts < 2150U)) {
		return BOARD_MOTOR_TYPE_MEDIUM;
	}
	if (millivolts > 500U && millivolts < 1000U) {
		return BOARD_MOTOR_TYPE_NONE;
	}
	return BOARD_MOTOR_TYPE_UNKNOWN;
}

static bool motor_type_is_valid(enum board_motor_type type)
{
	return type == BOARD_MOTOR_TYPE_LARGE ||
		type == BOARD_MOTOR_TYPE_MEDIUM ||
		type == BOARD_MOTOR_TYPE_NONE;
}

static enum board_motor_type motor_type_from_probe(uint16_t float_mv,
	uint16_t low_mv, uint16_t pullup_mv)
{
	enum board_motor_type float_type = motor_type_from_mv(float_mv);
	enum board_motor_type low_type = motor_type_from_mv(low_mv);

	if (float_type == low_type && motor_type_is_valid(float_type)) {
		return float_type;
	}
	if ((float_type == BOARD_MOTOR_TYPE_LARGE ||
	     float_type == BOARD_MOTOR_TYPE_MEDIUM) &&
	    low_type == BOARD_MOTOR_TYPE_UNKNOWN) {
		return float_type;
	}
	if ((low_type == BOARD_MOTOR_TYPE_LARGE ||
	     low_type == BOARD_MOTOR_TYPE_MEDIUM) &&
	    float_type == BOARD_MOTOR_TYPE_UNKNOWN) {
		return low_type;
	}
	if (float_type == BOARD_MOTOR_TYPE_NONE &&
	    low_type == BOARD_MOTOR_TYPE_UNKNOWN) {
		return BOARD_MOTOR_TYPE_NONE;
	}
	if (low_type == BOARD_MOTOR_TYPE_NONE &&
	    float_type == BOARD_MOTOR_TYPE_UNKNOWN) {
		return BOARD_MOTOR_TYPE_NONE;
	}
	if (float_mv < MOTOR_ID_IMPEDANCE_PHASE_MAX_MV &&
	    low_mv < MOTOR_ID_IMPEDANCE_PHASE_MAX_MV) {
		if (pullup_mv > MOTOR_ID_PULLUP_LARGE_LOW_MV &&
		    pullup_mv < MOTOR_ID_PULLUP_LARGE_HIGH_MV) {
			return BOARD_MOTOR_TYPE_LARGE;
		}
		if (pullup_mv > MOTOR_ID_PULLUP_MEDIUM_LOW_MV &&
		    pullup_mv < MOTOR_ID_PULLUP_MEDIUM_HIGH_MV) {
			return BOARD_MOTOR_TYPE_MEDIUM;
		}
		if (pullup_mv > MOTOR_ID_PULLUP_NONE_LOW_MV &&
		    pullup_mv < MOTOR_ID_PULLUP_NONE_HIGH_MV) {
			return BOARD_MOTOR_TYPE_NONE;
		}
	}
	return BOARD_MOTOR_TYPE_UNKNOWN;
}

static uint8_t motor_speed_sample_count(enum board_motor_type type,
	uint8_t speed_percent)
{
	static const uint8_t medium_samples[4] = {2U, 4U, 8U, 16U};
	static const uint8_t large_samples[4] = {4U, 16U, 32U, 64U};
	const uint8_t *samples = type == BOARD_MOTOR_TYPE_MEDIUM ?
		medium_samples : large_samples;
	uint8_t index;

	if (speed_percent > 80U) {
		index = 3U;
	} else if (speed_percent > 60U) {
		index = 2U;
	} else if (speed_percent > 40U) {
		index = 1U;
	} else {
		index = 0U;
	}
	return samples[index];
}

static uint32_t motor_counts_per_speed(enum board_motor_type type)
{
	return type == BOARD_MOTOR_TYPE_MEDIUM ?
		MOTOR_MEDIUM_COUNTS_PER_SPEED : MOTOR_LARGE_COUNTS_PER_SPEED;
}

static void update_motor_speed(uint32_t now_ms)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		int32_t count = tacho_count[port_index];
		int32_t count_delta = count - speed_sample_count[port_index];
		uint32_t elapsed_ms = now_ms - speed_sample_ms[port_index];
		uint8_t requested = output_power[port_index] < 0 ?
			(uint8_t)(-output_power[port_index]) :
			(uint8_t)output_power[port_index];
		uint8_t required_samples = motor_speed_sample_count(
			motor_type[port_index], requested);
		uint32_t absolute_delta = count_delta < 0 ?
			(uint32_t)(-count_delta) : (uint32_t)count_delta;

		if (elapsed_ms == 0U ||
		    (absolute_delta < required_samples &&
		     elapsed_ms < MOTOR_SPEED_SAMPLE_TIMEOUT_MS)) {
			continue;
		}
		if (count_delta == 0) {
			measured_speed_percent[port_index] = 0;
		} else {
			int64_t numerator = (int64_t)count_delta *
				(int64_t)motor_counts_per_speed(motor_type[port_index]) *
				1000LL;
			int32_t speed = (int32_t)(numerator /
				((int64_t)elapsed_ms * MOTOR_SPEED_TIMER_HZ));

			if (speed > 100) {
				speed = 100;
			} else if (speed < -100) {
				speed = -100;
			}
			measured_speed_percent[port_index] = (int8_t)speed;
		}
		speed_sample_count[port_index] = count;
		speed_sample_ms[port_index] = now_ms;
		speed_measurement_valid[port_index] = true;
	}
}

static uint8_t motor_position_max_speed(enum board_motor_type type,
	int32_t absolute_degrees)
{
	if (absolute_degrees <=
	    (type == BOARD_MOTOR_TYPE_MEDIUM ? 30 : 20)) {
		return 10U;
	}
	if (absolute_degrees <=
	    (type == BOARD_MOTOR_TYPE_MEDIUM ? 80 : 60)) {
		return 20U;
	}
	if (absolute_degrees <= 240) {
		return 30U;
	}
	if (absolute_degrees <= 280) {
		return 40U;
	}
	if (absolute_degrees <= 360) {
		return 50U;
	}
	if (absolute_degrees <= 440) {
		return 60U;
	}
	if (absolute_degrees <= 480) {
		return 70U;
	}
	if (absolute_degrees <= 520) {
		return 80U;
	}
	if (absolute_degrees <= 800) {
		return 90U;
	}
	return 100U;
}

static uint16_t motor_position_slowdown_degrees(enum board_motor_type type,
	uint8_t speed_percent)
{
	if (speed_percent <= 20U) {
		return type == BOARD_MOTOR_TYPE_MEDIUM ? 20U : 10U;
	}
	if (speed_percent <= 30U) {
		return 60U;
	}
	if (speed_percent <= 40U) {
		return 70U;
	}
	if (speed_percent <= 50U) {
		return type == BOARD_MOTOR_TYPE_MEDIUM ? 100U : 90U;
	}
	if (speed_percent <= 60U) {
		return 110U;
	}
	if (speed_percent <= 70U) {
		return type == BOARD_MOTOR_TYPE_MEDIUM ? 120U : 130U;
	}
	if (speed_percent <= 80U) {
		return type == BOARD_MOTOR_TYPE_MEDIUM ? 130U : 140U;
	}
	return type == BOARD_MOTOR_TYPE_MEDIUM ? 180U : 160U;
}

static int8_t motor_position_tracking_speed(enum board_motor_type type,
	int8_t requested_speed, int32_t position_error,
	int8_t measured_speed)
{
	uint8_t requested_magnitude = requested_speed < 0 ?
		(uint8_t)(-(int16_t)requested_speed) : (uint8_t)requested_speed;
	uint8_t measured_magnitude = measured_speed < 0 ?
		(uint8_t)(-(int16_t)measured_speed) : (uint8_t)measured_speed;
	uint8_t profile_speed = requested_magnitude > measured_magnitude ?
		requested_magnitude : measured_magnitude;
	uint16_t slowdown = motor_position_slowdown_degrees(type, profile_speed);
	uint32_t deceleration_window = (uint32_t)slowdown * 2U;
	uint32_t remaining = position_error < 0 ?
		(uint32_t)(-position_error) : (uint32_t)position_error;
	uint32_t magnitude;

	if (remaining >= deceleration_window) {
		return requested_speed;
	}
	magnitude = ((uint32_t)requested_magnitude * remaining +
		deceleration_window - 1U) / deceleration_window;
	if (magnitude < MOTOR_POSITION_MIN_SPEED_PERCENT) {
		magnitude = MOTOR_POSITION_MIN_SPEED_PERCENT;
	} else if (magnitude > requested_magnitude) {
		magnitude = requested_magnitude;
	}
	return requested_speed < 0 ? -(int8_t)magnitude : (int8_t)magnitude;
}

static void reset_motor_measurements(enum board_motor_port port, uint32_t now_ms)
{
	uint8_t port_index = (uint8_t)port;

	tacho_count[port_index] = 0;
	measured_speed_percent[port_index] = 0;
	speed_sample_count[port_index] = 0;
	speed_sample_ms[port_index] = now_ms;
	speed_measurement_valid[port_index] = false;
}

static void apply_identified_motor_type(enum board_motor_port port,
	enum board_motor_type identified_type, uint32_t now_ms)
{
	uint8_t port_index = (uint8_t)port;

	if (!motor_type_is_valid(identified_type)) {
		return;
	}
	if (motor_type[port_index] != identified_type) {
		reset_motor_measurements(port, now_ms);
	}
	motor_type[port_index] = identified_type;
}

static void cancel_identification_refresh(void)
{
	if (identification_refresh_state == MOTOR_IDENTIFICATION_REFRESH_IDLE) {
		return;
	}
	motor_tacho_direction_float(identification_refresh_port);
	motor_tacho_phase_pullup(identification_refresh_port, false);
	tacho_count[(uint8_t)identification_refresh_port] =
		identification_refresh_tacho_count;
	measured_speed_percent[(uint8_t)identification_refresh_port] = 0;
	speed_sample_count[(uint8_t)identification_refresh_port] =
		identification_refresh_tacho_count;
	speed_measurement_valid[(uint8_t)identification_refresh_port] = false;
	identification_refresh_state = MOTOR_IDENTIFICATION_REFRESH_IDLE;
	identification_refresh_automatic = false;
}

static void cancel_automatic_identification_refresh(void)
{
	if (identification_refresh_automatic) {
		cancel_identification_refresh();
	}
}

static void start_identification_refresh(uint32_t now_ms,
	enum board_motor_port port, bool automatic)
{
	identification_refresh_port = port;
	identification_refresh_started_ms = now_ms;
	identification_refresh_float_raw = 0U;
	identification_refresh_float_mv = 0U;
	identification_refresh_tacho_count = tacho_count[(uint8_t)port];
	identification_refresh_state = MOTOR_IDENTIFICATION_REFRESH_FLOAT;
	identification_refresh_automatic = automatic;
	motor_tacho_direction_float(port);
}

static bool automatic_identification_allowed(void)
{
	uint8_t port_index;

	if (normal_test_active() || stop_test_active() || dual_test_active() ||
	    any_position_control_active() || any_speed_control_active() ||
	    any_hold_control_active()) {
		return false;
	}
	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		if (output_state[port_index] != BOARD_MOTOR_OUTPUT_COAST ||
		    output_power[port_index] != 0 ||
		    measured_speed_percent[port_index] != 0) {
			return false;
		}
	}
	return true;
}

static void update_motor_identification(uint32_t now_ms)
{
	uint32_t refresh_elapsed = now_ms - identification_refresh_started_ms;

	if (identification_refresh_state ==
	    MOTOR_IDENTIFICATION_REFRESH_FLOAT) {
		const struct motor_port_config *config =
			motor_config(identification_refresh_port);
		uint16_t raw;

		if (refresh_elapsed < MOTOR_ID_REFRESH_PHASE_MS) {
			return;
		}
		raw = read_motor_id_adc(config);
		identification_refresh_float_raw = raw;
		identification_refresh_float_mv =
			(uint16_t)(((uint32_t)raw * MOTOR_ID_SCALE_MV) /
			MOTOR_ID_ADC_FULL_SCALE);
		motor_tacho_direction_drive_low(identification_refresh_port);
		identification_refresh_started_ms = now_ms;
		identification_refresh_state =
			MOTOR_IDENTIFICATION_REFRESH_DRIVE_LOW;
		return;
	}
	if (identification_refresh_state == MOTOR_IDENTIFICATION_REFRESH_DRIVE_LOW) {
		const struct motor_port_config *config =
			motor_config(identification_refresh_port);
		uint16_t low_raw;
		uint16_t low_mv;

		if (refresh_elapsed < MOTOR_ID_REFRESH_PHASE_MS) {
			return;
		}
		low_raw = read_motor_id_adc(config);
		low_mv = (uint16_t)(((uint32_t)low_raw * MOTOR_ID_SCALE_MV) /
			MOTOR_ID_ADC_FULL_SCALE);
		motor_id_pin6_low_adc_raw[(uint8_t)identification_refresh_port] =
			low_raw;
		motor_id_pin6_low_mv[(uint8_t)identification_refresh_port] = low_mv;
		motor_tacho_direction_float(identification_refresh_port);
		motor_tacho_phase_pullup(identification_refresh_port, true);
		identification_refresh_started_ms = now_ms;
		identification_refresh_state =
			MOTOR_IDENTIFICATION_REFRESH_PIN5_PULLUP;
		return;
	}
	if (identification_refresh_state ==
	    MOTOR_IDENTIFICATION_REFRESH_PIN5_PULLUP) {
		const struct motor_port_config *config =
			motor_config(identification_refresh_port);
		enum board_motor_type probe_type;
		uint16_t pullup_raw;
		uint16_t pullup_mv;

		if (refresh_elapsed < MOTOR_ID_REFRESH_PHASE_MS) {
			return;
		}
		pullup_raw = read_motor_id_adc(config);
		pullup_mv = (uint16_t)(((uint32_t)pullup_raw * MOTOR_ID_SCALE_MV) /
			MOTOR_ID_ADC_FULL_SCALE);
		motor_id_pin5_pullup_adc_raw[(uint8_t)identification_refresh_port] =
			pullup_raw;
		motor_id_pin5_pullup_mv[(uint8_t)identification_refresh_port] =
			pullup_mv;
		motor_id_pin5_pullup_high[(uint8_t)identification_refresh_port] =
			gpio_get(config->tacho_port, config->tacho_phase_pin) != 0U ? 1U : 0U;
		motor_tacho_phase_pullup(identification_refresh_port, false);
		tacho_count[(uint8_t)identification_refresh_port] =
			identification_refresh_tacho_count;
		measured_speed_percent[(uint8_t)identification_refresh_port] = 0;
		speed_sample_count[(uint8_t)identification_refresh_port] =
			identification_refresh_tacho_count;
		speed_sample_ms[(uint8_t)identification_refresh_port] = now_ms;
		speed_measurement_valid[(uint8_t)identification_refresh_port] = false;
		probe_type = motor_type_from_probe(identification_refresh_float_mv,
			motor_id_pin6_low_mv[(uint8_t)identification_refresh_port],
			pullup_mv);
		if (probe_type == motor_type_from_mv(identification_refresh_float_mv)) {
			motor_id_adc_raw[(uint8_t)identification_refresh_port] =
				identification_refresh_float_raw;
			motor_id_mv[(uint8_t)identification_refresh_port] =
				identification_refresh_float_mv;
		} else {
			motor_id_adc_raw[(uint8_t)identification_refresh_port] =
				motor_id_pin6_low_adc_raw[
				(uint8_t)identification_refresh_port];
			motor_id_mv[(uint8_t)identification_refresh_port] =
				motor_id_pin6_low_mv[(uint8_t)identification_refresh_port];
		}
		apply_identified_motor_type(identification_refresh_port, probe_type,
			now_ms);
		identification_refresh_state = MOTOR_IDENTIFICATION_REFRESH_IDLE;
		identification_refresh_automatic = false;
		return;
	}
	if ((uint32_t)(now_ms - last_automatic_identification_ms) <
	    MOTOR_ID_AUTO_SCAN_INTERVAL_MS || !automatic_identification_allowed()) {
		return;
	}
	last_automatic_identification_ms = now_ms;
	start_identification_refresh(now_ms,
		(enum board_motor_port)automatic_identification_next_port, true);
	automatic_identification_next_port = (uint8_t)
		((automatic_identification_next_port + 1U) % BOARD_MOTOR_PORT_COUNT);
}

static bool normal_test_active(void)
{
	return test_state == BOARD_MOTOR_TEST_FORWARD ||
		test_state == BOARD_MOTOR_TEST_PAUSE ||
		test_state == BOARD_MOTOR_TEST_REVERSE;
}

static bool stop_test_active(void)
{
	return stop_test_state == BOARD_MOTOR_STOP_TEST_DRIVE ||
		stop_test_state == BOARD_MOTOR_STOP_TEST_MEASURE;
}

static bool dual_test_active(void)
{
	return dual_test_state == BOARD_MOTOR_DUAL_TEST_FORWARD ||
		dual_test_state == BOARD_MOTOR_DUAL_TEST_BRAKE_PAUSE ||
		dual_test_state == BOARD_MOTOR_DUAL_TEST_REVERSE ||
		dual_test_state == BOARD_MOTOR_DUAL_TEST_FINAL_BRAKE;
}

static const struct motor_port_config *motor_config(enum board_motor_port port)
{
	return &motor_ports[(uint8_t)port];
}

static int32_t current_tacho_count(void)
{
	return tacho_count[(uint8_t)test_port];
}

static void set_direction_pin(enum board_motor_port port, uint16_t pin,
	uint8_t output_type, uint32_t pull)
{
	const struct motor_port_config *config = motor_config(port);

	gpio_mode_setup(config->direction_port, GPIO_MODE_OUTPUT, pull, pin);
	gpio_set_output_options(config->direction_port, output_type,
		GPIO_OSPEED_100MHZ, pin);
}

static void motor_output_off(enum board_motor_port port)
{
	const struct motor_port_config *config = motor_config(port);

	/* Free coast: A/M0.36A=296 and B/M0.37A=323 post-stop pulses at 60%. */
	timer_set_oc_value(TIM4, config->pwm_channel, MOTOR_PWM_OFF_COMPARE);
	gpio_clear(config->direction_port,
		config->direction_0_pin | config->direction_1_pin);
	set_direction_pin(port, config->direction_0_pin, GPIO_OTYPE_OD,
		GPIO_PUPD_NONE);
	set_direction_pin(port, config->direction_1_pin, GPIO_OTYPE_OD,
		GPIO_PUPD_NONE);
	output_state[(uint8_t)port] = BOARD_MOTOR_OUTPUT_COAST;
	output_power[(uint8_t)port] = 0;
}

static void motor_output_off_all(void)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		motor_output_off((enum board_motor_port)port_index);
	}
}

static void motor_output_high_push_pull_stop(enum board_motor_port port)
{
	const struct motor_port_config *config = motor_config(port);

	/* Active brake: A/M0.36A=39 and B/M0.37A=37 post-stop pulses at 60%. */
	timer_set_oc_value(TIM4, config->pwm_channel, MOTOR_PWM_OFF_COMPARE);
	gpio_set(config->direction_port,
		config->direction_0_pin | config->direction_1_pin);
	set_direction_pin(port, config->direction_0_pin, GPIO_OTYPE_PP,
		GPIO_PUPD_PULLUP);
	set_direction_pin(port, config->direction_1_pin, GPIO_OTYPE_PP,
		GPIO_PUPD_PULLUP);
	output_state[(uint8_t)port] = BOARD_MOTOR_OUTPUT_BRAKE;
	output_power[(uint8_t)port] = 0;
}

static void motor_apply_stop_mode(enum board_motor_port port,
	enum board_motor_stop_mode mode)
{
	if (mode == BOARD_MOTOR_STOP_HIGH_PUSH_PULL) {
		motor_output_high_push_pull_stop(port);
	} else {
		motor_output_off(port);
	}
}

static void motor_output_forward(enum board_motor_port port,
	uint32_t compare, int8_t power_percent)
{
	const struct motor_port_config *config = motor_config(port);

	motor_output_off(port);
	set_direction_pin(port, config->direction_0_pin, GPIO_OTYPE_OD,
		GPIO_PUPD_NONE);
	set_direction_pin(port, config->direction_1_pin, GPIO_OTYPE_PP,
		GPIO_PUPD_PULLUP);
	gpio_set(config->direction_port,
		config->direction_0_pin | config->direction_1_pin);
	timer_set_oc_value(TIM4, config->pwm_channel, compare);
	output_state[(uint8_t)port] = BOARD_MOTOR_OUTPUT_DRIVE;
	output_power[(uint8_t)port] = power_percent;
}

static void motor_output_reverse(enum board_motor_port port,
	uint32_t compare, int8_t power_percent)
{
	const struct motor_port_config *config = motor_config(port);

	motor_output_off(port);
	set_direction_pin(port, config->direction_0_pin, GPIO_OTYPE_PP,
		GPIO_PUPD_PULLUP);
	set_direction_pin(port, config->direction_1_pin, GPIO_OTYPE_OD,
		GPIO_PUPD_NONE);
	gpio_set(config->direction_port,
		config->direction_0_pin | config->direction_1_pin);
	timer_set_oc_value(TIM4, config->pwm_channel, compare);
	output_state[(uint8_t)port] = BOARD_MOTOR_OUTPUT_DRIVE;
	output_power[(uint8_t)port] = power_percent;
}

static int32_t absolute_i32(int32_t value)
{
	return value < 0 ? -value : value;
}

static void cancel_hold_control(enum board_motor_port port, bool coast)
{
	struct motor_hold_control *control = &hold_controls[(uint8_t)port];

	control->state = BOARD_MOTOR_HOLD_IDLE;
	control->pwm_x100 = 0;
	control->disconnect_samples = 0U;
	control->settled = false;
	if (coast) {
		motor_output_off(port);
	}
}

static bool start_hold_control(enum board_motor_port port,
	int32_t target_count)
{
	struct motor_hold_control *control;
	enum board_motor_type type;

	if (!motor_port_valid(port)) {
		return false;
	}
	type = motor_type[(uint8_t)port];
	if (type != BOARD_MOTOR_TYPE_LARGE &&
	    type != BOARD_MOTOR_TYPE_MEDIUM) {
		motor_output_off(port);
		return false;
	}
	control = &hold_controls[(uint8_t)port];
	motor_output_off(port);
	control->state = BOARD_MOTOR_HOLD_HOLDING;
	control->type = type;
	control->target_count = target_count;
	control->last_count = tacho_count[(uint8_t)port];
	control->last_control_ms = 0U;
	control->last_disconnect_check_ms = 0U;
	control->pwm_x100 = 0;
	control->disconnect_samples = 0U;
	control->settled = absolute_i32(target_count - control->last_count) <=
		(type == BOARD_MOTOR_TYPE_MEDIUM ?
		 MOTOR_HOLD_MEDIUM_ENTER_DEADBAND_COUNTS :
		 MOTOR_HOLD_LARGE_ENTER_DEADBAND_COUNTS);
	return true;
}

static bool handoff_position_to_hold(enum board_motor_port port,
	const struct motor_position_control *position, uint32_t now_ms)
{
	struct motor_hold_control *hold;
	int32_t current;
	int32_t enter_deadband;

	if (!motor_port_valid(port) || position == NULL ||
	    (position->type != BOARD_MOTOR_TYPE_LARGE &&
	     position->type != BOARD_MOTOR_TYPE_MEDIUM)) {
		return false;
	}
	current = tacho_count[(uint8_t)port];
	enter_deadband = position->type == BOARD_MOTOR_TYPE_MEDIUM ?
		MOTOR_HOLD_MEDIUM_ENTER_DEADBAND_COUNTS :
		MOTOR_HOLD_LARGE_ENTER_DEADBAND_COUNTS;
	hold = &hold_controls[(uint8_t)port];
	hold->state = BOARD_MOTOR_HOLD_HOLDING;
	hold->type = position->type;
	hold->target_count = position->target_count;
	hold->last_count = position->last_count;
	hold->last_control_ms = now_ms;
	hold->last_disconnect_check_ms = 0U;
	hold->pwm_x100 = position->pwm_x100;
	hold->disconnect_samples = 0U;
	hold->settled = absolute_i32(position->target_count - current) <=
		enter_deadband &&
		absolute_i32(measured_speed_percent[(uint8_t)port]) <=
		MOTOR_POSITION_SETTLING_MEDIUM_SPEED_TOLERANCE_PERCENT;
	return true;
}

static void motor_apply_completion_stop_mode(enum board_motor_port port,
	enum board_motor_stop_mode mode, int32_t hold_target_count)
{
	if (mode == BOARD_MOTOR_STOP_HOLD_POSITION) {
		(void)start_hold_control(port, hold_target_count);
		return;
	}
	cancel_hold_control(port, false);
	motor_apply_stop_mode(port, mode);
}

static int32_t hold_slew_pwm(int32_t previous, int32_t requested,
	int32_t maximum_step)
{
	if (requested > previous + maximum_step) {
		return previous + maximum_step;
	}
	if (requested < previous - maximum_step) {
		return previous - maximum_step;
	}
	return requested;
}

static int32_t position_settling_slew_pwm(bool medium, int32_t previous,
	int32_t requested, int32_t drive_step)
{
	int32_t adjusted = requested;
	int32_t maximum_step = drive_step;

	if (!medium) {
		return hold_slew_pwm(previous, requested, drive_step);
	}
	if ((previous > 0 && requested < 0) ||
	    (previous < 0 && requested > 0)) {
		adjusted = 0;
	}
	if (absolute_i32(adjusted) < absolute_i32(previous)) {
		maximum_step = MOTOR_POSITION_SETTLING_MEDIUM_RELEASE_SLEW_X100;
	}
	return hold_slew_pwm(previous, adjusted, maximum_step);
}

static void apply_hold_pwm(enum board_motor_port port, int32_t pwm_x100)
{
	int32_t pwm_percent = pwm_x100 / 100;

	if (pwm_percent == output_power[(uint8_t)port]) {
		return;
	}
	if (pwm_percent > 0) {
		motor_output_forward(port, 100U - (uint32_t)pwm_percent,
			(int8_t)pwm_percent);
	} else if (pwm_percent < 0) {
		motor_output_reverse(port, 100U - (uint32_t)(-pwm_percent),
			(int8_t)pwm_percent);
	} else {
		motor_output_off(port);
	}
}

static void update_hold_control(uint32_t now_ms)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		enum board_motor_port port = (enum board_motor_port)port_index;
		struct motor_hold_control *control = &hold_controls[port_index];
		int32_t current;
		int32_t velocity;
		int32_t error;
		int32_t absolute_error;
		int32_t requested_pwm;
		int32_t kp_x100;
		int32_t kd_x100;
		int32_t maximum_pwm_x100;
		int32_t minimum_recovery_pwm_x100;
		int32_t pwm_slew_x100;
		int32_t enter_deadband;
		int32_t exit_deadband;

		if (control->state != BOARD_MOTOR_HOLD_HOLDING) {
			continue;
		}
		if ((uint32_t)(now_ms - control->last_disconnect_check_ms) >=
		    MOTOR_HOLD_DISCONNECT_CHECK_MS) {
			const struct motor_port_config *config = motor_config(port);
			uint16_t raw = read_motor_id_adc(config);
			uint16_t millivolts = (uint16_t)(((uint32_t)raw *
				MOTOR_ID_SCALE_MV) / MOTOR_ID_ADC_FULL_SCALE);

			control->last_disconnect_check_ms = now_ms;
			if (motor_type_from_mv(millivolts) == BOARD_MOTOR_TYPE_NONE) {
				if (control->disconnect_samples < UINT8_MAX) {
					control->disconnect_samples++;
				}
			} else {
				control->disconnect_samples = 0U;
			}
			if (control->disconnect_samples >=
			    MOTOR_HOLD_DISCONNECT_SAMPLES) {
				cancel_hold_control(port, true);
				motor_id_adc_raw[port_index] = raw;
				motor_id_mv[port_index] = millivolts;
				apply_identified_motor_type(port,
					BOARD_MOTOR_TYPE_NONE, now_ms);
				continue;
			}
		}
		if ((uint32_t)(now_ms - control->last_control_ms) <
		    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
			continue;
		}
		control->last_control_ms = now_ms;
		current = tacho_count[port_index];
		velocity = current - control->last_count;
		control->last_count = current;
		error = control->target_count - current;
		absolute_error = absolute_i32(error);
		if (control->type == BOARD_MOTOR_TYPE_MEDIUM) {
			kp_x100 = MOTOR_HOLD_MEDIUM_KP_X100;
			kd_x100 = MOTOR_HOLD_MEDIUM_KD_X100;
			maximum_pwm_x100 = MOTOR_HOLD_MEDIUM_MAX_PWM_X100;
			minimum_recovery_pwm_x100 =
				MOTOR_HOLD_MEDIUM_MIN_RECOVERY_PWM_X100;
			pwm_slew_x100 = MOTOR_HOLD_MEDIUM_PWM_SLEW_X100;
			enter_deadband = MOTOR_HOLD_MEDIUM_ENTER_DEADBAND_COUNTS;
			exit_deadband = MOTOR_HOLD_MEDIUM_EXIT_DEADBAND_COUNTS;
		} else {
			kp_x100 = MOTOR_HOLD_LARGE_KP_X100;
			kd_x100 = MOTOR_HOLD_LARGE_KD_X100;
			maximum_pwm_x100 = MOTOR_HOLD_LARGE_MAX_PWM_X100;
			minimum_recovery_pwm_x100 = 0;
			pwm_slew_x100 = MOTOR_HOLD_LARGE_PWM_SLEW_X100;
			enter_deadband = MOTOR_HOLD_LARGE_ENTER_DEADBAND_COUNTS;
			exit_deadband = MOTOR_HOLD_LARGE_EXIT_DEADBAND_COUNTS;
		}
		if ((control->settled && absolute_error <= exit_deadband) ||
		    (absolute_error <= enter_deadband &&
		     absolute_i32(velocity) <= MOTOR_HOLD_SETTLED_SPEED_COUNTS)) {
			control->settled = true;
			control->pwm_x100 = hold_slew_pwm(control->pwm_x100, 0,
				pwm_slew_x100);
			apply_hold_pwm(port, control->pwm_x100);
			continue;
		}
		control->settled = false;
		requested_pwm = error * kp_x100 - velocity * kd_x100;
		if (minimum_recovery_pwm_x100 != 0 &&
		    absolute_error >= MOTOR_HOLD_MEDIUM_RECOVERY_THRESHOLD_COUNTS &&
		    velocity == 0 &&
		    absolute_i32(requested_pwm) < minimum_recovery_pwm_x100) {
			requested_pwm = error < 0 ? -minimum_recovery_pwm_x100 :
				minimum_recovery_pwm_x100;
		}
		if (requested_pwm > maximum_pwm_x100) {
			requested_pwm = maximum_pwm_x100;
		} else if (requested_pwm < -maximum_pwm_x100) {
			requested_pwm = -maximum_pwm_x100;
		}
		control->pwm_x100 = hold_slew_pwm(control->pwm_x100,
			requested_pwm, pwm_slew_x100);
		apply_hold_pwm(port, control->pwm_x100);
	}
}

static void configure_pwm_channel(enum tim_oc_id channel)
{
	timer_set_oc_mode(TIM4, channel, TIM_OCM_PWM1);
	timer_set_oc_polarity_high(TIM4, channel);
	timer_enable_oc_preload(TIM4, channel);
	timer_set_oc_value(TIM4, channel, MOTOR_PWM_OFF_COMPARE);
	timer_enable_oc_output(TIM4, channel);
}

static bool speed_control_active(enum board_motor_port port)
{
	return speed_controls[(uint8_t)port].state == BOARD_MOTOR_SPEED_RUNNING;
}

static bool position_control_active(enum board_motor_port port)
{
	return position_controls[(uint8_t)port].state ==
		BOARD_MOTOR_POSITION_RUNNING;
}

static bool hold_control_active(enum board_motor_port port)
{
	return hold_controls[(uint8_t)port].state ==
		BOARD_MOTOR_HOLD_HOLDING;
}

static bool any_speed_control_active(void)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		if (speed_control_active((enum board_motor_port)port_index)) {
			return true;
		}
	}
	return false;
}

static bool any_position_control_active(void)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		if (position_control_active((enum board_motor_port)port_index)) {
			return true;
		}
	}
	return false;
}

static bool any_hold_control_active(void)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		if (hold_control_active((enum board_motor_port)port_index)) {
			return true;
		}
	}
	return false;
}

static bool legacy_diagnostic_active(void)
{
	return normal_test_active() || stop_test_active() || dual_test_active() ||
		(identification_refresh_state != MOTOR_IDENTIFICATION_REFRESH_IDLE &&
		 !identification_refresh_automatic);
}

static bool pair_position_owns_port(enum board_motor_port port)
{
	return pair_position_control.state ==
			BOARD_MOTOR_PAIR_POSITION_RUNNING &&
		(port == pair_position_control.left_port ||
		 port == pair_position_control.right_port);
}

static bool pair_speed_owns_port(enum board_motor_port port)
{
	return pair_speed_control.state == BOARD_MOTOR_PAIR_SPEED_RUNNING &&
		(port == pair_speed_control.left_port ||
		 port == pair_speed_control.right_port);
}

static bool port_control_active(enum board_motor_port port)
{
	return speed_control_active(port) || position_control_active(port) ||
		pair_position_owns_port(port) || hold_control_active(port);
}

static int32_t position_control_progress(enum board_motor_port port)
{
	const struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	int32_t movement = tacho_count[(uint8_t)port] - control->start_count;

	return control->target_count < control->start_count ? -movement : movement;
}

static int32_t pair_position_target_magnitude(enum board_motor_port port)
{
	const struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	int32_t target = control->target_count - control->start_count;

	return target < 0 ? -target : target;
}

static int32_t round_pair_ratio(int64_t numerator, int32_t denominator)
{
	if (numerator >= 0) {
		return (int32_t)((numerator + denominator / 2) / denominator);
	}
	return (int32_t)((numerator - denominator / 2) / denominator);
}

static int8_t pair_sync_pd_correction(int32_t control_error,
	int32_t previous_control_error, int8_t current)
{
	return board_motor_pair_sync_correction(control_error, previous_control_error, current);
}

static void pair_sync_limit_recovery_error(int32_t synchronization_error,
	int32_t *synchronization_offset, int32_t *previous_control_error,
	uint8_t *offset_release_samples)
{
	int32_t limited_error = synchronization_error;

	if (limited_error > MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS) {
		limited_error = MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS;
	} else if (limited_error < -MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS) {
		limited_error = -MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS;
	}
	*synchronization_offset = synchronization_error - limited_error;
	*previous_control_error = limited_error;
	*offset_release_samples = 0U;
}

static void pair_sync_release_offset(int32_t *synchronization_offset,
	uint8_t *offset_release_samples, bool recovery_active)
{
	if (*synchronization_offset == 0 || recovery_active) {
		*offset_release_samples = 0U;
		return;
	}
	if (++(*offset_release_samples) <
	    MOTOR_PAIR_SYNC_OFFSET_RELEASE_SAMPLES) {
		return;
	}
	*offset_release_samples = 0U;
	if (*synchronization_offset > 0) {
		(*synchronization_offset)--;
	} else {
		(*synchronization_offset)++;
	}
}

static int32_t pair_recovery_pwm_limit(enum board_motor_type type,
	uint8_t target_magnitude, int32_t peer_pwm_x100,
	uint8_t peer_target_magnitude)
{
	int32_t margin = type == BOARD_MOTOR_TYPE_MEDIUM ?
		MOTOR_PAIR_RECOVERY_MEDIUM_PWM_MARGIN_X100 :
		MOTOR_PAIR_RECOVERY_LARGE_PWM_MARGIN_X100;
	int32_t limit = (int32_t)target_magnitude * 100 + margin;

	if (peer_target_magnitude != 0U) {
		int32_t peer_equivalent = round_pair_ratio(
			(int64_t)absolute_i32(peer_pwm_x100) * target_magnitude,
			peer_target_magnitude) +
			MOTOR_PAIR_RECOVERY_PEER_PWM_MARGIN_X100;

		if (peer_equivalent > limit) {
			limit = peer_equivalent;
		}
	}
	if (limit > 10000) {
		limit = 10000;
	}
	return limit;
}

static int32_t pair_position_scaled_progress(enum board_motor_port port)
{
	int32_t left_target = pair_position_target_magnitude(
		pair_position_control.left_port);
	int32_t right_target = pair_position_target_magnitude(
		pair_position_control.right_port);
	int32_t reference_target = left_target > right_target ?
		left_target : right_target;
	int32_t port_target = pair_position_target_magnitude(port);

	if (port_target == 0) {
		return 0;
	}
	return round_pair_ratio((int64_t)position_control_progress(port) *
		reference_target, port_target);
}

static uint8_t pair_position_scaled_max_speed(uint8_t maximum_speed_percent,
	int32_t target_magnitude, int32_t reference_target)
{
	if (maximum_speed_percent == 0U) {
		return 0U;
	}
	int32_t scaled_speed = round_pair_ratio(
		(int64_t)maximum_speed_percent * target_magnitude,
		reference_target);

	if (scaled_speed < (int32_t)MOTOR_POSITION_MIN_SPEED_PERCENT) {
		scaled_speed = (int32_t)MOTOR_POSITION_MIN_SPEED_PERCENT;
	}
	return (uint8_t)scaled_speed;
}

static int8_t pair_adjust_position_speed(enum board_motor_port port,
	int8_t target_speed)
{
	int32_t left_target;
	int32_t right_target;
	int32_t reference_target;
	int32_t reduction = 0;
	int8_t correction = pair_position_control.correction_percent;
	bool correction_applies;

	if (!pair_position_owns_port(port) || target_speed == 0) {
		return target_speed;
	}
	correction_applies =
		(port == pair_position_control.left_port &&
		 correction > 0) ||
		(port == pair_position_control.right_port &&
		 correction < 0);
	if (port == pair_position_control.leader_port && correction_applies) {
		left_target = pair_position_target_magnitude(pair_position_control.left_port);
		right_target = pair_position_target_magnitude(pair_position_control.right_port);
		reference_target = left_target > right_target ? left_target : right_target;
		if (reference_target != 0) {
			reduction = round_pair_ratio((int64_t)absolute_i32(correction) *
				pair_position_target_magnitude(port), reference_target);
		}
	}
	return board_motor_pair_adjust_speed(target_speed, reduction);
}

static void record_pair_position_error(void)
{
	int32_t absolute_error;

	pair_position_control.synchronization_error =
		pair_position_scaled_progress(pair_position_control.left_port) -
		pair_position_scaled_progress(pair_position_control.right_port);
	absolute_error = pair_position_control.synchronization_error < 0 ?
		-pair_position_control.synchronization_error :
		pair_position_control.synchronization_error;
	if (absolute_error >
	    pair_position_control.maximum_synchronization_error) {
		pair_position_control.maximum_synchronization_error = absolute_error;
	}
}

static bool pair_position_begin_recovery(enum board_motor_port port,
	enum board_motor_port peer_port)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	const struct motor_position_control *peer =
		&position_controls[(uint8_t)peer_port];
	int32_t movement = tacho_count[(uint8_t)port] - control->last_count;
	uint8_t target_magnitude = control->requested_speed < 0 ?
		(uint8_t)(-(int16_t)control->requested_speed) :
		(uint8_t)control->requested_speed;
	uint8_t peer_target_magnitude = peer->requested_speed < 0 ?
		(uint8_t)(-(int16_t)peer->requested_speed) :
		(uint8_t)peer->requested_speed;

	if (control->requested_speed < 0) {
		movement = -movement;
	}
	if (control->stalled && control->pair_recovery_samples == 0U &&
	    control->pair_recovery_pwm_limit_x100 == 0) {
		control->pair_recovery_pwm_limit_x100 = (int16_t)
			absolute_i32(control->pwm_x100);
	}
	if (!control->stalled || movement <= 0 ||
	    control->pair_recovery_samples != 0U) {
		return false;
	}
	control->pair_recovery_pwm_limit_x100 = (int16_t)
		pair_recovery_pwm_limit(control->type, target_magnitude,
			peer->pwm_x100, peer_target_magnitude);
	control->pair_recovery_samples = MOTOR_PAIR_RECOVERY_HOLDOFF_SAMPLES;
	return true;
}

static void update_pair_position_correction(uint32_t now_ms)
{
	int32_t control_error;
	const struct motor_position_control *left;
	const struct motor_position_control *right;
	bool left_recovered;
	bool right_recovered;
	bool recovery_active;

	if (pair_position_control.state !=
	    BOARD_MOTOR_PAIR_POSITION_RUNNING) {
		pair_position_control.correction_percent = 0;
		pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		return;
	}
	if ((uint32_t)(now_ms - pair_position_control.last_sync_ms) <
	    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
		return;
	}
	pair_position_control.last_sync_ms = now_ms;
	left = &position_controls[(uint8_t)pair_position_control.left_port];
	right = &position_controls[(uint8_t)pair_position_control.right_port];
	if (left->requested_speed == 0 || right->requested_speed == 0) {
		pair_position_control.synchronization_error = 0;
		pair_position_control.synchronization_offset = 0;
		pair_position_control.previous_control_error = 0;
		pair_position_control.correction_percent = 0;
		pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		return;
	}
	record_pair_position_error();
	if (!position_control_active(pair_position_control.left_port) ||
	    !position_control_active(pair_position_control.right_port)) {
		pair_position_control.correction_percent = 0;
		pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		return;
	}
	left_recovered = pair_position_begin_recovery(
		pair_position_control.left_port,
		pair_position_control.right_port);
	right_recovered = pair_position_begin_recovery(
		pair_position_control.right_port,
		pair_position_control.left_port);
	if (left_recovered || right_recovered) {
		pair_sync_limit_recovery_error(
			pair_position_control.synchronization_error,
			&pair_position_control.synchronization_offset,
			&pair_position_control.previous_control_error,
			&pair_position_control.offset_release_samples);
	}
	recovery_active =
		position_controls[(uint8_t)pair_position_control.left_port].
			pair_recovery_samples != 0U ||
		position_controls[(uint8_t)pair_position_control.right_port].
			pair_recovery_samples != 0U;
	pair_sync_release_offset(&pair_position_control.synchronization_offset,
		&pair_position_control.offset_release_samples, recovery_active);
	control_error = pair_position_control.synchronization_error -
		pair_position_control.synchronization_offset;
	pair_position_control.correction_percent = pair_sync_pd_correction(
		control_error, pair_position_control.previous_control_error,
		pair_position_control.correction_percent);
	pair_position_control.previous_control_error = control_error;
	if (pair_position_control.correction_percent > 0) {
		pair_position_control.leader_port = pair_position_control.left_port;
	} else if (pair_position_control.correction_percent < 0) {
		pair_position_control.leader_port = pair_position_control.right_port;
	} else {
		pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	}
	if (!left_recovered &&
	    position_controls[(uint8_t)pair_position_control.left_port].
		pair_recovery_samples != 0U) {
		position_controls[(uint8_t)pair_position_control.left_port].
			pair_recovery_samples--;
	}
	if (!right_recovered &&
	    position_controls[(uint8_t)pair_position_control.right_port].
		pair_recovery_samples != 0U) {
		position_controls[(uint8_t)pair_position_control.right_port].
			pair_recovery_samples--;
	}
}

static int32_t pair_speed_progress(enum board_motor_port port)
{
	int32_t start_count = port == pair_speed_control.left_port ?
		pair_speed_control.left_start_count :
		pair_speed_control.right_start_count;
	int32_t movement = tacho_count[(uint8_t)port] - start_count;

	return speed_controls[(uint8_t)port].requested_speed < 0 ?
		-movement : movement;
}

static uint8_t pair_speed_target_magnitude(enum board_motor_port port)
{
	int8_t target = port == pair_speed_control.left_port ?
		pair_speed_control.left_requested_speed :
		pair_speed_control.right_requested_speed;

	return target < 0 ? (uint8_t)(-(int16_t)target) : (uint8_t)target;
}

static int32_t pair_speed_scaled_progress(enum board_motor_port port)
{
	uint8_t left_target = pair_speed_target_magnitude(
		pair_speed_control.left_port);
	uint8_t right_target = pair_speed_target_magnitude(
		pair_speed_control.right_port);
	uint8_t reference_target = left_target > right_target ?
		left_target : right_target;
	uint8_t port_target = pair_speed_target_magnitude(port);

	if (port_target == 0U) {
		return 0;
	}
	return round_pair_ratio((int64_t)pair_speed_progress(port) *
		reference_target, port_target);
}

static int16_t pair_speed_scaled_correction(enum board_motor_port port,
	int8_t correction)
{
	uint8_t left_target = pair_speed_target_magnitude(
		pair_speed_control.left_port);
	uint8_t right_target = pair_speed_target_magnitude(
		pair_speed_control.right_port);
	uint8_t reference_target = left_target > right_target ?
		left_target : right_target;
	int16_t correction_magnitude = correction < 0 ?
		-(int16_t)correction : (int16_t)correction;
	int32_t scaled_correction;

	if (correction_magnitude == 0 || reference_target == 0U) {
		return 0;
	}
	scaled_correction = round_pair_ratio(
		(int64_t)correction_magnitude * pair_speed_target_magnitude(port),
		reference_target);
	return scaled_correction == 0 ? 1 : (int16_t)scaled_correction;
}

static void finish_pair_speed(enum board_motor_stop_mode stop_mode,
	enum board_motor_pair_speed_state state)
{
	struct motor_speed_control *left =
		&speed_controls[(uint8_t)pair_speed_control.left_port];
	struct motor_speed_control *right =
		&speed_controls[(uint8_t)pair_speed_control.right_port];

	left->state = BOARD_MOTOR_SPEED_IDLE;
	left->requested_speed = 0;
	left->pwm_x100 = 0;
	left->pair_tick_progress = 0;
	left->pair_stall_samples = 0U;
	left->pair_recovery_samples = 0U;
	left->pair_recovery_pwm_limit_x100 = 0;
	left->pair_stalled = false;
	right->state = BOARD_MOTOR_SPEED_IDLE;
	right->requested_speed = 0;
	right->pwm_x100 = 0;
	right->pair_tick_progress = 0;
	right->pair_stall_samples = 0U;
	right->pair_recovery_samples = 0U;
	right->pair_recovery_pwm_limit_x100 = 0;
	right->pair_stalled = false;
	motor_apply_completion_stop_mode(pair_speed_control.left_port, stop_mode,
		tacho_count[(uint8_t)pair_speed_control.left_port]);
	motor_apply_completion_stop_mode(pair_speed_control.right_port, stop_mode,
		tacho_count[(uint8_t)pair_speed_control.right_port]);
	pair_speed_control.correction_percent = 0;
	pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.synchronization_offset = 0;
	pair_speed_control.previous_control_error = 0;
	pair_speed_control.offset_release_samples = 0U;
	pair_speed_control.state = state;
	if (state == BOARD_MOTOR_PAIR_SPEED_IDLE) {
		pair_speed_control.left_requested_speed = 0;
		pair_speed_control.right_requested_speed = 0;
		pair_speed_control.duration_ms = 0U;
		pair_speed_control.elapsed_ms = 0U;
	}
}

static void update_pair_speed_state(uint32_t now_ms)
{
	if (pair_speed_control.state != BOARD_MOTOR_PAIR_SPEED_RUNNING ||
	    pair_speed_control.duration_ms == 0U) {
		return;
	}
	pair_speed_control.elapsed_ms = now_ms - pair_speed_control.started_ms;
	if (pair_speed_control.elapsed_ms < pair_speed_control.duration_ms) {
		return;
	}
	pair_speed_control.elapsed_ms = pair_speed_control.duration_ms;
	record_pair_speed_error();
	finish_pair_speed(pair_speed_control.completion_stop_mode,
		BOARD_MOTOR_PAIR_SPEED_COMPLETE);
}

static void update_timed_speed_state(uint32_t now_ms)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		struct motor_speed_control *control = &speed_controls[port_index];
		uint32_t duration_ms;

		if (control->state != BOARD_MOTOR_SPEED_RUNNING ||
		    control->duration_ms == 0U) {
			continue;
		}
		control->elapsed_ms = now_ms - control->started_ms;
		if (control->elapsed_ms < control->duration_ms) {
			continue;
		}
		duration_ms = control->duration_ms;
		control->state = BOARD_MOTOR_SPEED_IDLE;
		control->requested_speed = 0;
		control->duration_ms = 0U;
		control->elapsed_ms = duration_ms;
		control->pwm_x100 = 0;
		motor_apply_completion_stop_mode(
			(enum board_motor_port)port_index,
			control->completion_stop_mode,
			tacho_count[port_index]);
	}
}

static void record_pair_speed_error(void)
{
	int32_t absolute_error;

	if (pair_speed_target_magnitude(pair_speed_control.left_port) == 0U ||
	    pair_speed_target_magnitude(pair_speed_control.right_port) == 0U) {
		pair_speed_control.synchronization_error = 0;
		return;
	}
	pair_speed_control.synchronization_error =
		pair_speed_scaled_progress(pair_speed_control.left_port) -
		pair_speed_scaled_progress(pair_speed_control.right_port);
	absolute_error = pair_speed_control.synchronization_error < 0 ?
		-pair_speed_control.synchronization_error :
		pair_speed_control.synchronization_error;
	if (absolute_error > pair_speed_control.maximum_synchronization_error) {
		pair_speed_control.maximum_synchronization_error = absolute_error;
	}
}

static int16_t pair_speed_sample_tick_progress(enum board_motor_port port)
{
	struct motor_speed_control *control = &speed_controls[(uint8_t)port];
	int32_t movement = tacho_count[(uint8_t)port] - control->pair_last_count;

	control->pair_last_count = tacho_count[(uint8_t)port];
	if (control->requested_speed < 0) {
		movement = -movement;
	}
	if (movement > INT16_MAX) {
		movement = INT16_MAX;
	} else if (movement < INT16_MIN) {
		movement = INT16_MIN;
	}
	control->pair_tick_progress = (int16_t)movement;
	return control->pair_tick_progress;
}

static bool pair_speed_update_stall_state(enum board_motor_port port)
{
	struct motor_speed_control *control = &speed_controls[(uint8_t)port];
	uint8_t target_magnitude = control->requested_speed < 0 ?
		(uint8_t)(-(int16_t)control->requested_speed) :
		(uint8_t)control->requested_speed;
	bool high_pwm = absolute_i32(control->pwm_x100) >=
		MOTOR_PAIR_STALL_PWM_X100;

	(void)pair_speed_sample_tick_progress(port);
	if (target_magnitude == 0U) {
		control->pair_stall_samples = 0U;
		control->pair_recovery_samples = 0U;
		control->pair_recovery_pwm_limit_x100 = 0;
		control->pair_stalled = false;
		return false;
	}
	if (control->pair_stalled) {
		if (control->pair_tick_progress <= 0) {
			return false;
		}
		control->pair_stalled = false;
		control->pair_stall_samples = 0U;
		control->pair_recovery_samples =
			MOTOR_PAIR_RECOVERY_HOLDOFF_SAMPLES;
		return true;
	}
	if (control->pair_recovery_samples != 0U) {
		control->pair_stall_samples = 0U;
		return false;
	}
	if (!high_pwm || control->pair_tick_progress > 0) {
		control->pair_stall_samples = 0U;
		control->pair_recovery_pwm_limit_x100 = 0;
		return false;
	}
	if (control->pair_stall_samples == 0U) {
		/* Freeze the incremental speed controller before it reaches 100%. */
		control->pair_recovery_pwm_limit_x100 = (int16_t)
			absolute_i32(control->pwm_x100);
	}
	if (control->pair_stall_samples < MOTOR_PAIR_STALL_ENTER_SAMPLES) {
		control->pair_stall_samples++;
	}
	if (control->pair_stall_samples >= MOTOR_PAIR_STALL_ENTER_SAMPLES) {
		control->pair_stalled = true;
	}
	return false;
}

static void pair_speed_set_recovery_limit(enum board_motor_port port,
	enum board_motor_port peer_port)
{
	struct motor_speed_control *control = &speed_controls[(uint8_t)port];
	const struct motor_speed_control *peer =
		&speed_controls[(uint8_t)peer_port];
	uint8_t target_magnitude = pair_speed_target_magnitude(port);
	uint8_t peer_target_magnitude = pair_speed_target_magnitude(peer_port);

	control->pair_recovery_pwm_limit_x100 = (int16_t)
		pair_recovery_pwm_limit(control->type, target_magnitude,
			peer->pwm_x100, peer_target_magnitude);
}

static void update_pair_speed_correction(uint32_t now_ms)
{
	int32_t control_error;
	bool left_recovered;
	bool right_recovered;
	bool recovery_active;

	if (pair_speed_control.state != BOARD_MOTOR_PAIR_SPEED_RUNNING) {
		pair_speed_control.correction_percent = 0;
		pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		return;
	}
	if ((uint32_t)(now_ms - pair_speed_control.last_sync_ms) <
	    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
		return;
	}
	pair_speed_control.last_sync_ms = now_ms;
	if (!speed_control_active(pair_speed_control.left_port) ||
	    !speed_control_active(pair_speed_control.right_port)) {
		(void)board_motor_stop_pair_speed(
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
		return;
	}
	if (pair_speed_target_magnitude(pair_speed_control.left_port) == 0U ||
	    pair_speed_target_magnitude(pair_speed_control.right_port) == 0U) {
		pair_speed_control.synchronization_error = 0;
		pair_speed_control.synchronization_offset = 0;
		pair_speed_control.previous_control_error = 0;
		pair_speed_control.correction_percent = 0;
		pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		return;
	}
	record_pair_speed_error();
	left_recovered = pair_speed_update_stall_state(
		pair_speed_control.left_port);
	right_recovered = pair_speed_update_stall_state(
		pair_speed_control.right_port);
	if (left_recovered) {
		pair_speed_set_recovery_limit(pair_speed_control.left_port,
			pair_speed_control.right_port);
	}
	if (right_recovered) {
		pair_speed_set_recovery_limit(pair_speed_control.right_port,
			pair_speed_control.left_port);
	}
	if (left_recovered || right_recovered) {
		pair_sync_limit_recovery_error(
			pair_speed_control.synchronization_error,
			&pair_speed_control.synchronization_offset,
			&pair_speed_control.previous_control_error,
			&pair_speed_control.offset_release_samples);
	}
	recovery_active =
		speed_controls[(uint8_t)pair_speed_control.left_port].pair_stalled ||
		speed_controls[(uint8_t)pair_speed_control.right_port].pair_stalled ||
		speed_controls[(uint8_t)pair_speed_control.left_port].
			pair_recovery_samples != 0U ||
		speed_controls[(uint8_t)pair_speed_control.right_port].
			pair_recovery_samples != 0U;
	pair_sync_release_offset(&pair_speed_control.synchronization_offset,
		&pair_speed_control.offset_release_samples, recovery_active);
	control_error = pair_speed_control.synchronization_error -
		pair_speed_control.synchronization_offset;
	pair_speed_control.correction_percent = pair_sync_pd_correction(
		control_error, pair_speed_control.previous_control_error,
		pair_speed_control.correction_percent);
	pair_speed_control.previous_control_error = control_error;
	if (pair_speed_control.correction_percent > 0) {
		pair_speed_control.leader_port = pair_speed_control.left_port;
	} else if (pair_speed_control.correction_percent < 0) {
		pair_speed_control.leader_port = pair_speed_control.right_port;
	} else {
		pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	}
	if (!left_recovered &&
	    speed_controls[(uint8_t)pair_speed_control.left_port].
		pair_recovery_samples != 0U) {
		speed_controls[(uint8_t)pair_speed_control.left_port].
			pair_recovery_samples--;
	}
	if (!right_recovered &&
	    speed_controls[(uint8_t)pair_speed_control.right_port].
		pair_recovery_samples != 0U) {
		speed_controls[(uint8_t)pair_speed_control.right_port].
			pair_recovery_samples--;
	}
}

static int8_t pair_adjust_continuous_speed(enum board_motor_port port,
	int8_t target_speed)
{
	int32_t reduction = 0;
	int8_t correction = pair_speed_control.correction_percent;
	bool correction_applies;

	if (!pair_speed_owns_port(port) ||
	    pair_speed_target_magnitude(pair_speed_control.left_port) == 0U ||
	    pair_speed_target_magnitude(pair_speed_control.right_port) == 0U) {
		return target_speed;
	}
	correction_applies =
		(port == pair_speed_control.left_port && correction > 0) ||
		(port == pair_speed_control.right_port && correction < 0);
	if (port == pair_speed_control.leader_port && correction_applies) {
		reduction = pair_speed_scaled_correction(port, correction);
	}
	return board_motor_pair_adjust_speed(target_speed, reduction);
}

static int32_t pair_active_recovery_pwm_limit(enum board_motor_port port)
{
	if (pair_speed_owns_port(port) &&
	    (speed_controls[(uint8_t)port].pair_stall_samples != 0U ||
	     speed_controls[(uint8_t)port].pair_stalled ||
	     speed_controls[(uint8_t)port].pair_recovery_samples != 0U)) {
		return speed_controls[(uint8_t)port].pair_recovery_pwm_limit_x100;
	}
	if (pair_position_owns_port(port) &&
	    (position_controls[(uint8_t)port].stall_candidate ||
	     position_controls[(uint8_t)port].stalled ||
	     position_controls[(uint8_t)port].pair_recovery_samples != 0U)) {
		return position_controls[(uint8_t)port].pair_recovery_pwm_limit_x100;
	}
	return 10000;
}

static void apply_closed_loop_speed(enum board_motor_port port,
	int8_t target_speed, int32_t *pwm_x100, uint32_t now_ms)
{
	int32_t speed_error = (int32_t)target_speed -
		measured_speed_percent[(uint8_t)port];
	int32_t pwm_percent;

	if (pair_position_owns_port(port) || pair_speed_owns_port(port)) {
		*pwm_x100 = board_motor_pair_regulator_step(&pair_regulators[(uint8_t)port],
			tacho_count[(uint8_t)port], now_ms, motor_counts_per_speed(motor_type[(uint8_t)port]),
			target_speed, *pwm_x100, pair_active_recovery_pwm_limit(port));
		apply_hold_pwm(port, *pwm_x100);
		return;
	}

	if (target_speed == 0) {
		/* Zero remains velocity control; release torque once motion reaches zero. */
		if (measured_speed_percent[(uint8_t)port] == 0) {
			*pwm_x100 = 0;
			if (output_power[(uint8_t)port] != 0) {
				motor_output_off(port);
			}
			return;
		}
	}
	/* Normal control keeps the original incremental P=0.08 step every 10 ms. */
	*pwm_x100 += speed_error * 8;
	if (*pwm_x100 > 10000) {
		*pwm_x100 = 10000;
	} else if (*pwm_x100 < -10000) {
		*pwm_x100 = -10000;
	}
	if (target_speed > 0 && *pwm_x100 < 0) {
		*pwm_x100 = 0;
	} else if (target_speed < 0 && *pwm_x100 > 0) {
		*pwm_x100 = 0;
	}
	pwm_percent = *pwm_x100 / 100;
	if (pwm_percent == output_power[(uint8_t)port]) {
		return;
	}
	if (pwm_percent > 0) {
		motor_output_forward(port, 100U - (uint32_t)pwm_percent,
			(int8_t)pwm_percent);
	} else if (pwm_percent < 0) {
		motor_output_reverse(port, 100U - (uint32_t)(-pwm_percent),
			(int8_t)pwm_percent);
	} else {
		motor_output_off(port);
	}
}

static void update_speed_control(uint32_t now_ms)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		enum board_motor_port port = (enum board_motor_port)port_index;
		struct motor_speed_control *control = &speed_controls[port_index];

		if (!speed_control_active(port) ||
		    (uint32_t)(now_ms - control->last_control_ms) <
		    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
			continue;
		}
		control->last_control_ms = now_ms;
		apply_closed_loop_speed(port,
			pair_adjust_continuous_speed(port, control->requested_speed),
			&control->pwm_x100, now_ms);
	}
}

static void finish_position_control(enum board_motor_port port, uint32_t now_ms,
	enum board_motor_position_state state)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	bool handed_off_to_hold = false;

	if (state == BOARD_MOTOR_POSITION_COMPLETE) {
		if (control->completion_stop_mode ==
		    BOARD_MOTOR_STOP_HOLD_POSITION) {
			if (control->phase == MOTOR_POSITION_PHASE_SETTLING &&
			    !pair_position_owns_port(port)) {
				handed_off_to_hold = handoff_position_to_hold(port,
					control, now_ms);
			}
			if (!handed_off_to_hold) {
				motor_apply_completion_stop_mode(port,
					control->completion_stop_mode,
					control->target_count);
			}
		} else {
			motor_apply_stop_mode(port,
				BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
		}
	} else {
		motor_output_off(port);
	}
	control->state = state;
	control->finished_ms = now_ms;
	control->stall_candidate = false;
	control->stalled = false;
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	if (!handed_off_to_hold) {
		control->pwm_x100 = 0;
	}
}

static void clear_position_stall(struct motor_position_control *control)
{
	control->stall_candidate_ms = 0U;
	control->stall_candidate = false;
	control->stalled = false;
}

static void update_position_stall(uint32_t now_ms,
	enum board_motor_port port, int32_t position_error)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	bool medium = control->type == BOARD_MOTOR_TYPE_MEDIUM;
	int32_t position_tolerance = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_POSITION_TOLERANCE_COUNTS :
		MOTOR_POSITION_SETTLING_LARGE_POSITION_TOLERANCE_COUNTS;
	int32_t speed_tolerance = medium ?
		MOTOR_POSITION_STALL_MEDIUM_SPEED_PERCENT :
		MOTOR_POSITION_STALL_LARGE_SPEED_PERCENT;
	uint32_t stall_ms = medium ? MOTOR_POSITION_STALL_MEDIUM_MS :
		MOTOR_POSITION_STALL_LARGE_MS;
	bool low_speed = speed_measurement_valid[(uint8_t)port] &&
		absolute_i32(measured_speed_percent[(uint8_t)port]) <=
			speed_tolerance;
	bool limited_actuation = control->phase ==
		MOTOR_POSITION_PHASE_SETTLING ||
		absolute_i32(control->pwm_x100) >=
			MOTOR_POSITION_STALL_TRACKING_PWM_X100;

	if (control->requested_speed == 0 ||
	    absolute_i32(position_error) <= position_tolerance ||
	    !low_speed || !limited_actuation) {
		clear_position_stall(control);
		if (control->pair_recovery_samples == 0U) {
			control->pair_recovery_pwm_limit_x100 = 0;
		}
		return;
	}
	if (!control->stall_candidate) {
		control->stall_candidate = true;
		control->stall_candidate_ms = now_ms;
		if (pair_position_owns_port(port)) {
			control->pair_recovery_pwm_limit_x100 = (int16_t)
				absolute_i32(control->pwm_x100);
		}
		return;
	}
	if ((uint32_t)(now_ms - control->stall_candidate_ms) >= stall_ms) {
		control->stalled = true;
	}
}

static void update_position_settling(enum board_motor_port port,
	uint32_t now_ms, int32_t position_error, int32_t velocity)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	bool medium = control->type == BOARD_MOTOR_TYPE_MEDIUM;
	int32_t kp_x100 = medium ? MOTOR_POSITION_SETTLING_MEDIUM_KP_X100 :
		MOTOR_POSITION_SETTLING_LARGE_KP_X100;
	int32_t kd_x100 = medium ? MOTOR_POSITION_SETTLING_MEDIUM_KD_X100 :
		MOTOR_POSITION_SETTLING_LARGE_KD_X100;
	int32_t maximum_pwm_x100 = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_MAX_PWM_X100 :
		MOTOR_POSITION_SETTLING_LARGE_MAX_PWM_X100;
	int32_t reverse_maximum_pwm_x100 = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_REVERSE_MAX_PWM_X100 :
		MOTOR_POSITION_SETTLING_LARGE_REVERSE_MAX_PWM_X100;
	int32_t pwm_slew_x100 = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_PWM_SLEW_X100 :
		MOTOR_POSITION_SETTLING_LARGE_PWM_SLEW_X100;
	int32_t position_tolerance = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_POSITION_TOLERANCE_COUNTS :
		MOTOR_POSITION_SETTLING_LARGE_POSITION_TOLERANCE_COUNTS;
	int32_t speed_tolerance = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_SPEED_TOLERANCE_PERCENT :
		MOTOR_POSITION_SETTLING_LARGE_SPEED_TOLERANCE_PERCENT;
	int32_t minimum_recovery_pwm_x100 = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_MIN_RECOVERY_PWM_X100 :
		MOTOR_POSITION_SETTLING_LARGE_MIN_RECOVERY_PWM_X100;
	int32_t recovery_threshold = medium ?
		MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_THRESHOLD_COUNTS :
		MOTOR_POSITION_SETTLING_LARGE_RECOVERY_THRESHOLD_COUNTS;
	int32_t absolute_error = absolute_i32(position_error);
	int32_t requested_pwm = position_error * kp_x100 - velocity * kd_x100;
	bool recovery_condition = absolute_error >= recovery_threshold &&
		absolute_i32(velocity) <=
			MOTOR_POSITION_SETTLING_RECOVERY_VELOCITY_COUNTS &&
		measured_speed_percent[(uint8_t)port] == 0 &&
		absolute_i32(requested_pwm) < minimum_recovery_pwm_x100;
	bool recovery_drive = false;
	bool recovery_cooldown = false;

	if (medium && control->recovery_drive_samples != 0U) {
		int8_t error_direction = position_error < 0 ? -1 : 1;
		bool motor_started = absolute_i32(velocity) >
			MOTOR_POSITION_SETTLING_RECOVERY_VELOCITY_COUNTS ||
			measured_speed_percent[(uint8_t)port] != 0;

		if (absolute_error <= position_tolerance || motor_started ||
		    error_direction != control->recovery_direction) {
			control->recovery_drive_samples = 0U;
			control->recovery_cooldown_samples =
				MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_COOLDOWN_SAMPLES;
			recovery_cooldown = true;
		} else {
			control->recovery_drive_samples--;
			recovery_drive = true;
			if (control->recovery_drive_samples == 0U) {
				control->recovery_cooldown_samples =
					MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_COOLDOWN_SAMPLES;
			}
		}
	} else if (medium && control->recovery_cooldown_samples != 0U) {
		control->recovery_cooldown_samples--;
		recovery_cooldown = true;
	} else if (medium && recovery_condition) {
		bool strong_recovery = absolute_error >=
			MOTOR_POSITION_SETTLING_MEDIUM_STRONG_RECOVERY_THRESHOLD_COUNTS;

		control->recovery_direction = position_error < 0 ? -1 : 1;
		control->recovery_pwm_x100 = strong_recovery ?
			MOTOR_POSITION_SETTLING_MEDIUM_STRONG_RECOVERY_PWM_X100 :
			minimum_recovery_pwm_x100;
		control->recovery_drive_samples = strong_recovery ?
			MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_DRIVE_SAMPLES - 1U : 1U;
		recovery_drive = true;
	}

	if (absolute_error <= position_tolerance &&
	    absolute_i32(velocity) <=
	    MOTOR_POSITION_SETTLING_VELOCITY_TOLERANCE_COUNTS) {
		requested_pwm = 0;
	}
	if (recovery_cooldown) {
		requested_pwm = 0;
	}
	if (requested_pwm > maximum_pwm_x100) {
		requested_pwm = maximum_pwm_x100;
	} else if (requested_pwm < -maximum_pwm_x100) {
		requested_pwm = -maximum_pwm_x100;
	}
	if (control->requested_speed > 0 &&
	    requested_pwm < -reverse_maximum_pwm_x100) {
		requested_pwm = -reverse_maximum_pwm_x100;
	} else if (control->requested_speed < 0 &&
		   requested_pwm > reverse_maximum_pwm_x100) {
		requested_pwm = reverse_maximum_pwm_x100;
	}
	if (recovery_drive || (!medium && recovery_condition)) {
		int32_t recovery_pwm = medium ? control->recovery_pwm_x100 :
			minimum_recovery_pwm_x100;

		requested_pwm = position_error < 0 ? -recovery_pwm : recovery_pwm;
	}
	if (recovery_drive) {
		control->pwm_x100 = hold_slew_pwm(control->pwm_x100,
			requested_pwm,
			MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_RAMP_X100);
	} else {
		control->pwm_x100 = position_settling_slew_pwm(medium,
			control->pwm_x100, requested_pwm, pwm_slew_x100);
	}
	apply_hold_pwm(port, control->pwm_x100);

	if (absolute_error <= position_tolerance &&
	    absolute_i32(measured_speed_percent[(uint8_t)port]) <=
	    speed_tolerance &&
	    absolute_i32(velocity) <=
	    MOTOR_POSITION_SETTLING_VELOCITY_TOLERANCE_COUNTS) {
		if (control->stable_samples < UINT8_MAX) {
			control->stable_samples++;
		}
	} else {
		control->stable_samples = 0U;
	}
	if (control->stable_samples >= MOTOR_POSITION_SETTLING_STABLE_SAMPLES) {
		finish_position_control(port, now_ms,
			BOARD_MOTOR_POSITION_COMPLETE);
	}
}

static void update_position_control_port(uint32_t now_ms,
	enum board_motor_port port)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];
	int32_t current;
	int32_t remaining;
	int32_t absolute_remaining;
	int32_t velocity;
	int8_t target_speed;
	int32_t settling_entry;
	bool pair_owned;

	if (!position_control_active(port)) {
		if (control->state == BOARD_MOTOR_POSITION_COMPLETE &&
		    control->completion_stop_mode ==
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
		    output_state[(uint8_t)port] == BOARD_MOTOR_OUTPUT_BRAKE &&
		    (uint32_t)(now_ms - control->finished_ms) >=
		    MOTOR_POSITION_BRAKE_MS) {
			motor_output_off(port);
		}
		return;
	}
	current = tacho_count[(uint8_t)port];
	remaining = control->target_count - current;
	if (control->requested_speed == 0) {
		if ((uint32_t)(now_ms - control->last_control_ms) >=
		    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
			control->last_control_ms = now_ms;
			control->last_count = current;
			apply_closed_loop_speed(port, 0, &control->pwm_x100, now_ms);
		}
		return;
	}
	pair_owned = pair_position_owns_port(port);
	if (pair_owned &&
	    ((control->requested_speed > 0 && remaining <= 0) ||
	     (control->requested_speed < 0 && remaining >= 0))) {
		finish_position_control(port, now_ms, BOARD_MOTOR_POSITION_COMPLETE);
		return;
	}
	if ((uint32_t)(now_ms - control->last_control_ms) <
	    MOTOR_SPEED_CONTROL_INTERVAL_MS) {
		return;
	}
	control->last_control_ms = now_ms;
	velocity = current - control->last_count;
	control->last_count = current;
	absolute_remaining = remaining < 0 ? -remaining : remaining;
	if (pair_owned) {
		target_speed = motor_position_tracking_speed(control->type,
			control->requested_speed, remaining,
			measured_speed_percent[(uint8_t)port]);
		target_speed = pair_adjust_position_speed(port, target_speed);
		apply_closed_loop_speed(port, target_speed, &control->pwm_x100, now_ms);
		update_position_stall(now_ms, port, remaining);
		return;
	}

	settling_entry = (int32_t)motor_position_slowdown_degrees(control->type,
		(uint8_t)(control->requested_speed < 0 ?
		-control->requested_speed : control->requested_speed)) * 2;
	if (control->phase == MOTOR_POSITION_PHASE_TRACKING &&
	    (absolute_remaining <= settling_entry ||
	     (control->requested_speed > 0 && remaining <= 0) ||
	     (control->requested_speed < 0 && remaining >= 0))) {
		control->phase = MOTOR_POSITION_PHASE_SETTLING;
		control->stable_samples = 0U;
		control->recovery_drive_samples = 0U;
		control->recovery_cooldown_samples = 0U;
		control->recovery_direction = 0;
		control->recovery_pwm_x100 = 0;
	}
	if (control->phase == MOTOR_POSITION_PHASE_TRACKING) {
		target_speed = motor_position_tracking_speed(control->type,
			control->requested_speed, remaining,
			measured_speed_percent[(uint8_t)port]);
		apply_closed_loop_speed(port, target_speed, &control->pwm_x100, now_ms);
		update_position_stall(now_ms, port, remaining);
		return;
	}
	update_position_settling(port, now_ms, remaining, velocity);
	if (position_control_active(port)) {
		update_position_stall(now_ms, port, remaining);
	}
}

static void update_position_control(uint32_t now_ms)
{
	uint8_t port_index;

	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		update_position_control_port(now_ms,
			(enum board_motor_port)port_index);
	}
}

static void update_pair_position_state(void)
{
	struct motor_position_control *left;
	struct motor_position_control *right;

	if (pair_position_control.state !=
	    BOARD_MOTOR_PAIR_POSITION_RUNNING) {
		return;
	}
	left = &position_controls[(uint8_t)pair_position_control.left_port];
	right = &position_controls[(uint8_t)pair_position_control.right_port];
	record_pair_position_error();
	if (left->state == BOARD_MOTOR_POSITION_COMPLETE &&
		   right->state == BOARD_MOTOR_POSITION_COMPLETE) {
		if (left->target_count == left->start_count) {
			motor_apply_completion_stop_mode(
				pair_position_control.left_port,
				left->completion_stop_mode,
				left->target_count);
		}
		if (right->target_count == right->start_count) {
			motor_apply_completion_stop_mode(
				pair_position_control.right_port,
				right->completion_stop_mode,
				right->target_count);
		}
		pair_position_control.correction_percent = 0;
		pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		pair_position_control.state =
			BOARD_MOTOR_PAIR_POSITION_COMPLETE;
	}
}

void board_motor_init(void)
{
	uint8_t port_index;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_GPIOG);
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_periph_clock_enable(RCC_ADC3);
	rcc_periph_clock_enable(RCC_SYSCFG);
	rcc_periph_clock_enable(RCC_TIM4);

	/* Make both H-bridge channels inactive before connecting either PWM pin. */
	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		enum board_motor_port port = (enum board_motor_port)port_index;
		const struct motor_port_config *config = motor_config(port);

		gpio_clear(config->direction_port,
			config->direction_0_pin | config->direction_1_pin);
		set_direction_pin(port, config->direction_0_pin, GPIO_OTYPE_OD,
			GPIO_PUPD_NONE);
		set_direction_pin(port, config->direction_1_pin, GPIO_OTYPE_OD,
			GPIO_PUPD_NONE);
	}

	rcc_periph_reset_pulse(RST_TIM4);
	timer_set_mode(TIM4, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM4, 67U);
	timer_set_period(TIM4, MOTOR_PWM_PERIOD);
	configure_pwm_channel(TIM_OC1);
	configure_pwm_channel(TIM_OC2);
	configure_pwm_channel(TIM_OC3);
	configure_pwm_channel(TIM_OC4);
	timer_enable_preload(TIM4);
	timer_generate_event(TIM4, TIM_EGR_UG);
	timer_enable_counter(TIM4);

	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
		MOTOR_A_PWM_PIN | MOTOR_B_PWM_PIN);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ,
		MOTOR_A_PWM_PIN | MOTOR_B_PWM_PIN);
	gpio_set_af(GPIOB, GPIO_AF2, MOTOR_A_PWM_PIN | MOTOR_B_PWM_PIN);
	gpio_mode_setup(MOTOR_C_PWM_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
		MOTOR_C_PWM_PIN);
	gpio_set_output_options(MOTOR_C_PWM_PORT, GPIO_OTYPE_PP,
		GPIO_OSPEED_100MHZ, MOTOR_C_PWM_PIN);
	gpio_set_af(MOTOR_C_PWM_PORT, GPIO_AF2, MOTOR_C_PWM_PIN);
	gpio_mode_setup(MOTOR_D_PWM_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
		MOTOR_D_PWM_PIN);
	gpio_set_output_options(MOTOR_D_PWM_PORT, GPIO_OTYPE_PP,
		GPIO_OSPEED_100MHZ, MOTOR_D_PWM_PIN);
	gpio_set_af(MOTOR_D_PWM_PORT, GPIO_AF2, MOTOR_D_PWM_PIN);

	/* Legacy encoder wiring: phase uses both edges; second pin gives direction. */
	gpio_mode_setup(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		MOTOR_A_TACHO_PHASE_PIN | MOTOR_A_TACHO_DIRECTION_PIN |
		MOTOR_B_TACHO_PHASE_PIN | MOTOR_B_TACHO_DIRECTION_PIN);
	gpio_mode_setup(MOTOR_C_TACHO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		MOTOR_C_TACHO_PHASE_PIN | MOTOR_C_TACHO_DIRECTION_PIN |
		MOTOR_D_TACHO_PHASE_PIN | MOTOR_D_TACHO_DIRECTION_PIN);
	gpio_mode_setup(MOTOR_A_ID_ADC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
		MOTOR_A_ID_ADC_PIN | MOTOR_B_ID_ADC_PIN);
	gpio_mode_setup(MOTOR_C_ID_ADC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
		MOTOR_C_ID_ADC_PIN | MOTOR_D_ID_ADC_PIN);
	exti_select_source(EXTI5, MOTOR_A_TACHO_PORT);
	exti_set_trigger(EXTI5, EXTI_TRIGGER_BOTH);
	exti_reset_request(EXTI5);
	exti_enable_request(EXTI5);
	exti_select_source(EXTI13, MOTOR_B_TACHO_PORT);
	exti_set_trigger(EXTI13, EXTI_TRIGGER_BOTH);
	exti_reset_request(EXTI13);
	exti_enable_request(EXTI13);
	exti_select_source(EXTI7, MOTOR_C_TACHO_PORT);
	exti_set_trigger(EXTI7, EXTI_TRIGGER_BOTH);
	exti_reset_request(EXTI7);
	exti_enable_request(EXTI7);
	exti_select_source(EXTI9, MOTOR_D_TACHO_PORT);
	exti_set_trigger(EXTI9, EXTI_TRIGGER_BOTH);
	exti_reset_request(EXTI9);
	exti_enable_request(EXTI9);
	nvic_set_priority(NVIC_EXTI9_5_IRQ, 64U);
	nvic_enable_irq(NVIC_EXTI9_5_IRQ);
	nvic_set_priority(NVIC_EXTI15_10_IRQ, 64U);
	nvic_enable_irq(NVIC_EXTI15_10_IRQ);

	test_state = BOARD_MOTOR_TEST_IDLE;
	test_port = BOARD_MOTOR_PORT_A;
	phase_started_ms = 0U;
	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		tacho_count[port_index] = 0;
		output_state[port_index] = BOARD_MOTOR_OUTPUT_COAST;
		output_power[port_index] = 0;
		motor_type[port_index] = BOARD_MOTOR_TYPE_UNKNOWN;
		motor_id_adc_raw[port_index] = 0U;
		motor_id_mv[port_index] = 0U;
		motor_id_pin6_low_adc_raw[port_index] = 0U;
		motor_id_pin6_low_mv[port_index] = 0U;
		motor_id_pin5_pullup_adc_raw[port_index] = 0U;
		motor_id_pin5_pullup_mv[port_index] = 0U;
		motor_id_pin5_pullup_high[port_index] = 0U;
		measured_speed_percent[port_index] = 0;
		speed_sample_count[port_index] = 0;
		speed_sample_ms[port_index] = 0U;
		speed_measurement_valid[port_index] = false;
		board_motor_pair_regulator_reset(&pair_regulators[port_index], 0, 0U);
		position_controls[port_index].state = BOARD_MOTOR_POSITION_IDLE;
		position_controls[port_index].phase = MOTOR_POSITION_PHASE_TRACKING;
		position_controls[port_index].type = BOARD_MOTOR_TYPE_UNKNOWN;
		position_controls[port_index].requested_speed = 0;
		position_controls[port_index].start_count = 0;
		position_controls[port_index].target_count = 0;
		position_controls[port_index].finished_ms = 0U;
		position_controls[port_index].last_control_ms = 0U;
		position_controls[port_index].last_count = 0;
		position_controls[port_index].pwm_x100 = 0;
		position_controls[port_index].stable_samples = 0U;
		position_controls[port_index].recovery_drive_samples = 0U;
		position_controls[port_index].recovery_cooldown_samples = 0U;
		position_controls[port_index].recovery_direction = 0;
		position_controls[port_index].recovery_pwm_x100 = 0;
		position_controls[port_index].stall_candidate_ms = 0U;
		position_controls[port_index].stall_candidate = false;
		position_controls[port_index].stalled = false;
		position_controls[port_index].pair_recovery_samples = 0U;
		position_controls[port_index].pair_recovery_pwm_limit_x100 = 0;
		position_controls[port_index].completion_stop_mode =
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
		speed_controls[port_index].state = BOARD_MOTOR_SPEED_IDLE;
		speed_controls[port_index].type = BOARD_MOTOR_TYPE_UNKNOWN;
		speed_controls[port_index].requested_speed = 0;
		speed_controls[port_index].started_ms = 0U;
		speed_controls[port_index].duration_ms = 0U;
		speed_controls[port_index].elapsed_ms = 0U;
		speed_controls[port_index].completion_stop_mode =
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
		speed_controls[port_index].last_control_ms = 0U;
		speed_controls[port_index].pwm_x100 = 0;
		speed_controls[port_index].pair_last_count = 0;
		speed_controls[port_index].pair_tick_progress = 0;
		speed_controls[port_index].pair_stall_samples = 0U;
		speed_controls[port_index].pair_recovery_samples = 0U;
		speed_controls[port_index].pair_recovery_pwm_limit_x100 = 0;
		speed_controls[port_index].pair_stalled = false;
		hold_controls[port_index].state = BOARD_MOTOR_HOLD_IDLE;
		hold_controls[port_index].type = BOARD_MOTOR_TYPE_UNKNOWN;
		hold_controls[port_index].target_count = 0;
		hold_controls[port_index].last_count = 0;
		hold_controls[port_index].last_control_ms = 0U;
		hold_controls[port_index].last_disconnect_check_ms = 0U;
		hold_controls[port_index].pwm_x100 = 0;
		hold_controls[port_index].disconnect_samples = 0U;
		hold_controls[port_index].settled = false;
	}
	forward_count = 0;
	reverse_count = 0;
	reverse_started_count = 0;
	test_run_ms = MOTOR_TEST_RUN_MS;
	test_pause_ms = MOTOR_TEST_PAUSE_MS;
	test_compare = 100U - MOTOR_TEST_POWER_PERCENT;
	stop_test_state = BOARD_MOTOR_STOP_TEST_IDLE;
	stop_test_mode = BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
	stop_test_powered_count = 0;
	stop_test_started_count = 0;
	stop_test_stopped_count = 0;
	dual_test_state = BOARD_MOTOR_DUAL_TEST_IDLE;
	dual_a_forward_count = 0;
	dual_b_forward_count = 0;
	dual_a_reverse_count = 0;
	dual_b_reverse_count = 0;
	dual_a_reverse_started_count = 0;
	dual_b_reverse_started_count = 0;
	last_automatic_identification_ms = 0U;
	automatic_identification_next_port = 0U;
	identification_refresh_state = MOTOR_IDENTIFICATION_REFRESH_IDLE;
	identification_refresh_automatic = false;
	identification_refresh_port = BOARD_MOTOR_PORT_A;
	identification_refresh_started_ms = 0U;
	identification_refresh_float_raw = 0U;
	identification_refresh_float_mv = 0U;
	identification_refresh_tacho_count = 0;
	pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_IDLE;
	pair_position_control.left_port = BOARD_MOTOR_PORT_A;
	pair_position_control.right_port = BOARD_MOTOR_PORT_B;
	pair_position_control.synchronization_error = 0;
	pair_position_control.maximum_synchronization_error = 0;
	pair_position_control.synchronization_offset = 0;
	pair_position_control.previous_control_error = 0;
	pair_position_control.correction_percent = 0;
	pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_position_control.last_sync_ms = 0U;
	pair_position_control.offset_release_samples = 0U;
	pair_speed_control.state = BOARD_MOTOR_PAIR_SPEED_IDLE;
	pair_speed_control.left_port = BOARD_MOTOR_PORT_A;
	pair_speed_control.right_port = BOARD_MOTOR_PORT_B;
	pair_speed_control.left_start_count = 0;
	pair_speed_control.right_start_count = 0;
	pair_speed_control.synchronization_error = 0;
	pair_speed_control.maximum_synchronization_error = 0;
	pair_speed_control.synchronization_offset = 0;
	pair_speed_control.previous_control_error = 0;
	pair_speed_control.correction_percent = 0;
	pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.left_requested_speed = 0;
	pair_speed_control.right_requested_speed = 0;
	pair_speed_control.started_ms = 0U;
	pair_speed_control.duration_ms = 0U;
	pair_speed_control.elapsed_ms = 0U;
	pair_speed_control.last_sync_ms = 0U;
	pair_speed_control.offset_release_samples = 0U;
	pair_speed_control.completion_stop_mode =
		BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
	last_position_port = BOARD_MOTOR_PORT_A;
	last_speed_port = BOARD_MOTOR_PORT_A;
}

bool board_motor_diagnostic_active(void)
{
	return legacy_diagnostic_active() || any_position_control_active() ||
		any_speed_control_active() || any_hold_control_active();
}

bool board_motor_set_power(enum board_motor_port port, int8_t power_percent)
{
	int16_t magnitude = power_percent;
	uint32_t compare;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) || power_percent < -100 ||
	    power_percent > 100 || legacy_diagnostic_active()) {
		return false;
	}
	cancel_hold_control(port, false);
	if (port_control_active(port)) {
		return false;
	}
	if (power_percent == 0) {
		motor_output_off(port);
		return true;
	}
	if (magnitude < 0) {
		magnitude = -magnitude;
	}
	compare = 100U - (uint32_t)magnitude;
	if (power_percent > 0) {
		motor_output_forward(port, compare, power_percent);
	} else {
		motor_output_reverse(port, compare, power_percent);
	}
	return true;
}

bool board_motor_coast(enum board_motor_port port)
{
	cancel_automatic_identification_refresh();
	if (motor_port_valid(port) && pair_position_owns_port(port)) {
		return board_motor_stop_pair_position(
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
	}
	if (motor_port_valid(port) && pair_speed_owns_port(port)) {
		return board_motor_stop_pair_speed(
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
	}
	if (!motor_port_valid(port) || legacy_diagnostic_active()) {
		return false;
	}
	if (hold_control_active(port)) {
		cancel_hold_control(port, true);
		return true;
	}
	if (
	    port_control_active(port)) {
		return false;
	}
	motor_output_off(port);
	return true;
}

bool board_motor_brake(enum board_motor_port port)
{
	cancel_automatic_identification_refresh();
	if (motor_port_valid(port) && pair_position_owns_port(port)) {
		return board_motor_stop_pair_position(
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
	}
	if (motor_port_valid(port) && pair_speed_owns_port(port)) {
		return board_motor_stop_pair_speed(
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
	}
	if (!motor_port_valid(port) || legacy_diagnostic_active()) {
		return false;
	}
	if (hold_control_active(port)) {
		cancel_hold_control(port, false);
		motor_output_high_push_pull_stop(port);
		return true;
	}
	if (
	    port_control_active(port)) {
		return false;
	}
	motor_output_high_push_pull_stop(port);
	return true;
}

bool board_motor_reset_tacho(enum board_motor_port port)
{
	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) || legacy_diagnostic_active() ||
	    port_control_active(port)) {
		return false;
	}
	tacho_count[(uint8_t)port] = 0;
	return true;
}

bool board_motor_control_snapshot(enum board_motor_port port,
	struct board_motor_control_snapshot *snapshot)
{
	uint8_t port_index = (uint8_t)port;

	if (!motor_port_valid(port) || snapshot == NULL) {
		return false;
	}
	snapshot->state = output_state[port_index];
	snapshot->type = motor_type[port_index];
	snapshot->power_percent = output_power[port_index];
	if (identification_refresh_state != MOTOR_IDENTIFICATION_REFRESH_IDLE &&
	    port == identification_refresh_port) {
		snapshot->speed_percent = 0;
		snapshot->tacho_count = identification_refresh_tacho_count;
	} else {
		snapshot->speed_percent = measured_speed_percent[port_index];
		snapshot->tacho_count = tacho_count[port_index];
	}
	snapshot->id_adc_raw = motor_id_adc_raw[port_index];
	snapshot->id_mv = motor_id_mv[port_index];
	snapshot->id_pin6_low_adc_raw =
		motor_id_pin6_low_adc_raw[port_index];
	snapshot->id_pin6_low_mv = motor_id_pin6_low_mv[port_index];
	snapshot->id_pin5_pullup_adc_raw =
		motor_id_pin5_pullup_adc_raw[port_index];
	snapshot->id_pin5_pullup_mv = motor_id_pin5_pullup_mv[port_index];
	snapshot->id_pin5_pullup_high = motor_id_pin5_pullup_high[port_index];
	return true;
}

bool board_motor_connection_present(enum board_motor_port port,
	bool *connected)
{
	const struct motor_port_config *config;
	int16_t power;
	uint32_t drive_compare = MOTOR_PWM_OFF_COMPARE;
	bool drive_paused = false;
	uint16_t raw;
	uint16_t millivolts;

	if (!motor_port_valid(port) || connected == NULL) {
		return false;
	}
	config = motor_config(port);
	power = output_power[(uint8_t)port];
	if (output_state[(uint8_t)port] == BOARD_MOTOR_OUTPUT_DRIVE &&
	    power != 0) {
		int16_t magnitude = power < 0 ? -power : power;

		drive_compare = 100U - (uint32_t)magnitude;
		timer_set_oc_value(TIM4, config->pwm_channel,
			MOTOR_PWM_OFF_COMPARE);
		timer_generate_event(TIM4, TIM_EGR_UG);
		drive_paused = true;
	}
	raw = read_motor_id_adc(config);
	if (drive_paused) {
		timer_set_oc_value(TIM4, config->pwm_channel, drive_compare);
		timer_generate_event(TIM4, TIM_EGR_UG);
	}
	millivolts = (uint16_t)(((uint32_t)raw * MOTOR_ID_SCALE_MV) /
		MOTOR_ID_ADC_FULL_SCALE);
	*connected = motor_type_from_mv(millivolts) != BOARD_MOTOR_TYPE_NONE;
	return true;
}

bool board_motor_refresh_identification(uint32_t now_ms,
	enum board_motor_port port)
{
	uint8_t check_index;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) || board_motor_diagnostic_active()) {
		return false;
	}
	for (check_index = 0U; check_index < BOARD_MOTOR_PORT_COUNT;
	     check_index++) {
		if (output_state[check_index] != BOARD_MOTOR_OUTPUT_COAST ||
		    output_power[check_index] != 0) {
			return false;
		}
	}
	/* Official sequence samples pin 5 with pin 6 floating, then low; motor bridge stays off. */
	start_identification_refresh(now_ms, port, false);
	return true;
}

bool board_motor_start_position(uint32_t now_ms, enum board_motor_port port,
	uint8_t speed_percent, int32_t degrees,
	enum board_motor_stop_mode stop_mode)
{
	struct motor_position_control *control;
	enum board_motor_type type;
	int32_t absolute_degrees;
	uint8_t maximum_speed;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) ||
	    speed_percent > 100U || degrees == 0 ||
	    degrees < -MOTOR_POSITION_MAX_DEGREES ||
	    degrees > MOTOR_POSITION_MAX_DEGREES ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION) ||
	    legacy_diagnostic_active()) {
		return false;
	}
	cancel_hold_control(port, false);
	if (port_control_active(port)) {
		return false;
	}
	type = motor_type[(uint8_t)port];
	if (type != BOARD_MOTOR_TYPE_LARGE &&
	    type != BOARD_MOTOR_TYPE_MEDIUM) {
		return false;
	}
	absolute_degrees = degrees < 0 ? -degrees : degrees;
	maximum_speed = motor_position_max_speed(type, absolute_degrees);
	if (speed_percent > maximum_speed) {
		speed_percent = maximum_speed;
	}
	motor_output_off(port);
	control = &position_controls[(uint8_t)port];
	control->type = type;
	control->start_count = tacho_count[(uint8_t)port];
	control->target_count = control->start_count + degrees;
	control->requested_speed = degrees < 0 ?
		-(int8_t)speed_percent : (int8_t)speed_percent;
	control->finished_ms = 0U;
	control->last_control_ms = now_ms - MOTOR_SPEED_CONTROL_INTERVAL_MS;
	control->last_count = control->start_count;
	board_motor_pair_regulator_reset(&pair_regulators[(uint8_t)port],
		control->start_count, now_ms);
	control->pwm_x100 = 0;
	control->phase = MOTOR_POSITION_PHASE_TRACKING;
	control->stable_samples = 0U;
	control->recovery_drive_samples = 0U;
	control->recovery_cooldown_samples = 0U;
	control->recovery_direction = 0;
	control->recovery_pwm_x100 = 0;
	control->stall_candidate_ms = 0U;
	control->stall_candidate = false;
	control->stalled = false;
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	control->completion_stop_mode = stop_mode;
	speed_sample_count[(uint8_t)port] = control->start_count;
	speed_sample_ms[(uint8_t)port] = now_ms;
	measured_speed_percent[(uint8_t)port] = 0;
	speed_measurement_valid[(uint8_t)port] = false;
	control->state = BOARD_MOTOR_POSITION_RUNNING;
	last_position_port = port;
	return true;
}

bool board_motor_stop_position(enum board_motor_port port,
	enum board_motor_stop_mode stop_mode)
{
	struct motor_position_control *control;

	cancel_automatic_identification_refresh();
	if (motor_port_valid(port) && pair_position_owns_port(port)) {
		return board_motor_stop_pair_position(stop_mode);
	}
	if (!motor_port_valid(port) || legacy_diagnostic_active() ||
	    speed_control_active(port) ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION)) {
		return false;
	}
	control = &position_controls[(uint8_t)port];
	control->state = BOARD_MOTOR_POSITION_IDLE;
	control->phase = MOTOR_POSITION_PHASE_TRACKING;
	control->requested_speed = 0;
	control->pwm_x100 = 0;
	control->stable_samples = 0U;
	control->recovery_drive_samples = 0U;
	control->recovery_cooldown_samples = 0U;
	control->recovery_direction = 0;
	control->recovery_pwm_x100 = 0;
	clear_position_stall(control);
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	motor_apply_completion_stop_mode(port, stop_mode,
		tacho_count[(uint8_t)port]);
	return true;
}

bool board_motor_position_snapshot_for_port(enum board_motor_port port,
	struct board_motor_position_snapshot *snapshot)
{
	const struct motor_position_control *control;

	if (!motor_port_valid(port) || snapshot == NULL) {
		return false;
	}
	control = &position_controls[(uint8_t)port];
	snapshot->state = control->state;
	snapshot->port = port;
	snapshot->type = control->type;
	snapshot->requested_speed_percent = control->requested_speed;
	snapshot->measured_speed_percent = measured_speed_percent[(uint8_t)port];
	snapshot->start_count = control->start_count;
	snapshot->target_count = control->target_count;
	snapshot->current_count = tacho_count[(uint8_t)port];
	return true;
}

bool board_motor_position_stalled(enum board_motor_port port, bool *stalled)
{
	if (!motor_port_valid(port) || stalled == NULL) {
		return false;
	}
	*stalled = position_control_active(port) &&
		position_controls[(uint8_t)port].stalled;
	return true;
}

struct board_motor_position_snapshot board_motor_position_snapshot(void)
{
	struct board_motor_position_snapshot snapshot = {0};

	(void)board_motor_position_snapshot_for_port(last_position_port, &snapshot);
	return snapshot;
}

bool board_motor_hold_snapshot_for_port(enum board_motor_port port,
	struct board_motor_hold_snapshot *snapshot)
{
	const struct motor_hold_control *control;

	if (!motor_port_valid(port) || snapshot == NULL) {
		return false;
	}
	control = &hold_controls[(uint8_t)port];
	snapshot->state = control->state;
	snapshot->port = port;
	snapshot->type = control->type;
	snapshot->target_count = control->target_count;
	snapshot->current_count = tacho_count[(uint8_t)port];
	snapshot->power_percent = output_power[(uint8_t)port];
	return true;
}

static void start_pair_stationary_position(uint32_t now_ms,
	enum board_motor_port port, enum board_motor_type type,
	enum board_motor_stop_mode stop_mode)
{
	struct motor_position_control *control =
		&position_controls[(uint8_t)port];

	motor_output_off(port);
	control->type = type;
	control->start_count = tacho_count[(uint8_t)port];
	control->target_count = control->start_count;
	control->requested_speed = 0;
	control->finished_ms = now_ms;
	control->last_control_ms = now_ms;
	control->last_count = control->start_count;
	control->pwm_x100 = 0;
	control->phase = MOTOR_POSITION_PHASE_TRACKING;
	control->stable_samples = 0U;
	control->recovery_drive_samples = 0U;
	control->recovery_cooldown_samples = 0U;
	control->recovery_direction = 0;
	control->recovery_pwm_x100 = 0;
	control->stall_candidate_ms = 0U;
	control->stall_candidate = false;
	control->stalled = false;
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	control->completion_stop_mode = stop_mode;
	control->state = BOARD_MOTOR_POSITION_COMPLETE;
}

bool board_motor_start_pair_position(uint32_t now_ms,
	enum board_motor_port left_port, int32_t left_degrees,
	enum board_motor_port right_port, int32_t right_degrees,
	uint8_t maximum_speed_percent,
	enum board_motor_stop_mode stop_mode)
{
	int32_t left_magnitude;
	int32_t right_magnitude;
	int32_t reference_target;
	uint8_t left_speed_percent;
	uint8_t right_speed_percent;
	enum board_motor_type left_type;
	enum board_motor_type right_type;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(left_port) || !motor_port_valid(right_port) ||
	    left_port == right_port ||
	    (left_degrees == 0 && right_degrees == 0) ||
	    left_degrees < -MOTOR_POSITION_MAX_DEGREES ||
	    left_degrees > MOTOR_POSITION_MAX_DEGREES ||
	    right_degrees < -MOTOR_POSITION_MAX_DEGREES ||
	    right_degrees > MOTOR_POSITION_MAX_DEGREES ||
	    maximum_speed_percent > 100U ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION) ||
	    legacy_diagnostic_active() ||
	    pair_position_control.state ==
		BOARD_MOTOR_PAIR_POSITION_RUNNING ||
	    pair_speed_control.state == BOARD_MOTOR_PAIR_SPEED_RUNNING) {
		return false;
	}
	left_magnitude = left_degrees < 0 ? -left_degrees : left_degrees;
	right_magnitude = right_degrees < 0 ? -right_degrees : right_degrees;
	reference_target = left_magnitude > right_magnitude ?
		left_magnitude : right_magnitude;
	left_speed_percent = pair_position_scaled_max_speed(
		maximum_speed_percent, left_magnitude, reference_target);
	right_speed_percent = pair_position_scaled_max_speed(
		maximum_speed_percent, right_magnitude, reference_target);
	left_type = motor_type[(uint8_t)left_port];
	right_type = motor_type[(uint8_t)right_port];
	if ((left_type != BOARD_MOTOR_TYPE_LARGE &&
	     left_type != BOARD_MOTOR_TYPE_MEDIUM) ||
	    (right_type != BOARD_MOTOR_TYPE_LARGE &&
	     right_type != BOARD_MOTOR_TYPE_MEDIUM)) {
		return false;
	}
	cancel_hold_control(left_port, false);
	cancel_hold_control(right_port, false);
	if (port_control_active(left_port) || port_control_active(right_port)) {
		return false;
	}
	if (left_degrees == 0) {
		start_pair_stationary_position(now_ms, left_port, left_type,
			stop_mode);
	} else if (!board_motor_start_position(now_ms, left_port,
	    left_speed_percent, left_degrees, stop_mode)) {
		return false;
	}
	if (right_degrees == 0) {
		start_pair_stationary_position(now_ms, right_port, right_type,
			stop_mode);
	} else if (!board_motor_start_position(now_ms, right_port,
	    right_speed_percent, right_degrees, stop_mode)) {
		if (left_degrees != 0) {
			(void)board_motor_stop_position(left_port,
				BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
		} else {
			motor_output_off(left_port);
		}
		return false;
	}
	pair_speed_control.state = BOARD_MOTOR_PAIR_SPEED_IDLE;
	pair_speed_control.correction_percent = 0;
	pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.left_requested_speed = 0;
	pair_speed_control.right_requested_speed = 0;
	pair_speed_control.duration_ms = 0U;
	pair_speed_control.elapsed_ms = 0U;
	pair_position_control.left_port = left_port;
	pair_position_control.right_port = right_port;
	pair_position_control.synchronization_error = 0;
	pair_position_control.maximum_synchronization_error = 0;
	pair_position_control.synchronization_offset = 0;
	pair_position_control.previous_control_error = 0;
	pair_position_control.correction_percent = 0;
	pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_position_control.last_sync_ms =
		now_ms - MOTOR_SPEED_CONTROL_INTERVAL_MS;
	pair_position_control.offset_release_samples = 0U;
	pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_RUNNING;
	return true;
}

bool board_motor_stop_pair_position(enum board_motor_stop_mode stop_mode)
{
	struct motor_position_control *left;
	struct motor_position_control *right;

	cancel_automatic_identification_refresh();
	if (legacy_diagnostic_active() ||
	    pair_position_control.state == BOARD_MOTOR_PAIR_POSITION_IDLE ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION)) {
		return false;
	}
	left = &position_controls[(uint8_t)pair_position_control.left_port];
	right = &position_controls[(uint8_t)pair_position_control.right_port];
	left->state = BOARD_MOTOR_POSITION_IDLE;
	left->phase = MOTOR_POSITION_PHASE_TRACKING;
	left->requested_speed = 0;
	left->pwm_x100 = 0;
	left->stable_samples = 0U;
	left->recovery_drive_samples = 0U;
	left->recovery_cooldown_samples = 0U;
	left->recovery_direction = 0;
	left->recovery_pwm_x100 = 0;
	left->pair_recovery_samples = 0U;
	left->pair_recovery_pwm_limit_x100 = 0;
	right->state = BOARD_MOTOR_POSITION_IDLE;
	right->phase = MOTOR_POSITION_PHASE_TRACKING;
	right->requested_speed = 0;
	right->pwm_x100 = 0;
	right->stable_samples = 0U;
	right->recovery_drive_samples = 0U;
	right->recovery_cooldown_samples = 0U;
	right->recovery_direction = 0;
	right->recovery_pwm_x100 = 0;
	right->pair_recovery_samples = 0U;
	right->pair_recovery_pwm_limit_x100 = 0;
	motor_apply_completion_stop_mode(pair_position_control.left_port,
		stop_mode,
		tacho_count[(uint8_t)pair_position_control.left_port]);
	motor_apply_completion_stop_mode(pair_position_control.right_port,
		stop_mode,
		tacho_count[(uint8_t)pair_position_control.right_port]);
	pair_position_control.correction_percent = 0;
	pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_position_control.synchronization_offset = 0;
	pair_position_control.previous_control_error = 0;
	pair_position_control.offset_release_samples = 0U;
	pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_IDLE;
	return true;
}

struct board_motor_pair_position_snapshot
	board_motor_pair_position_snapshot(void)
{
	struct board_motor_pair_position_snapshot snapshot;
	const struct motor_position_control *left =
		&position_controls[(uint8_t)pair_position_control.left_port];
	const struct motor_position_control *right =
		&position_controls[(uint8_t)pair_position_control.right_port];

	snapshot.state = pair_position_control.state;
	snapshot.left_port = pair_position_control.left_port;
	snapshot.right_port = pair_position_control.right_port;
	snapshot.left_start_count = left->start_count;
	snapshot.left_target_count = left->target_count;
	snapshot.left_current_count =
		tacho_count[(uint8_t)pair_position_control.left_port];
	snapshot.right_start_count = right->start_count;
	snapshot.right_target_count = right->target_count;
	snapshot.right_current_count =
		tacho_count[(uint8_t)pair_position_control.right_port];
	snapshot.synchronization_error_count =
		pair_position_control.synchronization_error;
	snapshot.maximum_synchronization_error_count =
		pair_position_control.maximum_synchronization_error;
	return snapshot;
}

bool board_motor_start_speed(uint32_t now_ms, enum board_motor_port port,
	int8_t speed_percent)
{
	struct motor_speed_control *control;
	enum board_motor_type type;
	uint8_t magnitude;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) || speed_percent < -100 ||
	    speed_percent > 100 || legacy_diagnostic_active()) {
		return false;
	}
	cancel_hold_control(port, false);
	magnitude = speed_percent < 0 ?
		(uint8_t)(-(int16_t)speed_percent) : (uint8_t)speed_percent;
	if (magnitude != 0U && magnitude < MOTOR_SPEED_MIN_PERCENT) {
		return false;
	}
	type = motor_type[(uint8_t)port];
	if (type != BOARD_MOTOR_TYPE_LARGE &&
	    type != BOARD_MOTOR_TYPE_MEDIUM) {
		return false;
	}
	control = &speed_controls[(uint8_t)port];
	if (speed_control_active(port)) {
		if (control->duration_ms != 0U || control->type != type ||
		    position_control_active(port) || pair_position_owns_port(port) ||
		    pair_speed_owns_port(port)) {
			return false;
		}
		control->requested_speed = speed_percent;
		last_speed_port = port;
		return true;
	}
	if (port_control_active(port) || pair_speed_owns_port(port)) {
		return false;
	}
	motor_output_off(port);
	control->state = BOARD_MOTOR_SPEED_RUNNING;
	control->type = type;
	control->requested_speed = speed_percent;
	control->started_ms = now_ms;
	control->duration_ms = 0U;
	control->elapsed_ms = 0U;
	control->completion_stop_mode = BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
	control->last_control_ms = now_ms - MOTOR_SPEED_CONTROL_INTERVAL_MS;
	board_motor_pair_regulator_reset(&pair_regulators[(uint8_t)port],
		tacho_count[(uint8_t)port], now_ms);
	control->pwm_x100 = 0;
	control->pair_last_count = tacho_count[(uint8_t)port];
	control->pair_tick_progress = 0;
	control->pair_stall_samples = 0U;
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	control->pair_stalled = false;
	speed_sample_count[(uint8_t)port] = tacho_count[(uint8_t)port];
	speed_sample_ms[(uint8_t)port] = now_ms;
	measured_speed_percent[(uint8_t)port] = 0;
	speed_measurement_valid[(uint8_t)port] = false;
	last_speed_port = port;
	return true;
}

bool board_motor_start_speed_for_time(uint32_t now_ms,
	enum board_motor_port port, int8_t speed_percent, uint32_t duration_ms,
	enum board_motor_stop_mode stop_mode)
{
	struct motor_speed_control *control;

	if (!motor_port_valid(port) || duration_ms == 0U ||
	    duration_ms > MOTOR_TIMED_MAX_DURATION_MS ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION) ||
	    port_control_active(port) || pair_speed_owns_port(port)) {
		return false;
	}
	if (!board_motor_start_speed(now_ms, port, speed_percent)) {
		return false;
	}
	control = &speed_controls[(uint8_t)port];
	control->duration_ms = duration_ms;
	control->completion_stop_mode = stop_mode;
	return true;
}

bool board_motor_stop_speed(enum board_motor_port port,
	enum board_motor_stop_mode stop_mode)
{
	struct motor_speed_control *control;

	cancel_automatic_identification_refresh();
	if (motor_port_valid(port) && pair_speed_owns_port(port)) {
		return board_motor_stop_pair_speed(stop_mode);
	}
	if (!motor_port_valid(port) || legacy_diagnostic_active() ||
	    position_control_active(port) ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION)) {
		return false;
	}
	control = &speed_controls[(uint8_t)port];
	control->state = BOARD_MOTOR_SPEED_IDLE;
	control->requested_speed = 0;
	control->started_ms = 0U;
	control->duration_ms = 0U;
	control->elapsed_ms = 0U;
	control->pwm_x100 = 0;
	control->pair_tick_progress = 0;
	control->pair_stall_samples = 0U;
	control->pair_recovery_samples = 0U;
	control->pair_recovery_pwm_limit_x100 = 0;
	control->pair_stalled = false;
	motor_apply_completion_stop_mode(port, stop_mode,
		tacho_count[(uint8_t)port]);
	return true;
}

bool board_motor_speed_snapshot_for_port(enum board_motor_port port,
	struct board_motor_speed_snapshot *snapshot)
{
	const struct motor_speed_control *control;
	uint8_t port_index;

	if (!motor_port_valid(port) || snapshot == NULL) {
		return false;
	}
	port_index = (uint8_t)port;
	control = &speed_controls[port_index];
	snapshot->state = control->state;
	snapshot->port = port;
	snapshot->output_state = output_state[port_index];
	snapshot->type = control->type;
	snapshot->requested_speed_percent = control->requested_speed;
	snapshot->measured_speed_percent = measured_speed_percent[port_index];
	snapshot->power_percent = output_power[port_index];
	snapshot->tacho_count = tacho_count[port_index];
	snapshot->duration_ms = control->duration_ms;
	snapshot->elapsed_ms = control->elapsed_ms;
	return true;
}

struct board_motor_speed_snapshot board_motor_speed_snapshot(void)
{
	struct board_motor_speed_snapshot snapshot = {0};

	(void)board_motor_speed_snapshot_for_port(last_speed_port, &snapshot);
	return snapshot;
}

static bool start_pair_speed(uint32_t now_ms,
	enum board_motor_port left_port, int8_t left_speed_percent,
	enum board_motor_port right_port, int8_t right_speed_percent,
	uint32_t duration_ms, enum board_motor_stop_mode completion_stop_mode)
{
	uint8_t left_magnitude;
	uint8_t right_magnitude;

	cancel_automatic_identification_refresh();
	if (!motor_port_valid(left_port) || !motor_port_valid(right_port) ||
	    left_port == right_port || left_speed_percent < -100 ||
	    left_speed_percent > 100 || right_speed_percent < -100 ||
	    right_speed_percent > 100 || legacy_diagnostic_active() ||
	    duration_ms > MOTOR_PAIR_TIMED_MAX_DURATION_MS ||
	    (completion_stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     completion_stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     completion_stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION) ||
	    pair_position_control.state == BOARD_MOTOR_PAIR_POSITION_RUNNING) {
		return false;
	}
	left_magnitude = left_speed_percent < 0 ?
		(uint8_t)(-(int16_t)left_speed_percent) :
		(uint8_t)left_speed_percent;
	right_magnitude = right_speed_percent < 0 ?
		(uint8_t)(-(int16_t)right_speed_percent) :
		(uint8_t)right_speed_percent;
	if ((left_magnitude != 0U &&
	     left_magnitude < MOTOR_SPEED_MIN_PERCENT) ||
	    (right_magnitude != 0U &&
	     right_magnitude < MOTOR_SPEED_MIN_PERCENT)) {
		return false;
	}
	if (pair_speed_control.state == BOARD_MOTOR_PAIR_SPEED_RUNNING) {
		if (duration_ms != 0U || pair_speed_control.duration_ms != 0U ||
		    pair_speed_control.left_port != left_port ||
		    pair_speed_control.right_port != right_port ||
		    !speed_control_active(left_port) ||
		    !speed_control_active(right_port)) {
			return false;
		}
		speed_controls[(uint8_t)left_port].requested_speed =
			left_speed_percent;
		speed_controls[(uint8_t)right_port].requested_speed =
			right_speed_percent;
		speed_controls[(uint8_t)left_port].pair_last_count =
			tacho_count[(uint8_t)left_port];
		speed_controls[(uint8_t)right_port].pair_last_count =
			tacho_count[(uint8_t)right_port];
		speed_controls[(uint8_t)left_port].pair_tick_progress = 0;
		speed_controls[(uint8_t)right_port].pair_tick_progress = 0;
		speed_controls[(uint8_t)left_port].pair_stall_samples = 0U;
		speed_controls[(uint8_t)right_port].pair_stall_samples = 0U;
		speed_controls[(uint8_t)left_port].pair_recovery_samples = 0U;
		speed_controls[(uint8_t)right_port].pair_recovery_samples = 0U;
		speed_controls[(uint8_t)left_port].pair_recovery_pwm_limit_x100 = 0;
		speed_controls[(uint8_t)right_port].pair_recovery_pwm_limit_x100 = 0;
		speed_controls[(uint8_t)left_port].pair_stalled = false;
		speed_controls[(uint8_t)right_port].pair_stalled = false;
		pair_speed_control.left_requested_speed = left_speed_percent;
		pair_speed_control.right_requested_speed = right_speed_percent;
		pair_speed_control.left_start_count = tacho_count[(uint8_t)left_port];
		pair_speed_control.right_start_count = tacho_count[(uint8_t)right_port];
		pair_speed_control.synchronization_error = 0;
		pair_speed_control.maximum_synchronization_error = 0;
		pair_speed_control.synchronization_offset = 0;
		pair_speed_control.previous_control_error = 0;
		pair_speed_control.correction_percent = 0;
		pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
		pair_speed_control.last_sync_ms =
			now_ms - MOTOR_SPEED_CONTROL_INTERVAL_MS;
		pair_speed_control.offset_release_samples = 0U;
		return true;
	}
	cancel_hold_control(left_port, false);
	cancel_hold_control(right_port, false);
	if (port_control_active(left_port) || port_control_active(right_port)) {
		return false;
	}
	if (!board_motor_start_speed(now_ms, left_port, left_speed_percent)) {
		return false;
	}
	if (!board_motor_start_speed(now_ms, right_port, right_speed_percent)) {
		(void)board_motor_stop_speed(left_port,
			BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
		return false;
	}
	pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_IDLE;
	pair_position_control.correction_percent = 0;
	pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.left_port = left_port;
	pair_speed_control.right_port = right_port;
	pair_speed_control.left_start_count = tacho_count[(uint8_t)left_port];
	pair_speed_control.right_start_count = tacho_count[(uint8_t)right_port];
	pair_speed_control.synchronization_error = 0;
	pair_speed_control.maximum_synchronization_error = 0;
	pair_speed_control.synchronization_offset = 0;
	pair_speed_control.previous_control_error = 0;
	pair_speed_control.correction_percent = 0;
	pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.left_requested_speed = left_speed_percent;
	pair_speed_control.right_requested_speed = right_speed_percent;
	pair_speed_control.started_ms = now_ms;
	pair_speed_control.duration_ms = duration_ms;
	pair_speed_control.elapsed_ms = 0U;
	pair_speed_control.last_sync_ms =
		now_ms - MOTOR_SPEED_CONTROL_INTERVAL_MS;
	pair_speed_control.offset_release_samples = 0U;
	pair_speed_control.completion_stop_mode = completion_stop_mode;
	pair_speed_control.state = BOARD_MOTOR_PAIR_SPEED_RUNNING;
	return true;
}

bool board_motor_start_pair_speed(uint32_t now_ms,
	enum board_motor_port left_port, int8_t left_speed_percent,
	enum board_motor_port right_port, int8_t right_speed_percent)
{
	return start_pair_speed(now_ms, left_port, left_speed_percent,
		right_port, right_speed_percent, 0U,
		BOARD_MOTOR_STOP_LOW_OPEN_DRAIN);
}

bool board_motor_start_pair_speed_for_time(uint32_t now_ms,
	enum board_motor_port left_port, int8_t left_speed_percent,
	enum board_motor_port right_port, int8_t right_speed_percent,
	uint32_t duration_ms, enum board_motor_stop_mode stop_mode)
{
	if (duration_ms == 0U) {
		return false;
	}
	return start_pair_speed(now_ms, left_port, left_speed_percent,
		right_port, right_speed_percent, duration_ms, stop_mode);
}

bool board_motor_stop_pair_speed(enum board_motor_stop_mode stop_mode)
{
	cancel_automatic_identification_refresh();
	if (legacy_diagnostic_active() ||
	    (pair_speed_control.state != BOARD_MOTOR_PAIR_SPEED_RUNNING &&
	     pair_speed_control.state != BOARD_MOTOR_PAIR_SPEED_COMPLETE) ||
	    (stop_mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     stop_mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL &&
	     stop_mode != BOARD_MOTOR_STOP_HOLD_POSITION)) {
		return false;
	}
	finish_pair_speed(stop_mode, BOARD_MOTOR_PAIR_SPEED_IDLE);
	return true;
}

struct board_motor_pair_speed_snapshot board_motor_pair_speed_snapshot(void)
{
	struct board_motor_pair_speed_snapshot snapshot = {0};
	uint8_t left_index = (uint8_t)pair_speed_control.left_port;
	uint8_t right_index = (uint8_t)pair_speed_control.right_port;

	snapshot.state = pair_speed_control.state;
	snapshot.left_port = pair_speed_control.left_port;
	snapshot.right_port = pair_speed_control.right_port;
	snapshot.left_requested_speed_percent =
		pair_speed_control.left_requested_speed;
	snapshot.right_requested_speed_percent =
		pair_speed_control.right_requested_speed;
	snapshot.left_measured_speed_percent = measured_speed_percent[left_index];
	snapshot.right_measured_speed_percent = measured_speed_percent[right_index];
	snapshot.left_power_percent = output_power[left_index];
	snapshot.right_power_percent = output_power[right_index];
	snapshot.left_start_count = pair_speed_control.left_start_count;
	snapshot.left_current_count = tacho_count[left_index];
	snapshot.right_start_count = pair_speed_control.right_start_count;
	snapshot.right_current_count = tacho_count[right_index];
	snapshot.synchronization_error_count =
		pair_speed_control.synchronization_error;
	snapshot.maximum_synchronization_error_count =
		pair_speed_control.maximum_synchronization_error;
	snapshot.duration_ms = pair_speed_control.duration_ms;
	snapshot.elapsed_ms = pair_speed_control.elapsed_ms;
	return snapshot;
}

bool board_motor_start_test(uint32_t now_ms)
{
	return board_motor_start_port_test_with_power(now_ms, BOARD_MOTOR_PORT_A,
		MOTOR_TEST_POWER_PERCENT);
}

bool board_motor_start_test_with_power(uint32_t now_ms, uint8_t power_percent)
{
	return board_motor_start_port_test_with_power(now_ms, BOARD_MOTOR_PORT_A,
		power_percent);
}

bool board_motor_start_port_test_with_power(uint32_t now_ms,
	enum board_motor_port port, uint8_t power_percent)
{
	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) || power_percent == 0U ||
	    power_percent > 100U || board_motor_diagnostic_active()) {
		return false;
	}
	if (normal_test_active() || stop_test_active() || dual_test_active()) {
		return false;
	}
	motor_output_off_all();
	test_port = port;
	tacho_count[(uint8_t)test_port] = 0;
	forward_count = 0;
	reverse_count = 0;
	reverse_started_count = 0;
	stop_test_state = BOARD_MOTOR_STOP_TEST_IDLE;
	test_compare = 100U - power_percent;
	if (power_percent >= MOTOR_HIGH_POWER_THRESHOLD_PERCENT) {
		test_run_ms = MOTOR_HIGH_POWER_RUN_MS;
		test_pause_ms = MOTOR_HIGH_POWER_PAUSE_MS;
	} else {
		test_run_ms = MOTOR_TEST_RUN_MS;
		test_pause_ms = MOTOR_TEST_PAUSE_MS;
	}
	motor_output_forward(test_port, test_compare,
		(int8_t)(100U - test_compare));
	test_state = BOARD_MOTOR_TEST_FORWARD;
	phase_started_ms = now_ms;
	return true;
}

bool board_motor_start_stop_test(uint32_t now_ms,
	enum board_motor_stop_mode mode, uint8_t power_percent)
{
	return board_motor_start_port_stop_test(now_ms, BOARD_MOTOR_PORT_A, mode,
		power_percent);
}

bool board_motor_start_port_stop_test(uint32_t now_ms,
	enum board_motor_port port, enum board_motor_stop_mode mode,
	uint8_t power_percent)
{
	cancel_automatic_identification_refresh();
	if (!motor_port_valid(port) ||
	    (mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL) ||
	    power_percent == 0U || power_percent > 100U ||
	    board_motor_diagnostic_active()) {
		return false;
	}
	if (normal_test_active() || stop_test_active() || dual_test_active()) {
		return false;
	}

	motor_output_off_all();
	test_state = BOARD_MOTOR_TEST_IDLE;
	test_port = port;
	tacho_count[(uint8_t)test_port] = 0;
	stop_test_mode = mode;
	stop_test_powered_count = 0;
	stop_test_started_count = 0;
	stop_test_stopped_count = 0;
	test_compare = 100U - power_percent;
	motor_output_forward(test_port, test_compare,
		(int8_t)(100U - test_compare));
	stop_test_state = BOARD_MOTOR_STOP_TEST_DRIVE;
	phase_started_ms = now_ms;
	return true;
}

bool board_motor_start_dual_test(uint32_t now_ms, uint8_t power_percent)
{
	cancel_automatic_identification_refresh();
	if (power_percent == 0U || power_percent > 100U ||
	    board_motor_diagnostic_active()) {
		return false;
	}

	motor_output_off_all();
	test_state = BOARD_MOTOR_TEST_IDLE;
	stop_test_state = BOARD_MOTOR_STOP_TEST_IDLE;
	tacho_count[BOARD_MOTOR_PORT_A] = 0;
	tacho_count[BOARD_MOTOR_PORT_B] = 0;
	dual_a_forward_count = 0;
	dual_b_forward_count = 0;
	dual_a_reverse_count = 0;
	dual_b_reverse_count = 0;
	dual_a_reverse_started_count = 0;
	dual_b_reverse_started_count = 0;
	test_compare = 100U - power_percent;
	motor_output_forward(BOARD_MOTOR_PORT_A, test_compare,
		(int8_t)(100U - test_compare));
	motor_output_forward(BOARD_MOTOR_PORT_B, test_compare,
		(int8_t)(100U - test_compare));
	dual_test_state = BOARD_MOTOR_DUAL_TEST_FORWARD;
	phase_started_ms = now_ms;
	return true;
}

void board_motor_stop(void)
{
	uint8_t port_index;

	cancel_identification_refresh();
	motor_output_off_all();
	test_state = BOARD_MOTOR_TEST_IDLE;
	stop_test_state = BOARD_MOTOR_STOP_TEST_IDLE;
	dual_test_state = BOARD_MOTOR_DUAL_TEST_IDLE;
	pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_IDLE;
	pair_position_control.correction_percent = 0;
	pair_position_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.state = BOARD_MOTOR_PAIR_SPEED_IDLE;
	pair_speed_control.correction_percent = 0;
	pair_speed_control.leader_port = BOARD_MOTOR_PORT_COUNT;
	pair_speed_control.left_requested_speed = 0;
	pair_speed_control.right_requested_speed = 0;
	pair_speed_control.duration_ms = 0U;
	pair_speed_control.elapsed_ms = 0U;
	pair_position_control.synchronization_offset = 0;
	pair_position_control.previous_control_error = 0;
	pair_speed_control.synchronization_offset = 0;
	pair_speed_control.previous_control_error = 0;
	for (port_index = 0U; port_index < BOARD_MOTOR_PORT_COUNT; port_index++) {
		board_motor_pair_regulator_reset(&pair_regulators[port_index],
			tacho_count[port_index], 0U);
		position_controls[port_index].state = BOARD_MOTOR_POSITION_IDLE;
		position_controls[port_index].phase = MOTOR_POSITION_PHASE_TRACKING;
		position_controls[port_index].requested_speed = 0;
		position_controls[port_index].pwm_x100 = 0;
		position_controls[port_index].stable_samples = 0U;
		position_controls[port_index].recovery_drive_samples = 0U;
		position_controls[port_index].recovery_cooldown_samples = 0U;
		position_controls[port_index].recovery_direction = 0;
		position_controls[port_index].recovery_pwm_x100 = 0;
		position_controls[port_index].stall_candidate_ms = 0U;
		position_controls[port_index].stall_candidate = false;
		position_controls[port_index].stalled = false;
		position_controls[port_index].pair_recovery_samples = 0U;
		position_controls[port_index].pair_recovery_pwm_limit_x100 = 0;
		speed_controls[port_index].state = BOARD_MOTOR_SPEED_IDLE;
		speed_controls[port_index].requested_speed = 0;
		speed_controls[port_index].started_ms = 0U;
		speed_controls[port_index].duration_ms = 0U;
		speed_controls[port_index].elapsed_ms = 0U;
		speed_controls[port_index].pwm_x100 = 0;
		speed_controls[port_index].pair_stall_samples = 0U;
		speed_controls[port_index].pair_stalled = false;
		speed_controls[port_index].pair_recovery_samples = 0U;
		speed_controls[port_index].pair_recovery_pwm_limit_x100 = 0;
		hold_controls[port_index].state = BOARD_MOTOR_HOLD_IDLE;
		hold_controls[port_index].pwm_x100 = 0;
		hold_controls[port_index].disconnect_samples = 0U;
		hold_controls[port_index].settled = false;
	}
}

void board_motor_tick(uint32_t now_ms)
{
	uint32_t elapsed_ms = now_ms - phase_started_ms;

	update_motor_identification(now_ms);
	update_motor_speed(now_ms);
	update_pair_speed_correction(now_ms);
	update_speed_control(now_ms);
	update_timed_speed_state(now_ms);
	update_pair_speed_state(now_ms);
	update_pair_position_correction(now_ms);
	update_position_control(now_ms);
	update_pair_position_state();
	update_hold_control(now_ms);

	if (dual_test_state == BOARD_MOTOR_DUAL_TEST_FORWARD &&
	    elapsed_ms >= MOTOR_DUAL_TEST_RUN_MS) {
		dual_a_forward_count = tacho_count[BOARD_MOTOR_PORT_A];
		dual_b_forward_count = tacho_count[BOARD_MOTOR_PORT_B];
		motor_apply_stop_mode(BOARD_MOTOR_PORT_A,
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
		motor_apply_stop_mode(BOARD_MOTOR_PORT_B,
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
		dual_test_state = BOARD_MOTOR_DUAL_TEST_BRAKE_PAUSE;
		phase_started_ms = now_ms;
		return;
	}
	if (dual_test_state == BOARD_MOTOR_DUAL_TEST_BRAKE_PAUSE &&
	    elapsed_ms >= MOTOR_DUAL_TEST_BRAKE_MS) {
		dual_a_reverse_started_count = tacho_count[BOARD_MOTOR_PORT_A];
		dual_b_reverse_started_count = tacho_count[BOARD_MOTOR_PORT_B];
		motor_output_reverse(BOARD_MOTOR_PORT_A, test_compare,
			-(int8_t)(100U - test_compare));
		motor_output_reverse(BOARD_MOTOR_PORT_B, test_compare,
			-(int8_t)(100U - test_compare));
		dual_test_state = BOARD_MOTOR_DUAL_TEST_REVERSE;
		phase_started_ms = now_ms;
		return;
	}
	if (dual_test_state == BOARD_MOTOR_DUAL_TEST_REVERSE &&
	    elapsed_ms >= MOTOR_DUAL_TEST_RUN_MS) {
		dual_a_reverse_count = tacho_count[BOARD_MOTOR_PORT_A] -
			dual_a_reverse_started_count;
		dual_b_reverse_count = tacho_count[BOARD_MOTOR_PORT_B] -
			dual_b_reverse_started_count;
		motor_apply_stop_mode(BOARD_MOTOR_PORT_A,
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
		motor_apply_stop_mode(BOARD_MOTOR_PORT_B,
			BOARD_MOTOR_STOP_HIGH_PUSH_PULL);
		dual_test_state = BOARD_MOTOR_DUAL_TEST_FINAL_BRAKE;
		phase_started_ms = now_ms;
		return;
	}
	if (dual_test_state == BOARD_MOTOR_DUAL_TEST_FINAL_BRAKE &&
	    elapsed_ms >= MOTOR_DUAL_TEST_BRAKE_MS) {
		motor_output_off_all();
		dual_test_state = BOARD_MOTOR_DUAL_TEST_COMPLETE;
		phase_started_ms = now_ms;
		return;
	}

	if (stop_test_state == BOARD_MOTOR_STOP_TEST_DRIVE &&
	    elapsed_ms >= MOTOR_STOP_TEST_RUN_MS) {
		stop_test_powered_count = current_tacho_count();
		stop_test_started_count = current_tacho_count();
		motor_apply_stop_mode(test_port, stop_test_mode);
		stop_test_state = BOARD_MOTOR_STOP_TEST_MEASURE;
		phase_started_ms = now_ms;
		return;
	}
	if (stop_test_state == BOARD_MOTOR_STOP_TEST_MEASURE &&
	    elapsed_ms >= MOTOR_STOP_TEST_MEASURE_MS) {
		stop_test_stopped_count =
			current_tacho_count() - stop_test_started_count;
		/* Always finish with every port in the verified free-coast state. */
		motor_output_off_all();
		stop_test_state = BOARD_MOTOR_STOP_TEST_COMPLETE;
		phase_started_ms = now_ms;
		return;
	}

	if (test_state == BOARD_MOTOR_TEST_FORWARD &&
	    elapsed_ms >= test_run_ms) {
		motor_output_off(test_port);
		forward_count = current_tacho_count();
		test_state = BOARD_MOTOR_TEST_PAUSE;
		phase_started_ms = now_ms;
	} else if (test_state == BOARD_MOTOR_TEST_PAUSE &&
		   elapsed_ms >= test_pause_ms) {
		reverse_started_count = current_tacho_count();
		motor_output_reverse(test_port, test_compare,
			-(int8_t)(100U - test_compare));
		test_state = BOARD_MOTOR_TEST_REVERSE;
		phase_started_ms = now_ms;
	} else if (test_state == BOARD_MOTOR_TEST_REVERSE &&
		   elapsed_ms >= test_run_ms) {
		motor_output_off(test_port);
		reverse_count = current_tacho_count() - reverse_started_count;
		test_state = BOARD_MOTOR_TEST_COMPLETE;
		phase_started_ms = now_ms;
	}
}

enum board_motor_test_state board_motor_test_state(void)
{
	return test_state;
}

struct board_motor_tacho_snapshot board_motor_tacho_snapshot(void)
{
	struct board_motor_tacho_snapshot snapshot;
	int32_t count = current_tacho_count();

	snapshot.total_count = count;
	snapshot.forward_count = forward_count;
	snapshot.reverse_count = reverse_count;
	if (test_state == BOARD_MOTOR_TEST_FORWARD) {
		snapshot.forward_count = count;
	} else if (test_state == BOARD_MOTOR_TEST_REVERSE) {
		snapshot.reverse_count = count - reverse_started_count;
	}
	return snapshot;
}

struct board_motor_stop_test_snapshot board_motor_stop_test_snapshot(void)
{
	struct board_motor_stop_test_snapshot snapshot;
	int32_t count = current_tacho_count();

	snapshot.state = stop_test_state;
	snapshot.mode = stop_test_mode;
	snapshot.total_count = count;
	snapshot.powered_count = stop_test_powered_count;
	snapshot.stopped_count = stop_test_stopped_count;
	if (stop_test_state == BOARD_MOTOR_STOP_TEST_DRIVE) {
		snapshot.powered_count = count;
	} else if (stop_test_state == BOARD_MOTOR_STOP_TEST_MEASURE) {
		snapshot.stopped_count = count - stop_test_started_count;
	}
	return snapshot;
}

struct board_motor_dual_test_snapshot board_motor_dual_test_snapshot(void)
{
	struct board_motor_dual_test_snapshot snapshot;

	snapshot.state = dual_test_state;
	snapshot.a_forward_count = dual_a_forward_count;
	snapshot.b_forward_count = dual_b_forward_count;
	snapshot.a_reverse_count = dual_a_reverse_count;
	snapshot.b_reverse_count = dual_b_reverse_count;
	if (dual_test_state == BOARD_MOTOR_DUAL_TEST_FORWARD) {
		snapshot.a_forward_count = tacho_count[BOARD_MOTOR_PORT_A];
		snapshot.b_forward_count = tacho_count[BOARD_MOTOR_PORT_B];
	} else if (dual_test_state == BOARD_MOTOR_DUAL_TEST_REVERSE) {
		snapshot.a_reverse_count = tacho_count[BOARD_MOTOR_PORT_A] -
			dual_a_reverse_started_count;
		snapshot.b_reverse_count = tacho_count[BOARD_MOTOR_PORT_B] -
			dual_b_reverse_started_count;
	}
	return snapshot;
}

static void count_tacho_edge(enum board_motor_port port)
{
	const struct motor_port_config *config = motor_config(port);
	bool phase = gpio_get(config->tacho_port, config->tacho_phase_pin) != 0U;
	bool direction = gpio_get(config->tacho_port,
		config->tacho_direction_pin) != 0U;

	if (phase == direction) {
		tacho_count[(uint8_t)port]--;
	} else {
		tacho_count[(uint8_t)port]++;
	}
}

void exti9_5_isr(void)
{
	if (exti_get_flag_status(EXTI5) != 0U) {
		count_tacho_edge(BOARD_MOTOR_PORT_A);
		exti_reset_request(EXTI5);
	}
	if (exti_get_flag_status(EXTI7) != 0U) {
		count_tacho_edge(BOARD_MOTOR_PORT_C);
		exti_reset_request(EXTI7);
	}
	if (exti_get_flag_status(EXTI9) != 0U) {
		count_tacho_edge(BOARD_MOTOR_PORT_D);
		exti_reset_request(EXTI9);
	}
}

void exti15_10_isr(void)
{
	if (exti_get_flag_status(EXTI13) != 0U) {
		count_tacho_edge(BOARD_MOTOR_PORT_B);
		exti_reset_request(EXTI13);
	}
}
