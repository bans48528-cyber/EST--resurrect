#ifndef EST_SYSTEM_H
#define EST_SYSTEM_H

#include <stdint.h>

#include "est_types.h"

void est_system_init(void);
uint32_t est_system_millis(void);
est_result_t est_system_emergency_stop(void);
est_result_t est_system_cleanup(void);
est_result_t est_system_reboot(void);
void est_system_power_off(void) __attribute__((noreturn));

#endif
