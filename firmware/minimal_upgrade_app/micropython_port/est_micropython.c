#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/scs.h>

#include "py/builtin.h"
#include "py/compile.h"
#include "py/cstack.h"
#include "py/gc.h"
#include "py/lexer.h"
#include "py/mperrno.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "shared/runtime/gchelper.h"

#include "est_micropython.h"
#include "est_buttons.h"
#include "est_motor.h"
#include "est_runtime.h"
#include "est_sensor.h"
#include "est_system.h"
#include "usb_hid.h"
#include "watchdog.h"

#define EST_MICROPYTHON_HEAP_SIZE (48U * 1024U)
#define EST_MICROPYTHON_STACK_LIMIT (32U * 1024U)
#define EST_MICROPYTHON_EXPECTED_VALUE 96U
#define EST_CORE_CLOCK_MHZ 168U

extern uint32_t _stack;

static uint8_t micropython_heap[EST_MICROPYTHON_HEAP_SIZE]
	__attribute__((aligned(8)));
static uint8_t program_source[EST_MICROPYTHON_PROGRAM_MAX_SIZE + 1U];
static est_micropython_status_t micropython_status;
static volatile est_micropython_program_status_t program_status;
static bool micropython_initialized;
static volatile bool program_run_requested;
static volatile bool program_stop_requested;
static volatile bool program_executing;
static volatile est_micropython_program_error_t program_abort_error;
static bool program_requires_host;
static uint32_t program_started_ms;
static uint32_t program_last_watchdog_ms;
static uint32_t program_last_usb_poll_ms;

static const char self_test_script[] =
	"import est\n"
	"import est_runtime\n"
	"assert est_runtime.API_VERSION == 1\n"
	"items = []\n"
	"for i in range(96):\n"
	"    items.append((i, i * i))\n"
	"assert len(items) == 96\n"
	"assert est.drive_mix(50, 40) == (40, 20)\n"
	"assert est.motor_type(0) in (0, 4, 5, 255)\n"
	"assert est.Motor(\"A\").port() == \"A\"\n"
	"assert len(est.sensor(1)) == 6\n"
	"assert est.Sensor(1).port() == 1\n"
	"assert 0 <= est.buttons.value() <= 63\n"
	"assert est.battery.valid() in (True, False)\n"
	"assert est.force_gc() >= 0\n"
	"est._self_test_complete(len(items))\n";

static uint32_t cycle_count_us(uint32_t start_cycles)
{
	uint32_t elapsed = DWT_CYCCNT - start_cycles;

	return (elapsed + EST_CORE_CLOCK_MHZ - 1U) / EST_CORE_CLOCK_MHZ;
}

static void refresh_heap_status(void)
{
	gc_info_t info = {0};

	gc_info(&info);
	micropython_status.heap_total_bytes = (uint32_t)info.total;
	micropython_status.heap_used_bytes = (uint32_t)info.used;
	micropython_status.heap_free_bytes = (uint32_t)info.free;
}

static uint32_t crc32_bytes(const uint8_t *data, uint16_t length)
{
	uint32_t crc = 0xFFFFFFFFUL;
	uint16_t index;

	for (index = 0U; index < length; index++) {
		uint8_t bit;

		crc ^= data[index];
		for (bit = 0U; bit < 8U; bit++) {
			crc = (crc >> 1U) ^
				((crc & 1U) != 0U ? 0xEDB88320UL : 0U);
		}
	}
	return crc ^ 0xFFFFFFFFUL;
}

static uint16_t exception_source_line(mp_obj_t exception)
{
	size_t count;
	size_t *traceback;
	size_t index;

	if (!mp_obj_is_exception_instance(exception)) {
		return 0U;
	}
	mp_obj_exception_get_traceback(exception, &count, &traceback);
	for (index = 0U; index + 2U < count; index += 3U) {
		if ((qstr)traceback[index] == MP_QSTR__lt_stdin_gt_ &&
		    traceback[index + 1U] > 0U) {
			return traceback[index + 1U] <= UINT16_MAX ?
				(uint16_t)traceback[index + 1U] : UINT16_MAX;
		}
	}
	return 0U;
}

