#ifndef MICROPY_INCLUDED_EST_MPHALPORT_H
#define MICROPY_INCLUDED_EST_MPHALPORT_H

#include "est_system.h"

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);
int mp_hal_stdin_rx_chr(void);

static inline mp_uint_t mp_hal_ticks_ms(void)
{
	return (mp_uint_t)est_system_millis();
}

#endif
