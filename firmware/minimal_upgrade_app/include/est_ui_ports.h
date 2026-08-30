#ifndef EST_UI_PORTS_H
#define EST_UI_PORTS_H

#include <stdint.h>

typedef enum {
	EST_UI_PORT_DISCONNECTED = 0,
	EST_UI_PORT_DETECTING = 1,
	EST_UI_PORT_CONNECTED = 2
} est_ui_port_connection_t;

typedef enum {
	EST_UI_MOTOR_NONE = 0,
	EST_UI_MOTOR_LARGE = 1,
	EST_UI_MOTOR_MEDIUM = 2
} est_ui_motor_kind_t;

typedef enum {
	EST_UI_MOTOR_COAST = 0,
	EST_UI_MOTOR_DRIVE = 1,
	EST_UI_MOTOR_BRAKE = 2
} est_ui_motor_state_t;

typedef struct {
	est_ui_port_connection_t connection;
	est_ui_motor_kind_t kind;
	est_ui_motor_state_t state;
	int8_t speed_percent;
	int32_t angle_degrees;
} est_ui_motor_port_view_t;

typedef struct {
	est_ui_port_connection_t connection;
	const char *model;
	const char *mode;
	char value[20];
} est_ui_sensor_port_view_t;

typedef struct {
	est_ui_motor_port_view_t motors[4];
	est_ui_sensor_port_view_t sensors[4];
} est_ui_ports_view_t;

void est_ui_ports_capture(est_ui_ports_view_t *view);
bool est_ui_ports_cycle_sensor_mode(uint8_t sensor_port, uint32_t now_ms);

#endif