static bool execute_script(const char *source, size_t length,
	uint16_t *exception_line)
{
	nlr_buf_t nlr;

	if (exception_line != NULL) {
		*exception_line = 0U;
	}

	if (nlr_push(&nlr) == 0) {
		nlr_set_abort(&nlr);
		mp_lexer_t *lexer = mp_lexer_new_from_str_len(
			MP_QSTR__lt_stdin_gt_, source, length, 0U);
		qstr source_name = lexer->source_name;
		mp_parse_tree_t parse_tree = mp_parse(lexer, MP_PARSE_FILE_INPUT);
		mp_obj_t module_function = mp_compile(
			&parse_tree, source_name, true);

		mp_call_function_0(module_function);
		nlr_pop();
		nlr_set_abort(NULL);
		return true;
	}
	if (exception_line != NULL) {
		*exception_line = exception_source_line(
			MP_OBJ_FROM_PTR(nlr.ret_val));
	}
	nlr_set_abort(NULL);
	return false;
}

static void initialize_vm(void)
{
	mp_cstack_init_with_top(&_stack, EST_MICROPYTHON_STACK_LIMIT);
	gc_init(micropython_heap,
		micropython_heap + sizeof(micropython_heap));
	mp_init();
	micropython_initialized = true;
	micropython_status.flags |= EST_MICROPYTHON_FLAG_INITIALIZED;
}

static void reset_vm(void)
{
	if (micropython_initialized) {
		mp_deinit();
		micropython_initialized = false;
	}
	initialize_vm();
}

static void restart_sensors(void)
{
	est_sensor_port_t port;

	for (port = EST_SENSOR_PORT_1; port < EST_SENSOR_PORT_COUNT; port++) {
		(void)est_sensor_restart(port);
	}
}

static void program_cleanup_after_failure(void)
{
	(void)est_system_cleanup();
	program_status.flags |= EST_MICROPYTHON_PROGRAM_FLAG_CLEANUP_CALLED;
	restart_sensors();
}

void est_micropython_init(void)
{
	uint32_t started_ms;
	bool script_succeeded;

	memset(&micropython_status, 0, sizeof(micropython_status));
	micropython_status.state = EST_MICROPYTHON_STARTING;
	started_ms = est_system_millis();
	watchdog_startup_progress();

	SCS_DEMCR |= SCS_DEMCR_TRCENA;
	DWT_CYCCNT = 0U;
	DWT_CTRL |= DWT_CTRL_CYCCNTENA;

	memset((void *)&program_status, 0, sizeof(program_status));
	program_status.state = EST_MICROPYTHON_PROGRAM_EMPTY;
	program_run_requested = false;
	program_stop_requested = false;
	program_executing = false;
	program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	program_requires_host = false;
	initialize_vm();

	script_succeeded = execute_script(
		self_test_script, sizeof(self_test_script) - 1U, NULL);
	refresh_heap_status();
	micropython_status.startup_duration_ms =
		est_system_millis() - started_ms;
	if (!script_succeeded) {
		micropython_status.state = EST_MICROPYTHON_EXCEPTION;
		micropython_status.flags |= EST_MICROPYTHON_FLAG_EXCEPTION;
		(void)est_system_cleanup();
		micropython_status.flags |= EST_MICROPYTHON_FLAG_CLEANUP_CALLED;
	} else if (micropython_status.self_test_value !=
		   EST_MICROPYTHON_EXPECTED_VALUE) {
		micropython_status.state = EST_MICROPYTHON_SELF_TEST_FAILED;
		(void)est_system_cleanup();
		micropython_status.flags |= EST_MICROPYTHON_FLAG_CLEANUP_CALLED;
	} else {
		micropython_status.state = EST_MICROPYTHON_PASSED;
		micropython_status.flags |= EST_MICROPYTHON_FLAG_SCRIPT_COMPLETE;
	}
	watchdog_startup_progress();
}

