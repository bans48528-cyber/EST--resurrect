#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "board_motor.h"

/* V5 schematic and legacy EST app: outputs A/B/C/D share TIM4. */
#define MOTOR_A_PWM_PORT GPIOB
#define MOTOR_A_PWM_PIN GPIO9
#define MOTOR_A_DIRECTION_PORT GPIOG
#define MOTOR_A_DIRECTION_0_PIN GPIO10
#define MOTOR_A_DIRECTION_1_PIN GPIO11
#define MOTOR_A_TACHO_PORT GPIOE
#define MOTOR_A_TACHO_PHASE_PIN GPIO5
#define MOTOR_A_TACHO_DIRECTION_PIN GPIO6

#define MOTOR_B_PWM_PORT GPIOB
#define MOTOR_B_PWM_PIN GPIO8
#define MOTOR_B_DIRECTION_PORT GPIOD
#define MOTOR_B_DIRECTION_0_PIN GPIO0
#define MOTOR_B_DIRECTION_1_PIN GPIO1
#define MOTOR_B_TACHO_PORT GPIOE
#define MOTOR_B_TACHO_PHASE_PIN GPIO13
#define MOTOR_B_TACHO_DIRECTION_PIN GPIO14

#define MOTOR_C_PWM_PORT GPIOD
#define MOTOR_C_PWM_PIN GPIO13
#define MOTOR_C_DIRECTION_PORT GPIOG
#define MOTOR_C_DIRECTION_0_PIN GPIO12
#define MOTOR_C_DIRECTION_1_PIN GPIO13
#define MOTOR_C_TACHO_PORT GPIOC
#define MOTOR_C_TACHO_PHASE_PIN GPIO7
#define MOTOR_C_TACHO_DIRECTION_PIN GPIO6

#define MOTOR_D_PWM_PORT GPIOD
#define MOTOR_D_PWM_PIN GPIO12
#define MOTOR_D_DIRECTION_PORT GPIOA
#define MOTOR_D_DIRECTION_0_PIN GPIO10
#define MOTOR_D_DIRECTION_1_PIN GPIO9
#define MOTOR_D_TACHO_PORT GPIOC
#define MOTOR_D_TACHO_PHASE_PIN GPIO9
#define MOTOR_D_TACHO_DIRECTION_PIN GPIO8

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
};

static const struct motor_port_config motor_ports[BOARD_MOTOR_PORT_COUNT] = {
	[BOARD_MOTOR_PORT_A] = {
		MOTOR_A_PWM_PORT, MOTOR_A_PWM_PIN, TIM_OC4,
		MOTOR_A_DIRECTION_PORT, MOTOR_A_DIRECTION_0_PIN,
		MOTOR_A_DIRECTION_1_PIN, MOTOR_A_TACHO_PORT,
		MOTOR_A_TACHO_PHASE_PIN, MOTOR_A_TACHO_DIRECTION_PIN
	},
	[BOARD_MOTOR_PORT_B] = {
		MOTOR_B_PWM_PORT, MOTOR_B_PWM_PIN, TIM_OC3,
		MOTOR_B_DIRECTION_PORT, MOTOR_B_DIRECTION_0_PIN,
		MOTOR_B_DIRECTION_1_PIN, MOTOR_B_TACHO_PORT,
		MOTOR_B_TACHO_PHASE_PIN, MOTOR_B_TACHO_DIRECTION_PIN
	},
	[BOARD_MOTOR_PORT_C] = {
		MOTOR_C_PWM_PORT, MOTOR_C_PWM_PIN, TIM_OC2,
		MOTOR_C_DIRECTION_PORT, MOTOR_C_DIRECTION_0_PIN,
		MOTOR_C_DIRECTION_1_PIN, MOTOR_C_TACHO_PORT,
		MOTOR_C_TACHO_PHASE_PIN, MOTOR_C_TACHO_DIRECTION_PIN
	},
	[BOARD_MOTOR_PORT_D] = {
		MOTOR_D_PWM_PORT, MOTOR_D_PWM_PIN, TIM_OC1,
		MOTOR_D_DIRECTION_PORT, MOTOR_D_DIRECTION_0_PIN,
		MOTOR_D_DIRECTION_1_PIN, MOTOR_D_TACHO_PORT,
		MOTOR_D_TACHO_PHASE_PIN, MOTOR_D_TACHO_DIRECTION_PIN
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

static bool motor_port_valid(enum board_motor_port port)
{
	return (uint8_t)port < BOARD_MOTOR_PORT_COUNT;
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

static void configure_pwm_channel(enum tim_oc_id channel)
{
	timer_set_oc_mode(TIM4, channel, TIM_OCM_PWM1);
	timer_set_oc_polarity_high(TIM4, channel);
	timer_enable_oc_preload(TIM4, channel);
	timer_set_oc_value(TIM4, channel, MOTOR_PWM_OFF_COMPARE);
	timer_enable_oc_output(TIM4, channel);
}

void board_motor_init(void)
{
	uint8_t port_index;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOG);
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
}

bool board_motor_diagnostic_active(void)
{
	return normal_test_active() || stop_test_active() || dual_test_active();
}

bool board_motor_set_power(enum board_motor_port port, int8_t power_percent)
{
	int16_t magnitude = power_percent;
	uint32_t compare;

	if (!motor_port_valid(port) || power_percent < -100 ||
	    power_percent > 100 || board_motor_diagnostic_active()) {
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
	if (!motor_port_valid(port) || board_motor_diagnostic_active()) {
		return false;
	}
	motor_output_off(port);
	return true;
}

bool board_motor_brake(enum board_motor_port port)
{
	if (!motor_port_valid(port) || board_motor_diagnostic_active()) {
		return false;
	}
	motor_output_high_push_pull_stop(port);
	return true;
}

bool board_motor_reset_tacho(enum board_motor_port port)
{
	if (!motor_port_valid(port) || board_motor_diagnostic_active()) {
		return false;
	}
	tacho_count[(uint8_t)port] = 0;
	return true;
}

bool board_motor_control_snapshot(enum board_motor_port port,
	struct board_motor_control_snapshot *snapshot)
{
	if (!motor_port_valid(port) || snapshot == NULL) {
		return false;
	}
	snapshot->state = output_state[(uint8_t)port];
	snapshot->power_percent = output_power[(uint8_t)port];
	snapshot->tacho_count = tacho_count[(uint8_t)port];
	return true;
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
	if (!motor_port_valid(port) || power_percent == 0U ||
	    power_percent > 100U) {
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
	if (!motor_port_valid(port) ||
	    (mode != BOARD_MOTOR_STOP_LOW_OPEN_DRAIN &&
	     mode != BOARD_MOTOR_STOP_HIGH_PUSH_PULL) ||
	    power_percent == 0U || power_percent > 100U) {
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
	if (power_percent == 0U || power_percent > 100U ||
	    normal_test_active() || stop_test_active() || dual_test_active()) {
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
	motor_output_off_all();
	test_state = BOARD_MOTOR_TEST_IDLE;
	stop_test_state = BOARD_MOTOR_STOP_TEST_IDLE;
	dual_test_state = BOARD_MOTOR_DUAL_TEST_IDLE;
}

void board_motor_tick(uint32_t now_ms)
{
	uint32_t elapsed_ms = now_ms - phase_started_ms;

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
