#ifndef EST_MICROPYTHON_H
#define EST_MICROPYTHON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "est_types.h"

typedef enum {
	EST_MICROPYTHON_NOT_STARTED = 0,
	EST_MICROPYTHON_STARTING = 1,
	EST_MICROPYTHON_PASSED = 2,
	EST_MICROPYTHON_EXCEPTION = 3,
	EST_MICROPYTHON_SELF_TEST_FAILED = 4
} est_micropython_state_t;

#define EST_MICROPYTHON_FLAG_INITIALIZED 0x01U
#define EST_MICROPYTHON_FLAG_SCRIPT_COMPLETE 0x02U
#define EST_MICROPYTHON_FLAG_EXCEPTION 0x04U
#define EST_MICROPYTHON_FLAG_GC_RAN 0x08U
#define EST_MICROPYTHON_FLAG_CLEANUP_CALLED 0x10U

#define EST_MICROPYTHON_PROGRAM_MAX_SIZE 8192U
#define EST_MICROPYTHON_PROGRAM_NO_TIMEOUT_MS 0U
#define EST_MICROPYTHON_PROGRAM_MIN_TIMEOUT_MS 100U
#define EST_MICROPYTHON_PROGRAM_MAX_TIMEOUT_MS 10000U

typedef enum {
	EST_MICROPYTHON_PROGRAM_EMPTY = 0,
	EST_MICROPYTHON_PROGRAM_RECEIVING = 1,
	EST_MICROPYTHON_PROGRAM_READY = 2,
	EST_MICROPYTHON_PROGRAM_QUEUED = 3,
	EST_MICROPYTHON_PROGRAM_RUNNING = 4,
	EST_MICROPYTHON_PROGRAM_COMPLETED = 5,
	EST_MICROPYTHON_PROGRAM_EXCEPTION = 6,
	EST_MICROPYTHON_PROGRAM_STOPPED = 7,
	/* Reserved for protocol compatibility; unlimited runs do not emit it. */
	EST_MICROPYTHON_PROGRAM_TIMED_OUT = 8,
	EST_MICROPYTHON_PROGRAM_INVALID = 9
} est_micropython_program_state_t;

typedef enum {
	EST_MICROPYTHON_PROGRAM_ERROR_NONE = 0,
	EST_MICROPYTHON_PROGRAM_ERROR_INVALID_REQUEST = 1,
	EST_MICROPYTHON_PROGRAM_ERROR_CRC = 2,
	EST_MICROPYTHON_PROGRAM_ERROR_PYTHON_EXCEPTION = 3,
	EST_MICROPYTHON_PROGRAM_ERROR_STOPPED = 4,
	/* Reserved for protocol compatibility; unlimited runs do not emit it. */
	EST_MICROPYTHON_PROGRAM_ERROR_TIMEOUT = 5,
	EST_MICROPYTHON_PROGRAM_ERROR_INTERNAL = 6
} est_micropython_program_error_t;

#define EST_MICROPYTHON_PROGRAM_FLAG_VALID 0x01U
#define EST_MICROPYTHON_PROGRAM_FLAG_RESULT_SET 0x02U
#define EST_MICROPYTHON_PROGRAM_FLAG_CLEANUP_CALLED 0x04U
/* Reserved for status compatibility; unlimited runs leave it clear. */
#define EST_MICROPYTHON_PROGRAM_FLAG_TIMEOUT_ARMED 0x08U

typedef struct {
	est_micropython_program_state_t state;
	est_micropython_program_error_t error;
	uint8_t flags;
	uint16_t expected_length;
	uint16_t received_length;
	uint16_t run_count;
	uint32_t expected_crc32;
	uint32_t actual_crc32;
	uint32_t duration_ms;
	uint32_t timeout_ms;
	int32_t result_value;
} est_micropython_program_status_t;

typedef struct {
	est_micropython_state_t state;
	uint8_t flags;
	uint32_t heap_total_bytes;
	uint32_t heap_used_bytes;
	uint32_t heap_free_bytes;
	uint32_t startup_duration_ms;
	uint32_t maximum_gc_pause_us;
	uint16_t gc_count;
	uint16_t self_test_value;
} est_micropython_status_t;

void est_micropython_init(void);
void est_micropython_deinit(void);
void est_micropython_tick(void);
bool est_micropython_get_status(est_micropython_status_t *status);
void est_micropython_mark_self_test(uint16_t value);
est_result_t est_micropython_program_begin(uint16_t length, uint32_t crc32);
est_result_t est_micropython_program_begin_saved(uint16_t length,
	uint32_t crc32);
est_result_t est_micropython_program_write(uint16_t offset,
	const uint8_t *data, uint16_t length);
est_result_t est_micropython_program_read(uint16_t offset, uint8_t *data,
	uint16_t length);
est_result_t est_micropython_program_run(uint32_t timeout_ms);
est_result_t est_micropython_program_stop(void);
void est_micropython_program_stop_from_vm(void);
est_result_t est_micropython_program_clear(void);
bool est_micropython_program_get_status(
	est_micropython_program_status_t *status);
bool est_micropython_program_is_executing(void);
void est_micropython_program_report(int32_t value);
void est_micropython_vm_hook(void);

#endif