void est_micropython_deinit(void)
{
	program_stop_requested = true;
	(void)est_motor_stop_all(EST_STOP_COAST);
	if (micropython_initialized) {
		mp_deinit();
		micropython_initialized = false;
		micropython_status.flags &=
			(uint8_t)~EST_MICROPYTHON_FLAG_INITIALIZED;
	}
}

void est_micropython_tick(void)
{
	bool script_succeeded;
	uint32_t finished_ms;
	uint16_t exception_line;

	if (!program_run_requested ||
	    program_status.state != EST_MICROPYTHON_PROGRAM_QUEUED) {
		return;
	}
	program_run_requested = false;
	program_stop_requested = false;
	program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	program_status.state = EST_MICROPYTHON_PROGRAM_RUNNING;
	program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	program_status.flags &= (uint8_t)~(
		EST_MICROPYTHON_PROGRAM_FLAG_RESULT_SET |
		EST_MICROPYTHON_PROGRAM_FLAG_CLEANUP_CALLED |
		EST_MICROPYTHON_PROGRAM_FLAG_TIMEOUT_ARMED);
	program_status.duration_ms = 0U;
	program_status.result_value = 0;
	program_status.exception_line = 0U;
	program_started_ms = est_system_millis();
	program_last_watchdog_ms = program_started_ms;
	program_last_usb_poll_ms = program_started_ms;
	program_executing = true;
	(void)est_motor_stop_all(EST_STOP_COAST);
	reset_vm();
	script_succeeded = execute_script((const char *)program_source,
		program_status.expected_length, &exception_line);
	finished_ms = est_system_millis();
	program_status.duration_ms = finished_ms - program_started_ms;
	program_status.run_count++;
	program_status.flags &=
		(uint8_t)~EST_MICROPYTHON_PROGRAM_FLAG_TIMEOUT_ARMED;
	(void)est_motor_stop_all(EST_STOP_COAST);
	refresh_heap_status();

	if (script_succeeded) {
		program_status.state = EST_MICROPYTHON_PROGRAM_COMPLETED;
		program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	} else if (program_abort_error ==
		   EST_MICROPYTHON_PROGRAM_ERROR_STOPPED) {
		program_status.state = EST_MICROPYTHON_PROGRAM_STOPPED;
		program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
		program_cleanup_after_failure();
	} else {
		program_status.state = EST_MICROPYTHON_PROGRAM_EXCEPTION;
		program_status.error =
			EST_MICROPYTHON_PROGRAM_ERROR_PYTHON_EXCEPTION;
		program_status.exception_line = exception_line;
		program_cleanup_after_failure();
	}
	program_executing = false;
	program_stop_requested = false;
}

bool est_micropython_get_status(est_micropython_status_t *status)
{
	if (status == NULL) {
		return false;
	}
	*status = micropython_status;
	return true;
}

void est_micropython_mark_self_test(uint16_t value)
{
	micropython_status.self_test_value = value;
}

static est_result_t program_begin(uint16_t length, uint32_t crc32,
	bool requires_host)
{
	if (!micropython_initialized) {
		return EST_ERR_STATE;
	}
	if (program_executing || program_run_requested) {
		return EST_ERR_BUSY;
	}
	if (length == 0U || length > EST_MICROPYTHON_PROGRAM_MAX_SIZE) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	program_status.state = EST_MICROPYTHON_PROGRAM_RECEIVING;
	program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	program_status.flags = 0U;
	program_status.expected_length = length;
	program_status.received_length = 0U;
	program_status.run_count = 0U;
	program_status.expected_crc32 = crc32;
	program_status.actual_crc32 = 0U;
	program_status.duration_ms = 0U;
	program_status.timeout_ms = 0U;
	program_status.result_value = 0;
	program_status.exception_line = 0U;
	program_requires_host = requires_host;
	program_source[0] = 0U;
	return EST_OK;
}

