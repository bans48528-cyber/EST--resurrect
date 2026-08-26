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

enum board_motor_type {
	BOARD_MOTOR_TYPE_NONE = 0,
	BOARD_MOTOR_TYPE_LARGE = 4,
	BOARD_MOTOR_TYPE_MEDIUM = 5,
	BOARD_MOTOR_TYPE_UNKNOWN = 0xFF
};

struct board_motor_control_snapshot {
	enum board_motor_output_state state;
	enum board_motor_type type;
	int8_t power_percent;
	int8_t speed_percent;
	int32_t tacho_count;
	uint16_t id_adc_raw;
	uint16_t id_mv;
	uint16_t id_pin6_low_adc_raw;
	uint16_t id_pin6_low_mv;
	uint16_t id_pin5_pullup_adc_raw;
	uint16_t id_pin5_pullup_mv;
	uint8_t id_pin5_pullup_high;
};

enum board_motor_position_state {
	BOARD_MOTOR_POSITION_IDLE = 0,
	BOARD_MOTOR_POSITION_RUNNING = 1,
	BOARD_MOTOR_POSITION_COMPLETE = 2,
	BOARD_MOTOR_POSITION_TIMEOUT = 3
};

struct board_motor_position_snapshot {
	enum board_motor_position_state state;
	enum board_motor_port port;
	enum board_motor_type type;
	int8_t requested_speed_percent;
	int8_t measured_speed_percent;
	int32_t start_count;
	int32_t target_count;
	int32_t current_count;
};

enum board_motor_pair_position_state {
	BOARD_MOTOR_PAIR_POSITION_IDLE = 0,
	BOARD_MOTOR_PAIR_POSITION_RUNNING = 1,
	BOARD_MOTOR_PAIR_POSITION_COMPLETE = 2,
	BOARD_MOTOR_PAIR_POSITION_TIMEOUT = 3
};

struct board_motor_pair_position_snapshot {
	enum board_motor_pair_position_state state;
	enum board_motor_port left_port;
	enum board_motor_port right_port;
	int32_t left_start_count;
	int32_t left_target_count;
	int32_t left_current_count;
	int32_t right_start_count;
	int32_t right_target_count;
	int32_t right_current_count;
	int32_t synchronization_error_count;
	int32_t maximum_synchronization_error_count;
};

enum board_motor_speed_state {
	BOARD_MOTOR_SPEED_IDLE = 0,
	BOARD_MOTOR_SPEED_RUNNING = 1
};

struct board_motor_speed_snapshot {
	enum board_motor_speed_state state;
	enum board_motor_port port;
	enum board_motor_output_state output_state;
	enum board_motor_type type;
	int8_t requested_speed_percent;
	int8_t measured_speed_percent;
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
bool board_motor_refresh_identification(uint32_t now_ms,
	enum board_motor_port port);
bool board_motor_start_position(uint32_t now_ms, enum board_motor_port port,
	uint8_t speed_percent, int32_t degrees);
bool board_motor_stop_position(enum board_motor_port port,
	enum board_motor_stop_mode stop_mode);
bool board_motor_position_snapshot_for_port(enum board_motor_port port,
	struct board_motor_position_snapshot *snapshot);
struct board_motor_position_snapshot board_motor_position_snapshot(void);
bool board_motor_start_pair_position(uint32_t now_ms,
	enum board_motor_port left_port, int32_t left_degrees,
	enum board_motor_port right_port, int32_t right_degrees,
	uint8_t maximum_speed_percent);
bool board_motor_stop_pair_position(enum board_motor_stop_mode stop_mode);
struct board_motor_pair_position_snapshot
	board_motor_pair_position_snapshot(void);
bool board_motor_start_speed(uint32_t now_ms, enum board_motor_port port,
	int8_t speed_percent);
bool board_motor_stop_speed(enum board_motor_port port,
	enum board_motor_stop_mode stop_mode);
bool board_motor_speed_snapshot_for_port(enum board_motor_port port,
	struct board_motor_speed_snapshot *snapshot);
struct board_motor_speed_snapshot board_motor_speed_snapshot(void);
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
