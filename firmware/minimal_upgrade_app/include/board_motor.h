#ifndef BOARD_MOTOR_H
#define BOARD_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

enum board_motor_port {
	BOARD_MOTOR_PORT_A = 0,
	BOARD_MOTOR_PORT_B = 1,
	BOARD_MOTOR_PORT_C = 2,
	BOARD_MOTOR_PORT_D = 3,
	BOARD_MOTOR_PORT_COUNT = 4
};

enum board_motor_output_state {
	BOARD_MOTOR_OUTPUT_COAST = 0,
	BOARD_MOTOR_OUTPUT_DRIVE = 1,
	BOARD_MOTOR_OUTPUT_BRAKE = 2
};

struct board_motor_control_snapshot {
	enum board_motor_output_state state;
	int8_t power_percent;
	int32_t tacho_count;
};

enum board_motor_test_state {
	BOARD_MOTOR_TEST_IDLE = 0,
	BOARD_MOTOR_TEST_FORWARD = 1,
	BOARD_MOTOR_TEST_PAUSE = 2,
	BOARD_MOTOR_TEST_REVERSE = 3,
	BOARD_MOTOR_TEST_COMPLETE = 4
};

struct board_motor_tacho_snapshot {
	int32_t total_count;
	int32_t forward_count;
	int32_t reverse_count;
};

enum board_motor_stop_mode {
	/* M0.36A real-device result: this state is free coast. */
	BOARD_MOTOR_STOP_LOW_OPEN_DRAIN = 0,
	/* M0.36A real-device result: this state is active brake. */
	BOARD_MOTOR_STOP_HIGH_PUSH_PULL = 1
};

enum board_motor_stop_test_state {
	BOARD_MOTOR_STOP_TEST_IDLE = 0,
	BOARD_MOTOR_STOP_TEST_DRIVE = 1,
	BOARD_MOTOR_STOP_TEST_MEASURE = 2,
	BOARD_MOTOR_STOP_TEST_COMPLETE = 3
};

struct board_motor_stop_test_snapshot {
	enum board_motor_stop_test_state state;
	enum board_motor_stop_mode mode;
	int32_t total_count;
	int32_t powered_count;
	int32_t stopped_count;
};

enum board_motor_dual_test_state {
	BOARD_MOTOR_DUAL_TEST_IDLE = 0,
	BOARD_MOTOR_DUAL_TEST_FORWARD = 1,
	BOARD_MOTOR_DUAL_TEST_BRAKE_PAUSE = 2,
	BOARD_MOTOR_DUAL_TEST_REVERSE = 3,
	BOARD_MOTOR_DUAL_TEST_FINAL_BRAKE = 4,
	BOARD_MOTOR_DUAL_TEST_COMPLETE = 5
};

struct board_motor_dual_test_snapshot {
	enum board_motor_dual_test_state state;
	int32_t a_forward_count;
	int32_t b_forward_count;
	int32_t a_reverse_count;
	int32_t b_reverse_count;
};

void board_motor_init(void);
bool board_motor_diagnostic_active(void);
bool board_motor_set_power(enum board_motor_port port, int8_t power_percent);
bool board_motor_coast(enum board_motor_port port);
bool board_motor_brake(enum board_motor_port port);
bool board_motor_reset_tacho(enum board_motor_port port);
bool board_motor_control_snapshot(enum board_motor_port port,
	struct board_motor_control_snapshot *snapshot);
bool board_motor_start_test(uint32_t now_ms);
bool board_motor_start_test_with_power(uint32_t now_ms, uint8_t power_percent);
bool board_motor_start_port_test_with_power(uint32_t now_ms,
	enum board_motor_port port, uint8_t power_percent);
bool board_motor_start_stop_test(uint32_t now_ms,
	enum board_motor_stop_mode mode, uint8_t power_percent);
bool board_motor_start_port_stop_test(uint32_t now_ms,
	enum board_motor_port port, enum board_motor_stop_mode mode,
	uint8_t power_percent);
bool board_motor_start_dual_test(uint32_t now_ms, uint8_t power_percent);
void board_motor_stop(void);
void board_motor_tick(uint32_t now_ms);
enum board_motor_test_state board_motor_test_state(void);
struct board_motor_tacho_snapshot board_motor_tacho_snapshot(void);
struct board_motor_stop_test_snapshot board_motor_stop_test_snapshot(void);
struct board_motor_dual_test_snapshot board_motor_dual_test_snapshot(void);

#endif