est_result_t est_micropython_program_begin(uint16_t length, uint32_t crc32)
{
	return program_begin(length, crc32, true);
}

est_result_t est_micropython_program_begin_saved(uint16_t length,
	uint32_t crc32)
{
	return program_begin(length, crc32, false);
}

est_result_t est_micropython_program_write(uint16_t offset,
	const uint8_t *data, uint16_t length)
{
	uint16_t index;

	if (data == NULL || length == 0U) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (program_status.state != EST_MICROPYTHON_PROGRAM_RECEIVING ||
	    offset != program_status.received_length) {
		return EST_ERR_STATE;
	}
	if ((uint32_t)offset + length > program_status.expected_length) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	for (index = 0U; index < length; index++) {
		if (data[index] == 0U) {
			program_status.state = EST_MICROPYTHON_PROGRAM_INVALID;
			program_status.error =
				EST_MICROPYTHON_PROGRAM_ERROR_INVALID_REQUEST;
			return EST_ERR_INVALID_ARGUMENT;
		}
	}
	memcpy(&program_source[offset], data, length);
	program_status.received_length = (uint16_t)(offset + length);
	if (program_status.received_length != program_status.expected_length) {
		return EST_OK;
	}
	program_source[program_status.expected_length] = 0U;
	program_status.actual_crc32 = crc32_bytes(program_source,
		program_status.expected_length);
	if (program_status.actual_crc32 != program_status.expected_crc32) {
		program_status.state = EST_MICROPYTHON_PROGRAM_INVALID;
		program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_CRC;
		return EST_ERR_IO;
	}
	program_status.flags |= EST_MICROPYTHON_PROGRAM_FLAG_VALID;
	program_status.state = EST_MICROPYTHON_PROGRAM_READY;
	return EST_OK;
}

est_result_t est_micropython_program_read(uint16_t offset, uint8_t *data,
	uint16_t length)
{
	if (data == NULL || length == 0U) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (program_executing || program_run_requested) {
		return EST_ERR_BUSY;
	}
	if ((program_status.flags & EST_MICROPYTHON_PROGRAM_FLAG_VALID) == 0U) {
		return EST_ERR_STATE;
	}
	if ((uint32_t)offset + length > program_status.expected_length) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	memcpy(data, &program_source[offset], length);
	return EST_OK;
}

est_result_t est_micropython_program_run(uint32_t timeout_ms)
{
	if ((program_status.flags & EST_MICROPYTHON_PROGRAM_FLAG_VALID) == 0U) {
		return EST_ERR_STATE;
	}
	if (program_executing || program_run_requested) {
		return EST_ERR_BUSY;
	}
	if (timeout_ms != EST_MICROPYTHON_PROGRAM_NO_TIMEOUT_MS &&
	    (timeout_ms < EST_MICROPYTHON_PROGRAM_MIN_TIMEOUT_MS ||
	     timeout_ms > EST_MICROPYTHON_PROGRAM_MAX_TIMEOUT_MS)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	program_status.timeout_ms = EST_MICROPYTHON_PROGRAM_NO_TIMEOUT_MS;
	program_status.state = EST_MICROPYTHON_PROGRAM_QUEUED;
	program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_NONE;
	program_run_requested = true;
	return EST_OK;
}

est_result_t est_micropython_program_stop(void)
{
	if (program_status.state == EST_MICROPYTHON_PROGRAM_QUEUED) {
		program_run_requested = false;
		program_status.state = EST_MICROPYTHON_PROGRAM_STOPPED;
		program_status.error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
		(void)est_motor_stop_all(EST_STOP_COAST);
		return EST_OK;
	}
	if (!program_executing ||
	    program_status.state != EST_MICROPYTHON_PROGRAM_RUNNING) {
		return EST_ERR_STATE;
	}
	program_stop_requested = true;
	program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
	(void)est_motor_stop_all(EST_STOP_COAST);
	return EST_OK;
}

void est_micropython_program_stop_from_vm(void)
{
	if (!program_executing) {
		return;
	}
	program_stop_requested = true;
	program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
	(void)est_motor_stop_all(EST_STOP_COAST);
	mp_sched_vm_abort();
	mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_EXCEPTIONS);
}

