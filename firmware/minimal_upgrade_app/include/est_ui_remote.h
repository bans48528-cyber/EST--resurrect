#ifndef EST_UI_REMOTE_H
#define EST_UI_REMOTE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	EST_UI_REMOTE_FAULT_NONE = 0,
	EST_UI_REMOTE_FAULT_CONNECT_IR = 1,
	EST_UI_REMOTE_FAULT_SIGNAL_LOST = 2,
	EST_UI_REMOTE_FAULT_DEVICE_LOST = 3
} est_ui_remote_fault_t;

typedef struct {
	uint8_t motor_group;
	uint8_t code;
	est_ui_remote_fault_t fault;
	bool output_enabled;
} est_ui_remote_view_t;

void est_ui_remote_init(void);
void est_ui_remote_enter(uint32_t now_ms, uint8_t motor_group);
void est_ui_remote_switch_motor_group(uint32_t now_ms, uint8_t motor_group);
void est_ui_remote_leave(void);
void est_ui_remote_tick(uint32_t now_ms, uint8_t motor_group,
	est_ui_remote_view_t *view);

#endif
