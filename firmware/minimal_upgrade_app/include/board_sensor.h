#ifndef BOARD_SENSOR_H
#define BOARD_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SENSOR_PORT_COUNT 4U
#define BOARD_SENSOR_TYPE_SOUND 0x03U
#define BOARD_SENSOR_TYPE_TEMPERATURE 0x06U
#define BOARD_SENSOR_TYPE_TOUCH 0x10U
#define BOARD_SENSOR_TYPE_EV3_COLOR 0x1DU
#define BOARD_SENSOR_TYPE_ULTRASONIC 0x1EU
#define BOARD_SENSOR_TYPE_GYRO 0x20U
#define BOARD_SENSOR_TYPE_INFRARED 0x21U

enum board_sensor_port {
	BOARD_SENSOR_PORT_1 = 0,
	BOARD_SENSOR_PORT_2 = 1,
	BOARD_SENSOR_PORT_3 = 2,
	BOARD_SENSOR_PORT_4 = 3
};

enum board_sensor_state {
	BOARD_SENSOR_OFF = 0,
	BOARD_SENSOR_SYNCING = 1,
	BOARD_SENSOR_STREAMING = 2,
	BOARD_SENSOR_STALE = 3
};

enum board_sensor_mode {
	BOARD_SENSOR_MODE_REFLECTED = 0,
	BOARD_SENSOR_MODE_AMBIENT = 1,
	BOARD_SENSOR_MODE_COLOR = 2
};

#define BOARD_SENSOR_MODE_GYRO_ANGLE BOARD_SENSOR_MODE_REFLECTED
#define BOARD_SENSOR_MODE_GYRO_RATE BOARD_SENSOR_MODE_AMBIENT
#define BOARD_SENSOR_MODE_SOUND_DB BOARD_SENSOR_MODE_REFLECTED
#define BOARD_SENSOR_MODE_IR_PROXIMITY BOARD_SENSOR_MODE_REFLECTED
#define BOARD_SENSOR_MODE_IR_BEACON BOARD_SENSOR_MODE_AMBIENT
#define BOARD_SENSOR_MODE_IR_REMOTE BOARD_SENSOR_MODE_COLOR
#define BOARD_SENSOR_MODE_DISTANCE_CM BOARD_SENSOR_MODE_REFLECTED
#define BOARD_SENSOR_MODE_DISTANCE_INCH BOARD_SENSOR_MODE_AMBIENT
#define BOARD_SENSOR_MODE_PRESENCE BOARD_SENSOR_MODE_COLOR
#define BOARD_SENSOR_MODE_CELSIUS BOARD_SENSOR_MODE_REFLECTED
#define BOARD_SENSOR_MODE_FAHRENHEIT BOARD_SENSOR_MODE_AMBIENT

struct board_sensor_snapshot {
	enum board_sensor_state state;
	uint8_t sensor_type;
	uint8_t mode;
	bool value_valid;
	uint16_t value;
	uint16_t adc0_raw;
	uint16_t adc1_raw;
	uint8_t digital_mask;
	uint32_t rx_count;
	uint16_t checksum_errors;
};

void board_sensor_init(uint32_t now_ms);
void board_sensor_tick(uint32_t now_ms);
void board_sensor_stop(void);
bool board_sensor_restart(enum board_sensor_port port, uint32_t now_ms);
bool board_sensor_set_mode(enum board_sensor_port port,
	enum board_sensor_mode mode, uint32_t now_ms);
bool board_sensor_set_all_modes(enum board_sensor_mode mode, uint32_t now_ms);
bool board_sensor_get_snapshot(enum board_sensor_port port,
	struct board_sensor_snapshot *snapshot);

#endif