est_result_t est_micropython_program_clear(void)
{
	if (program_executing || program_run_requested) {
		return EST_ERR_BUSY;
	}
	memset((void *)&program_status, 0, sizeof(program_status));
	program_status.state = EST_MICROPYTHON_PROGRAM_EMPTY;
	program_source[0] = 0U;
	return EST_OK;
}

bool est_micropython_program_get_status(
	est_micropython_program_status_t *status)
{
	if (status == NULL) {
		return false;
	}
	status->state = program_status.state;
	status->error = program_status.error;
	status->flags = program_status.flags;
	status->expected_length = program_status.expected_length;
	status->received_length = program_status.received_length;
	status->run_count = program_status.run_count;
	status->expected_crc32 = program_status.expected_crc32;
	status->actual_crc32 = program_status.actual_crc32;
	status->duration_ms = program_status.duration_ms;
	status->timeout_ms = program_status.timeout_ms;
	status->result_value = program_status.result_value;
	status->exception_line = program_status.exception_line;
	return true;
}

bool est_micropython_program_is_executing(void)
{
	return program_executing;
}

void est_micropython_program_report(int32_t value)
{
	if (program_executing) {
		program_status.result_value = value;
		program_status.flags |= EST_MICROPYTHON_PROGRAM_FLAG_RESULT_SET;
	}
}

void est_micropython_vm_hook(void)
{
	uint32_t now_ms;

	if (!program_executing) {
		return;
	}
	now_ms = est_system_millis();
	if ((uint32_t)(now_ms - program_last_usb_poll_ms) >= 1U) {
		program_last_usb_poll_ms = now_ms;
		usb_hid_poll();
		est_runtime_tick(now_ms);
	}
	if (est_button_is_pressed(EST_BUTTON_BACK) ||
	    (program_requires_host && !usb_hid_host_connected())) {
		program_stop_requested = true;
		program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
		(void)est_motor_stop_all(EST_STOP_COAST);
	}
	if ((uint32_t)(now_ms - program_last_watchdog_ms) >= 10U) {
		program_last_watchdog_ms = now_ms;
		watchdog_vm_progress(now_ms);
	}
	if (program_stop_requested) {
		program_abort_error = EST_MICROPYTHON_PROGRAM_ERROR_STOPPED;
		mp_sched_vm_abort();
		mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_EXCEPTIONS);
	}
}

void gc_collect(void)
{
	uint32_t start_cycles = DWT_CYCCNT;
	uint32_t elapsed_us;

	gc_collect_start();
	gc_helper_collect_regs_and_stack();
	gc_collect_end();

	elapsed_us = cycle_count_us(start_cycles);
	micropython_status.gc_count++;
	micropython_status.flags |= EST_MICROPYTHON_FLAG_GC_RAN;
	if (elapsed_us > micropython_status.maximum_gc_pause_us) {
		micropython_status.maximum_gc_pause_us = elapsed_us;
	}
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len)
{
	(void)str;
	return len;
}

int mp_hal_stdin_rx_chr(void)
{
	return -1;
}

mp_lexer_t *mp_lexer_new_from_file(qstr filename)
{
	(void)filename;
	mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path)
{
	(void)path;
	return MP_IMPORT_STAT_NO_EXIST;
}

void nlr_jump_fail(void *value)
{
	(void)value;
	(void)est_system_cleanup();
	for (;;) {
	}
}

void MP_NORETURN __fatal_error(const char *message)
{
	(void)message;
	(void)est_system_cleanup();
	for (;;) {
	}
}
