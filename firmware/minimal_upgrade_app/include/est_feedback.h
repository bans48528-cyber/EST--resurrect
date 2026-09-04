#ifndef EST_FEEDBACK_H
#define EST_FEEDBACK_H

#include <stdbool.h>
#include <stdint.h>

void est_feedback_init(void);
void est_feedback_button(uint32_t now_ms);
void est_feedback_usb_connected(uint32_t now_ms);
void est_feedback_transfer_complete(uint32_t now_ms);
void est_feedback_program_start(bool requires_host, uint32_t now_ms);

#endif
