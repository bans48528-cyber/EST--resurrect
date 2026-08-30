#include <stdint.h>
#include <string.h>

#include "py/gc.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"

#include "est_battery.h"
#include "est_backlight.h"
#include "est_buttons.h"
#include "est_display.h"
#include "est_drive.h"
#include "est_led.h"
#include "est_micropython.h"
#include "est_motor.h"
#include "est_runtime.h"
#include "est_sensor.h"
#include "est_system.h"

static int32_t require_integer_range(mp_obj_t value, int32_t minimum,
	int32_t maximum, mp_rom_error_text_t message)
{
	int32_t integer = (int32_t)mp_obj_get_int(value);

	if (integer < minimum || integer > maximum) {
		mp_raise_ValueError(message);
	}
	return integer;
}

static mp_obj_t modest_millis(void)
{
	return mp_obj_new_int_from_uint(est_system_millis());
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_millis_obj, modest_millis);

static mp_obj_t modest_motor_type(mp_obj_t port_object)
{
	int32_t port = require_integer_range(
		port_object, 0, EST_MOTOR_PORT_COUNT - 1,
		MP_ERROR_TEXT("motor port must be 0..3"));
	est_motor_type_t type = EST_MOTOR_TYPE_UNKNOWN;
	est_result_t result = est_motor_get_type((est_motor_port_t)port, &type);

	if (result != EST_OK) {
		return mp_obj_new_int((mp_int_t)result);
	}
	return mp_obj_new_int((mp_int_t)type);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_type_obj, modest_motor_type);

static mp_obj_t modest_stop_all(void)
{
	return mp_obj_new_int((mp_int_t)est_motor_stop_all(EST_STOP_COAST));
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_stop_all_obj, modest_stop_all);

typedef struct {
	mp_obj_base_t base;
	est_motor_port_t port;
} modest_motor_instance_t;

static est_motor_port_t modest_motor_port_from_object(mp_obj_t port_object)
{
	size_t port_length;
	const char *port_name = mp_obj_str_get_data(port_object, &port_length);

	if (port_length != 1U || port_name[0] < 'A' || port_name[0] > 'D') {
		mp_raise_ValueError(MP_ERROR_TEXT("motor port must be A..D"));
	}
	return (est_motor_port_t)(port_name[0] - 'A');
}

static void modest_raise_motor_error(est_result_t error)
{
	switch (error) {
	case EST_ERR_INVALID_ARGUMENT:
		mp_raise_ValueError(MP_ERROR_TEXT("invalid motor argument"));
	case EST_ERR_NOT_CONNECTED:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor not connected"));
	case EST_ERR_TYPE_MISMATCH:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor type unsupported"));
	case EST_ERR_BUSY:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor busy"));
	case EST_ERR_NOT_SUPPORTED:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor operation unsupported"));
	default:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor operation failed"));
	}
}

static void modest_raise_pair_error(est_result_t error)
{
	if (error == EST_ERR_TYPE_MISMATCH) {
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("motor pair types must match"));
	}
	modest_raise_motor_error(error);
}

static void modest_motor_read(mp_obj_t self_object,
	est_motor_status_t *status)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_result_t result = est_motor_get_status(self->port, status);

	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
}

static mp_obj_t modest_motor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	modest_motor_instance_t *self;

	mp_arg_check_num(argument_count, keyword_count, 1U, 1U, false);
	self = mp_obj_malloc(modest_motor_instance_t, type);
	self->port = modest_motor_port_from_object(arguments[0]);
	return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t modest_motor_port(mp_obj_t self_object)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	char port_name = (char)('A' + self->port);

	return mp_obj_new_str(&port_name, 1U);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_port_obj, modest_motor_port);

static mp_obj_t modest_motor_type_value(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.type);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_motor_type_value_obj, modest_motor_type_value);

static mp_obj_t modest_motor_state(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_state_obj, modest_motor_state);

static mp_obj_t modest_motor_stop_mode(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.stop_mode);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_motor_stop_mode_obj, modest_motor_stop_mode);

static mp_obj_t modest_motor_power(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.power_percent);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_power_obj, modest_motor_power);

static mp_obj_t modest_motor_target_speed(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.target_speed_percent);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_motor_target_speed_obj, modest_motor_target_speed);

static mp_obj_t modest_motor_speed(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.actual_speed_percent);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_speed_obj, modest_motor_speed);

static mp_obj_t modest_motor_angle(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.angle_degrees);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_angle_obj, modest_motor_angle);

static mp_obj_t modest_motor_error(mp_obj_t self_object)
{
	est_motor_status_t status = {0};

	modest_motor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.error);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_error_obj, modest_motor_error);

static mp_obj_t modest_motor_stalled(mp_obj_t self_object)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	bool stalled = false;
	est_result_t result = est_motor_stalled(self->port, &stalled);

	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_obj_new_bool(stalled);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_motor_stalled_obj, modest_motor_stalled);

static mp_obj_t modest_motor_status(mp_obj_t self_object)
{
	est_motor_status_t status = {0};
	mp_obj_t values[8];

	modest_motor_read(self_object, &status);
	values[0] = mp_obj_new_int((mp_int_t)status.error);
	values[1] = mp_obj_new_int((mp_int_t)status.type);
	values[2] = mp_obj_new_int((mp_int_t)status.state);
	values[3] = mp_obj_new_int((mp_int_t)status.stop_mode);
	values[4] = mp_obj_new_int((mp_int_t)status.power_percent);
	values[5] = mp_obj_new_int((mp_int_t)status.target_speed_percent);
	values[6] = mp_obj_new_int((mp_int_t)status.actual_speed_percent);
	values[7] = mp_obj_new_int((mp_int_t)status.angle_degrees);
	return mp_obj_new_tuple(8U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_motor_status_obj, modest_motor_status);

static mp_obj_t modest_motor_stop(size_t argument_count,
	const mp_obj_t *arguments)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(arguments[0]);
	est_stop_mode_t stop_mode = EST_STOP_COAST;
	est_result_t result;

	if (argument_count == 2U) {
		int32_t requested_mode = require_integer_range(arguments[1],
			EST_STOP_COAST, EST_STOP_HOLD,
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE or HOLD"));

		stop_mode = (est_stop_mode_t)requested_mode;
	}
	result = est_motor_stop(self->port, stop_mode);
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_motor_stop_obj, 1, 2, modest_motor_stop);

static mp_obj_t modest_motor_run_power(mp_obj_t self_object,
	mp_obj_t power_object)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	int32_t power = require_integer_range(power_object, -100, 100,
		MP_ERROR_TEXT("power must be -100..100"));
	est_result_t result = est_motor_set_power(
		self->port, (int8_t)power);

	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
	modest_motor_run_power_obj, modest_motor_run_power);

static mp_obj_t modest_motor_run_speed(mp_obj_t self_object,
	mp_obj_t speed_object)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	int32_t speed = require_integer_range(speed_object, -100, 100,
		MP_ERROR_TEXT("speed must be -100..100"));
	est_result_t result;

	result = est_motor_run_speed(self->port, (int8_t)speed);
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
	modest_motor_run_speed_obj, modest_motor_run_speed);

static mp_obj_t modest_motor_run_time(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_duration_ms, ARG_speed, ARG_stop };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_duration_ms, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t duration_ms;
	int32_t speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	duration_ms = (int32_t)arguments[ARG_duration_ms].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (duration_ms < 1 || duration_ms > 600000) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("duration_ms must be 1..600000"));
	}
	if (speed < -100 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be -100..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE or HOLD"));
	}
	result = est_motor_run_time(self->port, (int8_t)speed,
		(uint32_t)duration_ms, (est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_motor_run_time_obj, 1, modest_motor_run_time);

static mp_obj_t modest_motor_run_angle(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_degrees, ARG_speed, ARG_stop };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_degrees, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t degrees;
	int32_t speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	degrees = (int32_t)arguments[ARG_degrees].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (degrees == 0 || degrees < -3600 || degrees > 3600) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("degrees must be -3600..-1 or 1..3600"));
	}
	if (speed < 0 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be 0..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE or HOLD"));
	}
	result = est_motor_run_angle(self->port, degrees, (uint8_t)speed,
		(est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_motor_run_angle_obj, 1, modest_motor_run_angle);

static mp_obj_t modest_motor_reset_angle(mp_obj_t self_object)
{
	modest_motor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_result_t result = est_motor_reset_angle(self->port);

	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_motor_reset_angle_obj, modest_motor_reset_angle);

static const mp_rom_map_elem_t modest_motor_locals_table[] = {
	{MP_ROM_QSTR(MP_QSTR_port), MP_ROM_PTR(&modest_motor_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_type), MP_ROM_PTR(&modest_motor_type_value_obj)},
	{MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&modest_motor_state_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop_mode),
		MP_ROM_PTR(&modest_motor_stop_mode_obj)},
	{MP_ROM_QSTR(MP_QSTR_power), MP_ROM_PTR(&modest_motor_power_obj)},
	{MP_ROM_QSTR(MP_QSTR_target_speed),
		MP_ROM_PTR(&modest_motor_target_speed_obj)},
	{MP_ROM_QSTR(MP_QSTR_speed), MP_ROM_PTR(&modest_motor_speed_obj)},
	{MP_ROM_QSTR(MP_QSTR_angle), MP_ROM_PTR(&modest_motor_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_error), MP_ROM_PTR(&modest_motor_error_obj)},
	{MP_ROM_QSTR(MP_QSTR_stalled), MP_ROM_PTR(&modest_motor_stalled_obj)},
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_motor_status_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&modest_motor_stop_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_power),
		MP_ROM_PTR(&modest_motor_run_power_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_speed),
		MP_ROM_PTR(&modest_motor_run_speed_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_time),
		MP_ROM_PTR(&modest_motor_run_time_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_angle),
		MP_ROM_PTR(&modest_motor_run_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_reset_angle),
		MP_ROM_PTR(&modest_motor_reset_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_NONE), MP_ROM_INT(EST_MOTOR_TYPE_NONE)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_LARGE), MP_ROM_INT(EST_MOTOR_TYPE_LARGE)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_MEDIUM), MP_ROM_INT(EST_MOTOR_TYPE_MEDIUM)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_UNKNOWN),
		MP_ROM_INT(EST_MOTOR_TYPE_UNKNOWN)},
	{MP_ROM_QSTR(MP_QSTR_STATE_IDLE), MP_ROM_INT(EST_MOTOR_IDLE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_POWER), MP_ROM_INT(EST_MOTOR_POWER)},
	{MP_ROM_QSTR(MP_QSTR_STATE_SPEED), MP_ROM_INT(EST_MOTOR_SPEED)},
	{MP_ROM_QSTR(MP_QSTR_STATE_POSITION), MP_ROM_INT(EST_MOTOR_POSITION)},
	{MP_ROM_QSTR(MP_QSTR_STATE_TIMED), MP_ROM_INT(EST_MOTOR_TIMED)},
	{MP_ROM_QSTR(MP_QSTR_STATE_HOLDING), MP_ROM_INT(EST_MOTOR_HOLDING)},
	{MP_ROM_QSTR(MP_QSTR_STATE_FAULT), MP_ROM_INT(EST_MOTOR_FAULT)},
	{MP_ROM_QSTR(MP_QSTR_STOP_COAST), MP_ROM_INT(EST_STOP_COAST)},
	{MP_ROM_QSTR(MP_QSTR_STOP_BRAKE), MP_ROM_INT(EST_STOP_BRAKE)},
	{MP_ROM_QSTR(MP_QSTR_STOP_HOLD), MP_ROM_INT(EST_STOP_HOLD)},
};
static MP_DEFINE_CONST_DICT(modest_motor_locals, modest_motor_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	modest_motor_class,
	MP_QSTR_Motor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_motor_make_new,
	locals_dict, &modest_motor_locals);

typedef enum {
	MODEST_PAIR_NONE = 0,
	MODEST_PAIR_POSITION = 1,
	MODEST_PAIR_SPEED = 2,
	MODEST_PAIR_MOTION = 3,
	MODEST_PAIR_STEER = 4
} modest_pair_mode_t;

typedef struct {
	mp_obj_base_t base;
	est_motor_port_t left_port;
	est_motor_port_t right_port;
	modest_pair_mode_t mode;
} modest_pair_instance_t;

typedef struct {
	est_drive_state_t state;
	int32_t left_degrees;
	int32_t right_degrees;
	int32_t synchronization_error_degrees;
	int32_t maximum_synchronization_error_degrees;
	est_result_t error;
} modest_pair_snapshot_t;

static mp_obj_t modest_pair_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	modest_pair_instance_t *self;
	est_motor_port_t left_port;
	est_motor_port_t right_port;

	mp_arg_check_num(argument_count, keyword_count, 2U, 2U, false);
	left_port = modest_motor_port_from_object(arguments[0]);
	right_port = modest_motor_port_from_object(arguments[1]);
	if (left_port == right_port) {
		mp_raise_ValueError(MP_ERROR_TEXT("motor ports must differ"));
	}
	self = mp_obj_malloc(modest_pair_instance_t, type);
	self->left_port = left_port;
	self->right_port = right_port;
	self->mode = MODEST_PAIR_NONE;
	return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t modest_pair_left_port(mp_obj_t self_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	char port_name = (char)('A' + self->left_port);

	return mp_obj_new_str(&port_name, 1U);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_pair_left_port_obj, modest_pair_left_port);

static mp_obj_t modest_pair_right_port(mp_obj_t self_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	char port_name = (char)('A' + self->right_port);

	return mp_obj_new_str(&port_name, 1U);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_pair_right_port_obj, modest_pair_right_port);

static void modest_pair_read(modest_pair_instance_t *self,
	modest_pair_snapshot_t *snapshot)
{
	est_result_t result = EST_OK;

	snapshot->state = EST_DRIVE_IDLE;
	snapshot->left_degrees = 0;
	snapshot->right_degrees = 0;
	snapshot->synchronization_error_degrees = 0;
	snapshot->maximum_synchronization_error_degrees = 0;
	snapshot->error = EST_OK;
	if (self->mode == MODEST_PAIR_POSITION) {
		est_drive_status_t status = {0};

		result = est_drive_get_status(&status);
		snapshot->state = status.state;
		snapshot->left_degrees = status.left_actual_degrees;
		snapshot->right_degrees = status.right_actual_degrees;
		snapshot->synchronization_error_degrees =
			status.synchronization_error_degrees;
		snapshot->maximum_synchronization_error_degrees =
			status.maximum_synchronization_error_degrees;
		snapshot->error = status.error;
	} else if (self->mode == MODEST_PAIR_SPEED) {
		est_motor_pair_speed_status_t status = {0};

		result = est_motor_pair_get_speed_status(&status);
		snapshot->state = status.state;
		snapshot->left_degrees = status.left_actual_degrees;
		snapshot->right_degrees = status.right_actual_degrees;
		snapshot->synchronization_error_degrees =
			status.synchronization_error_degrees;
		snapshot->maximum_synchronization_error_degrees =
			status.maximum_synchronization_error_degrees;
		snapshot->error = status.error;
	} else if (self->mode == MODEST_PAIR_MOTION) {
		est_drive_motion_status_t status = {0};

		result = est_drive_get_motion_status(&status);
		snapshot->state = status.state;
		snapshot->left_degrees = status.left_actual_degrees;
		snapshot->right_degrees = status.right_actual_degrees;
		snapshot->synchronization_error_degrees =
			status.synchronization_error_degrees;
		snapshot->maximum_synchronization_error_degrees =
			status.maximum_synchronization_error_degrees;
		snapshot->error = status.error;
	} else if (self->mode == MODEST_PAIR_STEER) {
		est_drive_steer_for_status_t status = {0};

		result = est_drive_get_steer_for_status(&status);
		snapshot->state = status.state;
		snapshot->left_degrees = status.left_actual_degrees;
		snapshot->right_degrees = status.right_actual_degrees;
		snapshot->synchronization_error_degrees =
			status.synchronization_error_degrees;
		snapshot->maximum_synchronization_error_degrees =
			status.maximum_synchronization_error_degrees;
		snapshot->error = status.error;
	}
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
}

static void modest_pair_wait(modest_pair_instance_t *self)
{
	for (;;) {
		modest_pair_snapshot_t snapshot;

		modest_pair_read(self, &snapshot);
		if (snapshot.state == EST_DRIVE_COMPLETE) {
			return;
		}
		if (snapshot.state == EST_DRIVE_FAULT) {
			modest_raise_motor_error(snapshot.error);
		}
		if (snapshot.state != EST_DRIVE_RUNNING) {
			mp_raise_msg(&mp_type_RuntimeError,
				MP_ERROR_TEXT("motor pair stopped"));
		}
		est_micropython_vm_hook();
	}
}

static mp_obj_t modest_pair_state(mp_obj_t self_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	modest_pair_snapshot_t snapshot;

	modest_pair_read(self, &snapshot);
	return mp_obj_new_int((mp_int_t)snapshot.state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_pair_state_obj, modest_pair_state);

static mp_obj_t modest_pair_running(mp_obj_t self_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	modest_pair_snapshot_t snapshot;

	modest_pair_read(self, &snapshot);
	return mp_obj_new_bool(snapshot.state == EST_DRIVE_RUNNING);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_pair_running_obj, modest_pair_running);

static mp_obj_t modest_pair_status(mp_obj_t self_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	modest_pair_snapshot_t snapshot;
	mp_obj_t values[6];

	modest_pair_read(self, &snapshot);
	values[0] = mp_obj_new_int((mp_int_t)snapshot.error);
	values[1] = mp_obj_new_int((mp_int_t)snapshot.state);
	values[2] = mp_obj_new_int((mp_int_t)snapshot.left_degrees);
	values[3] = mp_obj_new_int((mp_int_t)snapshot.right_degrees);
	values[4] = mp_obj_new_int(
		(mp_int_t)snapshot.synchronization_error_degrees);
	values[5] = mp_obj_new_int(
		(mp_int_t)snapshot.maximum_synchronization_error_degrees);
	return mp_obj_new_tuple(6U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_pair_status_obj, modest_pair_status);

static mp_obj_t modest_pair_stop(size_t argument_count,
	const mp_obj_t *arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(arguments[0]);
	est_stop_mode_t stop_mode = EST_STOP_COAST;
	est_result_t result;

	if (argument_count == 2U) {
		stop_mode = (est_stop_mode_t)require_integer_range(arguments[1],
			EST_STOP_COAST, EST_STOP_HOLD,
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	result = est_motor_pair_stop(stop_mode);
	if (result == EST_ERR_STATE) {
		(void)est_motor_stop(self->left_port, stop_mode);
		(void)est_motor_stop(self->right_port, stop_mode);
		return mp_const_none;
	}
	if (result != EST_OK) {
		modest_raise_motor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_pair_stop_obj, 1, 2, modest_pair_stop);

static mp_obj_t modest_motor_pair_run_speed(mp_obj_t self_object,
	mp_obj_t left_speed_object, mp_obj_t right_speed_object)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(self_object);
	int32_t left_speed = require_integer_range(left_speed_object, -100, 100,
		MP_ERROR_TEXT("left_speed must be -100..100"));
	int32_t right_speed = require_integer_range(right_speed_object, -100, 100,
		MP_ERROR_TEXT("right_speed must be -100..100"));
	est_result_t result;

	result = est_motor_pair_run_speeds(self->left_port,
		(int8_t)left_speed, self->right_port, (int8_t)right_speed);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_SPEED;
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(
	modest_motor_pair_run_speed_obj, modest_motor_pair_run_speed);

static mp_obj_t modest_motor_pair_run_time(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_duration_ms, ARG_left_speed, ARG_right_speed, ARG_stop,
		ARG_wait };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_duration_ms, MP_ARG_REQUIRED | MP_ARG_INT,
			{.u_int = 0}},
		{MP_QSTR_left_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_right_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
		{MP_QSTR_wait, MP_ARG_BOOL, {.u_bool = false}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t duration_ms;
	int32_t left_speed;
	int32_t right_speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	duration_ms = (int32_t)arguments[ARG_duration_ms].u_int;
	left_speed = (int32_t)arguments[ARG_left_speed].u_int;
	right_speed = (int32_t)arguments[ARG_right_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (duration_ms < 1 || duration_ms > 600000) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("duration_ms must be 1..600000"));
	}
	if (left_speed < -100 || left_speed > 100 ||
	    right_speed < -100 || right_speed > 100) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("pair speeds must be -100..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	result = est_motor_pair_run_speeds_for_time(self->left_port,
		(int8_t)left_speed, self->right_port, (int8_t)right_speed,
		(uint32_t)duration_ms, (est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_SPEED;
	if (arguments[ARG_wait].u_bool) {
		modest_pair_wait(self);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_motor_pair_run_time_obj, 1, modest_motor_pair_run_time);

static mp_obj_t modest_motor_pair_run_angle(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_left_degrees, ARG_right_degrees, ARG_speed, ARG_stop,
		ARG_wait };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_left_degrees, MP_ARG_REQUIRED | MP_ARG_INT,
			{.u_int = 0}},
		{MP_QSTR_right_degrees, MP_ARG_REQUIRED | MP_ARG_INT,
			{.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
		{MP_QSTR_wait, MP_ARG_BOOL, {.u_bool = false}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t left_degrees;
	int32_t right_degrees;
	int32_t speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	left_degrees = (int32_t)arguments[ARG_left_degrees].u_int;
	right_degrees = (int32_t)arguments[ARG_right_degrees].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (left_degrees == 0 || left_degrees < -3600 ||
	    left_degrees > 3600 || right_degrees == 0 ||
	    right_degrees < -3600 || right_degrees > 3600) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("pair degrees must be nonzero and within 3600"));
	}
	if (speed < 0 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be 0..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	result = est_motor_pair_run_angles(self->left_port, left_degrees,
		self->right_port, right_degrees, (uint8_t)speed,
		(est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_POSITION;
	if (arguments[ARG_wait].u_bool) {
		modest_pair_wait(self);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_motor_pair_run_angle_obj, 1, modest_motor_pair_run_angle);

static const mp_rom_map_elem_t modest_motor_pair_locals_table[] = {
	{MP_ROM_QSTR(MP_QSTR_left_port),
		MP_ROM_PTR(&modest_pair_left_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_right_port),
		MP_ROM_PTR(&modest_pair_right_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&modest_pair_state_obj)},
	{MP_ROM_QSTR(MP_QSTR_running), MP_ROM_PTR(&modest_pair_running_obj)},
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_pair_status_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&modest_pair_stop_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_speed),
		MP_ROM_PTR(&modest_motor_pair_run_speed_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_time),
		MP_ROM_PTR(&modest_motor_pair_run_time_obj)},
	{MP_ROM_QSTR(MP_QSTR_run_angle),
		MP_ROM_PTR(&modest_motor_pair_run_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_STATE_IDLE), MP_ROM_INT(EST_DRIVE_IDLE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_RUNNING), MP_ROM_INT(EST_DRIVE_RUNNING)},
	{MP_ROM_QSTR(MP_QSTR_STATE_COMPLETE),
		MP_ROM_INT(EST_DRIVE_COMPLETE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_FAULT), MP_ROM_INT(EST_DRIVE_FAULT)},
	{MP_ROM_QSTR(MP_QSTR_STOP_COAST), MP_ROM_INT(EST_STOP_COAST)},
	{MP_ROM_QSTR(MP_QSTR_STOP_BRAKE), MP_ROM_INT(EST_STOP_BRAKE)},
	{MP_ROM_QSTR(MP_QSTR_STOP_HOLD), MP_ROM_INT(EST_STOP_HOLD)},
};
static MP_DEFINE_CONST_DICT(
	modest_motor_pair_locals, modest_motor_pair_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	modest_motor_pair_class,
	MP_QSTR_MotorPair,
	MP_TYPE_FLAG_NONE,
	make_new, modest_pair_make_new,
	locals_dict, &modest_motor_pair_locals);

static mp_obj_t modest_drive_straight_angle(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_degrees, ARG_speed, ARG_stop, ARG_wait };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_degrees, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
		{MP_QSTR_wait, MP_ARG_BOOL, {.u_bool = false}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t degrees;
	int32_t speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	degrees = (int32_t)arguments[ARG_degrees].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (degrees == 0 || degrees < -3600 || degrees > 3600) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("degrees must be -3600..-1 or 1..3600"));
	}
	if (speed < 0 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be 0..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	result = est_drive_run_degrees(self->left_port, self->right_port,
		degrees, (uint8_t)speed, (est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_MOTION;
	if (arguments[ARG_wait].u_bool) {
		modest_pair_wait(self);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_drive_straight_angle_obj, 1, modest_drive_straight_angle);

static mp_obj_t modest_drive_straight_time(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_duration_ms, ARG_speed, ARG_stop, ARG_wait };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_duration_ms, MP_ARG_REQUIRED | MP_ARG_INT,
			{.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
		{MP_QSTR_wait, MP_ARG_BOOL, {.u_bool = false}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t duration_ms;
	int32_t speed;
	int32_t stop_mode;
	int32_t signed_duration;
	int32_t speed_magnitude;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	duration_ms = (int32_t)arguments[ARG_duration_ms].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (duration_ms < 1 || duration_ms > 600000) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("duration_ms must be 1..600000"));
	}
	if (speed < -100 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be -100..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	signed_duration = speed < 0 ? -duration_ms : duration_ms;
	speed_magnitude = speed < 0 ? -speed : speed;
	result = est_drive_run_time(self->left_port, self->right_port,
		signed_duration, (uint8_t)speed_magnitude,
		(est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_MOTION;
	if (arguments[ARG_wait].u_bool) {
		modest_pair_wait(self);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_drive_straight_time_obj, 1, modest_drive_straight_time);

static mp_obj_t modest_drive_steer(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_steering, ARG_speed };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_steering, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t steering;
	int32_t speed;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	steering = (int32_t)arguments[ARG_steering].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	if (steering < -100 || steering > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("steering must be -100..100"));
	}
	if (speed < -100 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be -100..100"));
	}
	result = est_drive_start_steer(self->left_port, self->right_port,
		(int8_t)steering, (int8_t)speed);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_SPEED;
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_drive_steer_obj, 1, modest_drive_steer);

static mp_obj_t modest_drive_steer_for(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments,
	est_drive_target_mode_t target_mode, qstr target_name)
{
	modest_pair_instance_t *self = MP_OBJ_TO_PTR(positional_arguments[0]);
	enum { ARG_steering, ARG_target, ARG_speed, ARG_stop, ARG_wait };
	const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_steering, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{target_name, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
		{MP_QSTR_speed, MP_ARG_INT, {.u_int = 50}},
		{MP_QSTR_stop, MP_ARG_INT, {.u_int = EST_STOP_COAST}},
		{MP_QSTR_wait, MP_ARG_BOOL, {.u_bool = false}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	int32_t steering;
	int32_t target;
	int32_t speed;
	int32_t stop_mode;
	est_result_t result;

	mp_arg_parse_all(argument_count - 1U, positional_arguments + 1,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	steering = (int32_t)arguments[ARG_steering].u_int;
	target = (int32_t)arguments[ARG_target].u_int;
	speed = (int32_t)arguments[ARG_speed].u_int;
	stop_mode = (int32_t)arguments[ARG_stop].u_int;
	if (steering < -100 || steering > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("steering must be -100..100"));
	}
	if (target < 1 ||
	    (target_mode == EST_DRIVE_TARGET_DEGREES && target > 3600) ||
	    (target_mode == EST_DRIVE_TARGET_TIME_MS && target > 600000)) {
		mp_raise_ValueError(MP_ERROR_TEXT("drive target out of range"));
	}
	if (speed < -100 || speed > 100) {
		mp_raise_ValueError(MP_ERROR_TEXT("speed must be -100..100"));
	}
	if (stop_mode < EST_STOP_COAST || stop_mode > EST_STOP_HOLD) {
		mp_raise_ValueError(
			MP_ERROR_TEXT("stop mode must be COAST, BRAKE, or HOLD"));
	}
	result = est_drive_steer_for(self->left_port, self->right_port,
		target_mode, (int8_t)steering, (int8_t)speed, target,
		(est_stop_mode_t)stop_mode);
	if (result != EST_OK) {
		modest_raise_pair_error(result);
	}
	self->mode = MODEST_PAIR_STEER;
	if (arguments[ARG_wait].u_bool) {
		modest_pair_wait(self);
	}
	return mp_const_none;
}

static mp_obj_t modest_drive_steer_angle(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	return modest_drive_steer_for(argument_count, positional_arguments,
		keyword_arguments, EST_DRIVE_TARGET_DEGREES, MP_QSTR_degrees);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_drive_steer_angle_obj, 1, modest_drive_steer_angle);

static mp_obj_t modest_drive_steer_time(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	return modest_drive_steer_for(argument_count, positional_arguments,
		keyword_arguments, EST_DRIVE_TARGET_TIME_MS,
		MP_QSTR_duration_ms);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_drive_steer_time_obj, 1, modest_drive_steer_time);

static const mp_rom_map_elem_t modest_drive_base_locals_table[] = {
	{MP_ROM_QSTR(MP_QSTR_left_port),
		MP_ROM_PTR(&modest_pair_left_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_right_port),
		MP_ROM_PTR(&modest_pair_right_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&modest_pair_state_obj)},
	{MP_ROM_QSTR(MP_QSTR_running), MP_ROM_PTR(&modest_pair_running_obj)},
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_pair_status_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&modest_pair_stop_obj)},
	{MP_ROM_QSTR(MP_QSTR_straight_angle),
		MP_ROM_PTR(&modest_drive_straight_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_straight_time),
		MP_ROM_PTR(&modest_drive_straight_time_obj)},
	{MP_ROM_QSTR(MP_QSTR_steer), MP_ROM_PTR(&modest_drive_steer_obj)},
	{MP_ROM_QSTR(MP_QSTR_steer_angle),
		MP_ROM_PTR(&modest_drive_steer_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_steer_time),
		MP_ROM_PTR(&modest_drive_steer_time_obj)},
	{MP_ROM_QSTR(MP_QSTR_STATE_IDLE), MP_ROM_INT(EST_DRIVE_IDLE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_RUNNING), MP_ROM_INT(EST_DRIVE_RUNNING)},
	{MP_ROM_QSTR(MP_QSTR_STATE_COMPLETE),
		MP_ROM_INT(EST_DRIVE_COMPLETE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_FAULT), MP_ROM_INT(EST_DRIVE_FAULT)},
	{MP_ROM_QSTR(MP_QSTR_STOP_COAST), MP_ROM_INT(EST_STOP_COAST)},
	{MP_ROM_QSTR(MP_QSTR_STOP_BRAKE), MP_ROM_INT(EST_STOP_BRAKE)},
	{MP_ROM_QSTR(MP_QSTR_STOP_HOLD), MP_ROM_INT(EST_STOP_HOLD)},
};
static MP_DEFINE_CONST_DICT(
	modest_drive_base_locals, modest_drive_base_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	modest_drive_base_class,
	MP_QSTR_DriveBase,
	MP_TYPE_FLAG_NONE,
	make_new, modest_pair_make_new,
	locals_dict, &modest_drive_base_locals);

static mp_obj_t modest_drive_mix(mp_obj_t steering_object,
	mp_obj_t speed_object)
{
	int32_t steering = require_integer_range(
		steering_object, -100, 100,
		MP_ERROR_TEXT("steering must be -100..100"));
	int32_t speed = require_integer_range(
		speed_object, -100, 100,
		MP_ERROR_TEXT("speed must be -100..100"));
	int8_t left_speed = 0;
	int8_t right_speed = 0;
	mp_obj_t values[2];
	est_result_t result = est_drive_mix_steering(
		(int8_t)steering, (int8_t)speed, &left_speed, &right_speed);

	if (result != EST_OK) {
		mp_raise_ValueError(MP_ERROR_TEXT("invalid drive mix"));
	}
	values[0] = mp_obj_new_int((mp_int_t)left_speed);
	values[1] = mp_obj_new_int((mp_int_t)right_speed);
	return mp_obj_new_tuple(2U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_2(modest_drive_mix_obj, modest_drive_mix);

static mp_obj_t modest_sensor(mp_obj_t port_object)
{
	int32_t port = require_integer_range(
		port_object, 1, EST_SENSOR_PORT_COUNT,
		MP_ERROR_TEXT("sensor port must be 1..4"));
	est_sensor_status_t status = {0};
	mp_obj_t values[6];
	est_result_t result = est_sensor_get_status(
		(est_sensor_port_t)(port - 1), &status);

	values[0] = mp_obj_new_int((mp_int_t)result);
	values[1] = mp_obj_new_int((mp_int_t)status.type);
	values[2] = mp_obj_new_int((mp_int_t)status.state);
	values[3] = mp_obj_new_int((mp_int_t)status.mode);
	values[4] = mp_obj_new_bool(status.value_valid);
	values[5] = mp_obj_new_int((mp_int_t)status.value);
	return mp_obj_new_tuple(6U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_obj, modest_sensor);

typedef struct {
	mp_obj_base_t base;
	est_sensor_port_t port;
	est_sensor_type_t expected_type;
	bool type_constrained;
	int32_t gyro_zero_degrees;
} modest_sensor_instance_t;

static void modest_raise_sensor_error(est_result_t error)
{
	switch (error) {
	case EST_ERR_INVALID_ARGUMENT:
		mp_raise_ValueError(MP_ERROR_TEXT("invalid sensor argument"));
	case EST_ERR_NOT_CONNECTED:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor not connected"));
	case EST_ERR_TYPE_MISMATCH:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor type mismatch"));
	case EST_ERR_NOT_SUPPORTED:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor mode unsupported"));
	case EST_ERR_BUSY:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor not ready"));
	case EST_ERR_TIMEOUT:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor data stale"));
	default:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor read failed"));
	}
}

static void modest_sensor_require_type(modest_sensor_instance_t *self,
	const est_sensor_status_t *status)
{
	if (!self->type_constrained || status->type == self->expected_type) {
		return;
	}
	if (status->state == EST_SENSOR_SYNCING) {
		modest_raise_sensor_error(EST_ERR_BUSY);
	}
	if (status->type == EST_SENSOR_TYPE_NONE) {
		modest_raise_sensor_error(EST_ERR_NOT_CONNECTED);
	}
	modest_raise_sensor_error(EST_ERR_TYPE_MISMATCH);
}

static void modest_sensor_read(mp_obj_t self_object,
	est_sensor_status_t *status)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_result_t result = est_sensor_get_status(self->port, status);

	if (result != EST_OK) {
		modest_raise_sensor_error(result);
	}
	modest_sensor_require_type(self, status);
}

static mp_obj_t modest_sensor_make_new_for_type(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments,
	est_sensor_type_t expected_type, bool type_constrained)
{
	int32_t port;
	modest_sensor_instance_t *self;
	est_sensor_status_t status = {0};
	est_result_t result;

	mp_arg_check_num(argument_count, keyword_count, 1U, 1U, false);
	port = require_integer_range(arguments[0], 1, EST_SENSOR_PORT_COUNT,
		MP_ERROR_TEXT("sensor port must be 1..4"));
	self = mp_obj_malloc(modest_sensor_instance_t, type);
	self->port = (est_sensor_port_t)(port - 1);
	self->expected_type = expected_type;
	self->type_constrained = type_constrained;
	self->gyro_zero_degrees = 0;
	if (type_constrained) {
		result = est_sensor_get_status(self->port, &status);
		if (result != EST_OK) {
			modest_raise_sensor_error(result);
		}
		modest_sensor_require_type(self, &status);
	}
	return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t modest_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_UNKNOWN, false);
}

static mp_obj_t modest_sound_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_SOUND, true);
}

static mp_obj_t modest_temperature_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_TEMPERATURE, true);
}

static mp_obj_t modest_touch_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_TOUCH, true);
}

static mp_obj_t modest_color_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_COLOR, true);
}

static mp_obj_t modest_ultrasonic_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_ULTRASONIC, true);
}

static mp_obj_t modest_gyro_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_GYRO, true);
}

static mp_obj_t modest_infrared_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	return modest_sensor_make_new_for_type(type, argument_count, keyword_count,
		arguments, EST_SENSOR_TYPE_INFRARED, true);
}

static mp_obj_t modest_sensor_port(mp_obj_t self_object)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);

	return mp_obj_new_int((mp_int_t)self->port + 1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_port_obj, modest_sensor_port);

static mp_obj_t modest_sensor_type_value(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.type);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_sensor_type_value_obj, modest_sensor_type_value);

static mp_obj_t modest_sensor_state(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.state);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_state_obj, modest_sensor_state);

static mp_obj_t modest_sensor_mode(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.mode);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_mode_obj, modest_sensor_mode);

static mp_obj_t modest_sensor_value_format(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.value_format);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_sensor_value_format_obj, modest_sensor_value_format);

static mp_obj_t modest_sensor_valid(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_bool(status.value_valid && status.error == EST_OK);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_valid_obj, modest_sensor_valid);

static mp_obj_t modest_sensor_error(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	return mp_obj_new_int((mp_int_t)status.error);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_error_obj, modest_sensor_error);

static mp_obj_t modest_sensor_value(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};

	modest_sensor_read(self_object, &status);
	if (status.error != EST_OK) {
		modest_raise_sensor_error(status.error);
	}
	if (!status.value_valid) {
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor value unavailable"));
	}
	return mp_obj_new_int((mp_int_t)status.value);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_value_obj, modest_sensor_value);

static mp_obj_t modest_sensor_status(mp_obj_t self_object)
{
	est_sensor_status_t status = {0};
	mp_obj_t values[7];

	modest_sensor_read(self_object, &status);
	values[0] = mp_obj_new_int((mp_int_t)status.error);
	values[1] = mp_obj_new_int((mp_int_t)status.type);
	values[2] = mp_obj_new_int((mp_int_t)status.state);
	values[3] = mp_obj_new_int((mp_int_t)status.mode);
	values[4] = mp_obj_new_int((mp_int_t)status.value_format);
	values[5] = mp_obj_new_bool(status.value_valid);
	values[6] = mp_obj_new_int((mp_int_t)status.value);
	return mp_obj_new_tuple(7U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_status_obj, modest_sensor_status);

#define MODEST_SENSOR_VALUE_TIMEOUT_MS 3000U
#define MODEST_SENSOR_MODE_RETRY_MS 500U

static int32_t modest_sensor_wait_value(mp_obj_t self_object,
	est_sensor_mode_t requested_mode)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_sensor_status_t status = {0};
	est_sensor_type_t expected_type = self->expected_type;
	est_result_t result;
	uint32_t started_ms = est_system_millis();
	uint32_t last_request_ms = started_ms;
	bool mode_requested = false;

	result = est_sensor_get_status(self->port, &status);
	if (result != EST_OK) {
		modest_raise_sensor_error(result);
	}
	if (!self->type_constrained) {
		if (status.state == EST_SENSOR_SYNCING) {
			modest_raise_sensor_error(EST_ERR_BUSY);
		}
		if (status.type == EST_SENSOR_TYPE_NONE) {
			modest_raise_sensor_error(EST_ERR_NOT_CONNECTED);
		}
		expected_type = status.type;
	}
	for (;;) {
		if (status.type == expected_type) {
			uint32_t now_ms = est_system_millis();

			if ((status.mode != requested_mode || !status.value_valid) &&
			    (!mode_requested ||
			     (uint32_t)(now_ms - last_request_ms) >=
				MODEST_SENSOR_MODE_RETRY_MS)) {
				result = est_sensor_set_mode(self->port, requested_mode);
				if (result != EST_OK) {
					modest_raise_sensor_error(result);
				}
				mode_requested = true;
				last_request_ms = now_ms;
			} else if (status.state == EST_SENSOR_STREAMING &&
			    status.mode == requested_mode && status.value_valid &&
			    status.error == EST_OK) {
				return status.value;
			}
		} else if (status.state != EST_SENSOR_SYNCING) {
			if (status.type == EST_SENSOR_TYPE_NONE) {
				modest_raise_sensor_error(EST_ERR_NOT_CONNECTED);
			}
			modest_raise_sensor_error(EST_ERR_TYPE_MISMATCH);
		}
		if ((uint32_t)(est_system_millis() - started_ms) >=
		    MODEST_SENSOR_VALUE_TIMEOUT_MS) {
			mp_raise_msg(&mp_type_RuntimeError,
				MP_ERROR_TEXT("sensor mode timeout"));
		}
		est_micropython_vm_hook();
		result = est_sensor_get_status(self->port, &status);
		if (result != EST_OK) {
			modest_raise_sensor_error(result);
		}
	}
}

static mp_obj_t modest_sensor_set_mode(mp_obj_t self_object,
	mp_obj_t mode_object)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_sensor_status_t status = {0};
	int32_t mode = require_integer_range(mode_object, 0, 2,
		MP_ERROR_TEXT("sensor mode must be 0..2"));
	est_result_t result;

	modest_sensor_read(self_object, &status);
	result = est_sensor_set_mode(self->port, (est_sensor_mode_t)mode);
	if (result != EST_OK) {
		modest_raise_sensor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
	modest_sensor_set_mode_obj, modest_sensor_set_mode);

static mp_obj_t modest_sensor_read_mode(mp_obj_t self_object,
	mp_obj_t mode_object)
{
	int32_t mode = require_integer_range(mode_object, 0, 2,
		MP_ERROR_TEXT("sensor mode must be 0..2"));

	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, (est_sensor_mode_t)mode));
}
static MP_DEFINE_CONST_FUN_OBJ_2(
	modest_sensor_read_mode_obj, modest_sensor_read_mode);

static mp_obj_t modest_sensor_restart(mp_obj_t self_object)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_sensor_status_t status = {0};
	est_result_t result;

	modest_sensor_read(self_object, &status);
	result = est_sensor_restart(self->port);
	if (result != EST_OK) {
		modest_raise_sensor_error(result);
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sensor_restart_obj, modest_sensor_restart);

static mp_obj_t modest_sound_db(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_SOUND_DB));
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_sound_db_obj, modest_sound_db);

static mp_obj_t modest_temperature_celsius(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_CELSIUS));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_temperature_celsius_obj, modest_temperature_celsius);

static mp_obj_t modest_temperature_fahrenheit(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_FAHRENHEIT));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_temperature_fahrenheit_obj, modest_temperature_fahrenheit);

static mp_obj_t modest_touch_pressed(mp_obj_t self_object)
{
	return mp_obj_new_bool(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_REFLECTED) != 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_touch_pressed_obj, modest_touch_pressed);

static mp_obj_t modest_color_reflection(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_REFLECTED));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_color_reflection_obj, modest_color_reflection);

static mp_obj_t modest_color_ambient(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_AMBIENT));
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_color_ambient_obj, modest_color_ambient);

static mp_obj_t modest_color_color(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_COLOR));
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_color_color_obj, modest_color_color);

static mp_obj_t modest_ultrasonic_distance_mm(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_DISTANCE_CM));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_ultrasonic_distance_mm_obj, modest_ultrasonic_distance_mm);

static mp_obj_t modest_ultrasonic_inches_tenths(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_DISTANCE_INCH));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_ultrasonic_inches_tenths_obj, modest_ultrasonic_inches_tenths);

static mp_obj_t modest_ultrasonic_presence(mp_obj_t self_object)
{
	return mp_obj_new_bool(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_PRESENCE) != 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_ultrasonic_presence_obj, modest_ultrasonic_presence);

static mp_obj_t modest_gyro_angle(mp_obj_t self_object)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	int32_t raw_angle = modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_GYRO_ANGLE);

	return mp_obj_new_int(raw_angle - self->gyro_zero_degrees);
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_gyro_angle_obj, modest_gyro_angle);

static mp_obj_t modest_gyro_speed(mp_obj_t self_object)
{
	return mp_obj_new_int(modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_GYRO_RATE));
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_gyro_speed_obj, modest_gyro_speed);

static mp_obj_t modest_gyro_reset_angle(mp_obj_t self_object)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);

	self->gyro_zero_degrees = modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_GYRO_ANGLE);
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_gyro_reset_angle_obj, modest_gyro_reset_angle);

static mp_obj_t modest_infrared_proximity(mp_obj_t self_object)
{
	uint16_t value = (uint16_t)modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_IR_PROXIMITY);

	return mp_obj_new_int((mp_int_t)(value & 0xFFU));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_infrared_proximity_obj, modest_infrared_proximity);

static mp_obj_t modest_infrared_beacon(mp_obj_t self_object)
{
	uint16_t value = (uint16_t)modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_IR_BEACON);
	uint8_t heading_byte = (uint8_t)(value & 0xFFU);
	int16_t heading = heading_byte <= 180U ? (int16_t)heading_byte :
		-(int16_t)(255U - heading_byte);
	mp_obj_t values[2];

	values[0] = mp_obj_new_int((mp_int_t)heading);
	values[1] = mp_obj_new_int((mp_int_t)((value >> 8U) & 0xFFU));
	return mp_obj_new_tuple(2U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_infrared_beacon_obj, modest_infrared_beacon);

static mp_obj_t modest_infrared_remote(mp_obj_t self_object)
{
	uint16_t value = (uint16_t)modest_sensor_wait_value(
		self_object, EST_SENSOR_MODE_IR_REMOTE);

	return mp_obj_new_int((mp_int_t)(value & 0xFFU));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_infrared_remote_obj, modest_infrared_remote);

#define MODEST_SENSOR_COMMON_LOCAL_ENTRIES \
	{MP_ROM_QSTR(MP_QSTR_port), MP_ROM_PTR(&modest_sensor_port_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_type), MP_ROM_PTR(&modest_sensor_type_value_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&modest_sensor_state_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&modest_sensor_mode_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_value_format), \
		MP_ROM_PTR(&modest_sensor_value_format_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_valid), MP_ROM_PTR(&modest_sensor_valid_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_error), MP_ROM_PTR(&modest_sensor_error_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&modest_sensor_value_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_sensor_status_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_set_mode), \
		MP_ROM_PTR(&modest_sensor_set_mode_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_read_mode), \
		MP_ROM_PTR(&modest_sensor_read_mode_obj)}, \
	{MP_ROM_QSTR(MP_QSTR_restart), MP_ROM_PTR(&modest_sensor_restart_obj)},

static const mp_rom_map_elem_t modest_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_TYPE_NONE), MP_ROM_INT(EST_SENSOR_TYPE_NONE)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_SOUND), MP_ROM_INT(EST_SENSOR_TYPE_SOUND)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_TEMPERATURE),
		MP_ROM_INT(EST_SENSOR_TYPE_TEMPERATURE)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_TOUCH), MP_ROM_INT(EST_SENSOR_TYPE_TOUCH)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_COLOR), MP_ROM_INT(EST_SENSOR_TYPE_COLOR)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_ULTRASONIC),
		MP_ROM_INT(EST_SENSOR_TYPE_ULTRASONIC)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_GYRO), MP_ROM_INT(EST_SENSOR_TYPE_GYRO)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_INFRARED),
		MP_ROM_INT(EST_SENSOR_TYPE_INFRARED)},
	{MP_ROM_QSTR(MP_QSTR_TYPE_UNKNOWN),
		MP_ROM_INT(EST_SENSOR_TYPE_UNKNOWN)},
	{MP_ROM_QSTR(MP_QSTR_STATE_DISCONNECTED),
		MP_ROM_INT(EST_SENSOR_DISCONNECTED)},
	{MP_ROM_QSTR(MP_QSTR_STATE_SYNCING), MP_ROM_INT(EST_SENSOR_SYNCING)},
	{MP_ROM_QSTR(MP_QSTR_STATE_STREAMING),
		MP_ROM_INT(EST_SENSOR_STREAMING)},
	{MP_ROM_QSTR(MP_QSTR_STATE_STALE), MP_ROM_INT(EST_SENSOR_STALE)},
	{MP_ROM_QSTR(MP_QSTR_STATE_ERROR), MP_ROM_INT(EST_SENSOR_ERROR)},
	{MP_ROM_QSTR(MP_QSTR_MODE_REFLECTED),
		MP_ROM_INT(EST_SENSOR_MODE_REFLECTED)},
	{MP_ROM_QSTR(MP_QSTR_MODE_AMBIENT),
		MP_ROM_INT(EST_SENSOR_MODE_AMBIENT)},
	{MP_ROM_QSTR(MP_QSTR_MODE_COLOR), MP_ROM_INT(EST_SENSOR_MODE_COLOR)},
};
static MP_DEFINE_CONST_DICT(
	modest_sensor_locals, modest_sensor_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	modest_sensor_type,
	MP_QSTR_Sensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_sensor_make_new,
	locals_dict, &modest_sensor_locals);

static const mp_rom_map_elem_t modest_sound_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_db), MP_ROM_PTR(&modest_sound_db_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_DB), MP_ROM_INT(EST_SENSOR_MODE_SOUND_DB)},
};
static MP_DEFINE_CONST_DICT(
	modest_sound_sensor_locals, modest_sound_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_sound_sensor_type,
	MP_QSTR_SoundSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_sound_sensor_make_new,
	locals_dict, &modest_sound_sensor_locals);

static const mp_rom_map_elem_t modest_temperature_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_celsius_tenths),
		MP_ROM_PTR(&modest_temperature_celsius_obj)},
	{MP_ROM_QSTR(MP_QSTR_fahrenheit_tenths),
		MP_ROM_PTR(&modest_temperature_fahrenheit_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_CELSIUS),
		MP_ROM_INT(EST_SENSOR_MODE_CELSIUS)},
	{MP_ROM_QSTR(MP_QSTR_MODE_FAHRENHEIT),
		MP_ROM_INT(EST_SENSOR_MODE_FAHRENHEIT)},
	{MP_ROM_QSTR(MP_QSTR_SCALE_TENTHS), MP_ROM_INT(10)},
};
static MP_DEFINE_CONST_DICT(
	modest_temperature_sensor_locals, modest_temperature_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_temperature_sensor_type,
	MP_QSTR_TemperatureSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_temperature_sensor_make_new,
	locals_dict, &modest_temperature_sensor_locals);

static const mp_rom_map_elem_t modest_touch_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_pressed), MP_ROM_PTR(&modest_touch_pressed_obj)},
};
static MP_DEFINE_CONST_DICT(
	modest_touch_sensor_locals, modest_touch_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_touch_sensor_type,
	MP_QSTR_TouchSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_touch_sensor_make_new,
	locals_dict, &modest_touch_sensor_locals);

static const mp_rom_map_elem_t modest_color_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_reflection),
		MP_ROM_PTR(&modest_color_reflection_obj)},
	{MP_ROM_QSTR(MP_QSTR_ambient), MP_ROM_PTR(&modest_color_ambient_obj)},
	{MP_ROM_QSTR(MP_QSTR_color), MP_ROM_PTR(&modest_color_color_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_REFLECTED),
		MP_ROM_INT(EST_SENSOR_MODE_REFLECTED)},
	{MP_ROM_QSTR(MP_QSTR_MODE_AMBIENT),
		MP_ROM_INT(EST_SENSOR_MODE_AMBIENT)},
	{MP_ROM_QSTR(MP_QSTR_MODE_COLOR), MP_ROM_INT(EST_SENSOR_MODE_COLOR)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_NONE), MP_ROM_INT(0)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_BLACK), MP_ROM_INT(1)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_BLUE), MP_ROM_INT(2)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_GREEN), MP_ROM_INT(3)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_YELLOW), MP_ROM_INT(4)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_RED), MP_ROM_INT(5)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_WHITE), MP_ROM_INT(6)},
	{MP_ROM_QSTR(MP_QSTR_COLOR_BROWN), MP_ROM_INT(7)},
};
static MP_DEFINE_CONST_DICT(
	modest_color_sensor_locals, modest_color_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_color_sensor_type,
	MP_QSTR_ColorSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_color_sensor_make_new,
	locals_dict, &modest_color_sensor_locals);

static const mp_rom_map_elem_t modest_ultrasonic_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_distance_mm),
		MP_ROM_PTR(&modest_ultrasonic_distance_mm_obj)},
	{MP_ROM_QSTR(MP_QSTR_inches_tenths),
		MP_ROM_PTR(&modest_ultrasonic_inches_tenths_obj)},
	{MP_ROM_QSTR(MP_QSTR_presence),
		MP_ROM_PTR(&modest_ultrasonic_presence_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_DISTANCE_MM),
		MP_ROM_INT(EST_SENSOR_MODE_DISTANCE_CM)},
	{MP_ROM_QSTR(MP_QSTR_MODE_DISTANCE_INCH),
		MP_ROM_INT(EST_SENSOR_MODE_DISTANCE_INCH)},
	{MP_ROM_QSTR(MP_QSTR_MODE_PRESENCE),
		MP_ROM_INT(EST_SENSOR_MODE_PRESENCE)},
	{MP_ROM_QSTR(MP_QSTR_SCALE_INCH_TENTHS), MP_ROM_INT(10)},
};
static MP_DEFINE_CONST_DICT(
	modest_ultrasonic_sensor_locals, modest_ultrasonic_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_ultrasonic_sensor_type,
	MP_QSTR_UltrasonicSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_ultrasonic_sensor_make_new,
	locals_dict, &modest_ultrasonic_sensor_locals);

static const mp_rom_map_elem_t modest_gyro_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_angle), MP_ROM_PTR(&modest_gyro_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_speed), MP_ROM_PTR(&modest_gyro_speed_obj)},
	{MP_ROM_QSTR(MP_QSTR_reset_angle),
		MP_ROM_PTR(&modest_gyro_reset_angle_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_ANGLE),
		MP_ROM_INT(EST_SENSOR_MODE_GYRO_ANGLE)},
	{MP_ROM_QSTR(MP_QSTR_MODE_RATE),
		MP_ROM_INT(EST_SENSOR_MODE_GYRO_RATE)},
};
static MP_DEFINE_CONST_DICT(
	modest_gyro_sensor_locals, modest_gyro_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_gyro_sensor_type,
	MP_QSTR_GyroSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_gyro_sensor_make_new,
	locals_dict, &modest_gyro_sensor_locals);

static const mp_rom_map_elem_t modest_infrared_sensor_locals_table[] = {
	MODEST_SENSOR_COMMON_LOCAL_ENTRIES
	{MP_ROM_QSTR(MP_QSTR_proximity),
		MP_ROM_PTR(&modest_infrared_proximity_obj)},
	{MP_ROM_QSTR(MP_QSTR_beacon),
		MP_ROM_PTR(&modest_infrared_beacon_obj)},
	{MP_ROM_QSTR(MP_QSTR_remote),
		MP_ROM_PTR(&modest_infrared_remote_obj)},
	{MP_ROM_QSTR(MP_QSTR_MODE_PROXIMITY),
		MP_ROM_INT(EST_SENSOR_MODE_IR_PROXIMITY)},
	{MP_ROM_QSTR(MP_QSTR_MODE_BEACON),
		MP_ROM_INT(EST_SENSOR_MODE_IR_BEACON)},
	{MP_ROM_QSTR(MP_QSTR_MODE_REMOTE),
		MP_ROM_INT(EST_SENSOR_MODE_IR_REMOTE)},
};
static MP_DEFINE_CONST_DICT(
	modest_infrared_sensor_locals, modest_infrared_sensor_locals_table);
static MP_DEFINE_CONST_OBJ_TYPE(
	modest_infrared_sensor_type,
	MP_QSTR_InfraredSensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_infrared_sensor_make_new,
	locals_dict, &modest_infrared_sensor_locals);

static void modest_require_peripheral_result(est_result_t result)
{
	if (result == EST_OK) {
		return;
	}
	if (result == EST_ERR_INVALID_ARGUMENT) {
		mp_raise_ValueError(MP_ERROR_TEXT("invalid peripheral argument"));
	}
	mp_raise_msg(&mp_type_RuntimeError,
		MP_ERROR_TEXT("peripheral operation failed"));
}

static mp_obj_t modest_display_clear(void)
{
	est_display_clear();
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_display_clear_obj, modest_display_clear);

static mp_obj_t modest_display_pixel(size_t argument_count,
	const mp_obj_t *arguments)
{
	uint16_t x = (uint16_t)require_integer_range(arguments[0], 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x must be 0..179"));
	uint16_t y = (uint16_t)require_integer_range(arguments[1], 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y must be 0..127"));
	bool on = argument_count == 2U || mp_obj_is_true(arguments[2]);

	modest_require_peripheral_result(est_display_pixel(x, y, on));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_display_pixel_obj, 2, 3, modest_display_pixel);

static mp_obj_t modest_display_line(size_t argument_count,
	const mp_obj_t *arguments)
{
	uint16_t x0 = (uint16_t)require_integer_range(arguments[0], 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x0 must be 0..179"));
	uint16_t y0 = (uint16_t)require_integer_range(arguments[1], 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y0 must be 0..127"));
	uint16_t x1 = (uint16_t)require_integer_range(arguments[2], 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x1 must be 0..179"));
	uint16_t y1 = (uint16_t)require_integer_range(arguments[3], 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y1 must be 0..127"));
	bool on = argument_count == 4U || mp_obj_is_true(arguments[4]);

	modest_require_peripheral_result(
		est_display_line(x0, y0, x1, y1, on));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_display_line_obj, 4, 5, modest_display_line);

static mp_obj_t modest_display_rectangle(size_t argument_count,
	const mp_obj_t *arguments)
{
	uint16_t x = (uint16_t)require_integer_range(arguments[0], 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x must be 0..179"));
	uint16_t y = (uint16_t)require_integer_range(arguments[1], 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y must be 0..127"));
	uint16_t width = (uint16_t)require_integer_range(arguments[2], 1,
		EST_DISPLAY_WIDTH, MP_ERROR_TEXT("width must be 1..180"));
	uint16_t height = (uint16_t)require_integer_range(arguments[3], 1,
		EST_DISPLAY_HEIGHT, MP_ERROR_TEXT("height must be 1..128"));
	bool filled = argument_count >= 5U && mp_obj_is_true(arguments[4]);
	bool on = argument_count < 6U || mp_obj_is_true(arguments[5]);

	modest_require_peripheral_result(est_display_rectangle(
		x, y, width, height, filled, on));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_display_rectangle_obj, 4, 6, modest_display_rectangle);

static est_display_font_t modest_display_font_from_object(mp_obj_t font_object)
{
	const char *font;

	if (!mp_obj_is_str(font_object)) {
		mp_raise_ValueError(MP_ERROR_TEXT("invalid display font"));
	}
	font = mp_obj_str_get_str(font_object);
	if (strcmp(font, "regular_black") == 0) {
		return EST_DISPLAY_FONT_REGULAR_BLACK;
	}
	if (strcmp(font, "bold_black") == 0) {
		return EST_DISPLAY_FONT_BOLD_BLACK;
	}
	if (strcmp(font, "large_black") == 0) {
		return EST_DISPLAY_FONT_LARGE_BLACK;
	}
	if (strcmp(font, "regular_white") == 0) {
		return EST_DISPLAY_FONT_REGULAR_WHITE;
	}
	if (strcmp(font, "bold_white") == 0) {
		return EST_DISPLAY_FONT_BOLD_WHITE;
	}
	if (strcmp(font, "large_white") == 0) {
		return EST_DISPLAY_FONT_LARGE_WHITE;
	}
	mp_raise_ValueError(MP_ERROR_TEXT("invalid display font"));
}

static mp_obj_t modest_display_text(size_t argument_count,
	const mp_obj_t *positional_arguments, mp_map_t *keyword_arguments)
{
	enum { ARG_x, ARG_y, ARG_text, ARG_scale, ARG_font };
	static const mp_arg_t allowed_arguments[] = {
		{MP_QSTR_x, MP_ARG_REQUIRED | MP_ARG_OBJ,
			{.u_rom_obj = MP_ROM_NONE}},
		{MP_QSTR_y, MP_ARG_REQUIRED | MP_ARG_OBJ,
			{.u_rom_obj = MP_ROM_NONE}},
		{MP_QSTR_text, MP_ARG_REQUIRED | MP_ARG_OBJ,
			{.u_rom_obj = MP_ROM_NONE}},
		{MP_QSTR_scale, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE}},
		{MP_QSTR_font, MP_ARG_KW_ONLY | MP_ARG_OBJ,
			{.u_rom_obj = MP_ROM_NONE}},
	};
	mp_arg_val_t arguments[MP_ARRAY_SIZE(allowed_arguments)];
	uint16_t x;
	uint16_t y;
	const char *value;

	mp_arg_parse_all(argument_count, positional_arguments,
		keyword_arguments, MP_ARRAY_SIZE(allowed_arguments),
		allowed_arguments, arguments);
	x = (uint16_t)require_integer_range(arguments[ARG_x].u_obj, 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x must be 0..179"));
	y = (uint16_t)require_integer_range(arguments[ARG_y].u_obj, 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y must be 0..127"));
	if (!mp_obj_is_str(arguments[ARG_text].u_obj)) {
		mp_raise_TypeError(MP_ERROR_TEXT("display text must be a string"));
	}
	value = mp_obj_str_get_str(arguments[ARG_text].u_obj);
	if (arguments[ARG_font].u_obj != mp_const_none) {
		est_display_font_t font;

		if (arguments[ARG_scale].u_obj != mp_const_none) {
			mp_raise_ValueError(
				MP_ERROR_TEXT("scale and font cannot be combined"));
		}
		font = modest_display_font_from_object(arguments[ARG_font].u_obj);
		modest_require_peripheral_result(
			est_display_text_font(x, y, value, font));
	} else {
		uint8_t scale = 1U;

		if (arguments[ARG_scale].u_obj != mp_const_none) {
			scale = (uint8_t)require_integer_range(
				arguments[ARG_scale].u_obj, 1, 4,
				MP_ERROR_TEXT("text scale must be 1..4"));
		}
		modest_require_peripheral_result(
			est_display_text(x, y, value, scale));
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(
	modest_display_text_obj, 3, modest_display_text);

static mp_obj_t modest_display_bitmap(size_t argument_count,
	const mp_obj_t *arguments)
{
	uint16_t x = (uint16_t)require_integer_range(arguments[0], 0,
		EST_DISPLAY_WIDTH - 1, MP_ERROR_TEXT("x must be 0..179"));
	uint16_t y = (uint16_t)require_integer_range(arguments[1], 0,
		EST_DISPLAY_HEIGHT - 1, MP_ERROR_TEXT("y must be 0..127"));
	uint16_t width = (uint16_t)require_integer_range(arguments[2], 1,
		EST_DISPLAY_WIDTH, MP_ERROR_TEXT("width must be 1..180"));
	uint16_t height = (uint16_t)require_integer_range(arguments[3], 1,
		EST_DISPLAY_HEIGHT, MP_ERROR_TEXT("height must be 1..128"));
	mp_buffer_info_t buffer;

	(void)argument_count;
	mp_get_buffer_raise(arguments[4], &buffer, MP_BUFFER_READ);
	modest_require_peripheral_result(est_display_bitmap(x, y, width, height,
		(const uint8_t *)buffer.buf, buffer.len));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
	modest_display_bitmap_obj, 5, 5, modest_display_bitmap);

static mp_obj_t modest_display_image(mp_obj_t name_object)
{
	const char *name;

	if (!mp_obj_is_str(name_object)) {
		mp_raise_TypeError(
			MP_ERROR_TEXT("display image name must be a string"));
	}
	name = mp_obj_str_get_str(name_object);
	if (est_display_image(name) != EST_OK) {
		mp_raise_ValueError(MP_ERROR_TEXT("unknown display image"));
	}
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_display_image_obj, modest_display_image);

static mp_obj_t modest_display_refresh(void)
{
	est_micropython_vm_hook();
	est_display_refresh_with_hook(est_micropython_vm_hook);
	est_micropython_vm_hook();
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_display_refresh_obj, modest_display_refresh);

static const mp_rom_map_elem_t modest_display_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_display)},
	{MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&modest_display_clear_obj)},
	{MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&modest_display_pixel_obj)},
	{MP_ROM_QSTR(MP_QSTR_line), MP_ROM_PTR(&modest_display_line_obj)},
	{MP_ROM_QSTR(MP_QSTR_rectangle),
		MP_ROM_PTR(&modest_display_rectangle_obj)},
	{MP_ROM_QSTR(MP_QSTR_text), MP_ROM_PTR(&modest_display_text_obj)},
	{MP_ROM_QSTR(MP_QSTR_bitmap), MP_ROM_PTR(&modest_display_bitmap_obj)},
	{MP_ROM_QSTR(MP_QSTR_image), MP_ROM_PTR(&modest_display_image_obj)},
	{MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&modest_display_refresh_obj)},
	{MP_ROM_QSTR(MP_QSTR_WIDTH), MP_ROM_INT(EST_DISPLAY_WIDTH)},
	{MP_ROM_QSTR(MP_QSTR_HEIGHT), MP_ROM_INT(EST_DISPLAY_HEIGHT)},
};
static MP_DEFINE_CONST_DICT(
	modest_display_globals, modest_display_globals_table);
static const mp_obj_module_t modest_display_module = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_display_globals,
};

static mp_obj_t modest_led_set(mp_obj_t color_object)
{
	int32_t color = require_integer_range(color_object, EST_LED_OFF,
		EST_LED_RED_BLUE, MP_ERROR_TEXT("invalid LED color"));

	modest_require_peripheral_result(est_led_set((est_led_color_t)color));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(modest_led_set_obj, modest_led_set);

static mp_obj_t modest_led_get(void)
{
	return mp_obj_new_int((mp_int_t)est_led_get());
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_led_get_obj, modest_led_get);

static const mp_rom_map_elem_t modest_led_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_led)},
	{MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&modest_led_set_obj)},
	{MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&modest_led_get_obj)},
	{MP_ROM_QSTR(MP_QSTR_OFF), MP_ROM_INT(EST_LED_OFF)},
	{MP_ROM_QSTR(MP_QSTR_RED), MP_ROM_INT(EST_LED_RED)},
	{MP_ROM_QSTR(MP_QSTR_BLUE), MP_ROM_INT(EST_LED_BLUE)},
	{MP_ROM_QSTR(MP_QSTR_RED_BLUE), MP_ROM_INT(EST_LED_RED_BLUE)},
};
static MP_DEFINE_CONST_DICT(modest_led_globals, modest_led_globals_table);
static const mp_obj_module_t modest_led_module = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_led_globals,
};

static mp_obj_t modest_backlight_set(mp_obj_t percent_object)
{
	int32_t percent = require_integer_range(percent_object, 0, 100,
		MP_ERROR_TEXT("backlight must be 0..100"));

	modest_require_peripheral_result(
		est_backlight_set_percent((uint8_t)percent));
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_backlight_set_obj, modest_backlight_set);

static mp_obj_t modest_backlight_get(void)
{
	return mp_obj_new_int_from_uint(est_backlight_get_percent());
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_backlight_get_obj, modest_backlight_get);

static const mp_rom_map_elem_t modest_backlight_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_backlight)},
	{MP_ROM_QSTR(MP_QSTR_set), MP_ROM_PTR(&modest_backlight_set_obj)},
	{MP_ROM_QSTR(MP_QSTR_get), MP_ROM_PTR(&modest_backlight_get_obj)},
	{MP_ROM_QSTR(MP_QSTR_MIN), MP_ROM_INT(0)},
	{MP_ROM_QSTR(MP_QSTR_MAX), MP_ROM_INT(100)},
};
static MP_DEFINE_CONST_DICT(
	modest_backlight_globals, modest_backlight_globals_table);
static const mp_obj_module_t modest_backlight_module = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_backlight_globals,
};

static mp_obj_t modest_buttons_value(void)
{
	return mp_obj_new_int_from_uint(est_buttons_pressed_mask());
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_buttons_value_obj, modest_buttons_value);

static mp_obj_t modest_buttons_pressed(mp_obj_t button_object)
{
	int32_t button_mask = require_integer_range(button_object, 1,
		EST_BUTTON_MASK_ALL, MP_ERROR_TEXT("invalid button"));
	uint8_t button = 0U;

	if (((uint32_t)button_mask & ((uint32_t)button_mask - 1U)) != 0U) {
		mp_raise_ValueError(MP_ERROR_TEXT("button must be one key"));
	}
	while (((uint32_t)button_mask & (1UL << button)) == 0U) {
		button++;
	}
	return mp_obj_new_bool(est_button_is_pressed((est_button_t)button));
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_buttons_pressed_obj, modest_buttons_pressed);

static const mp_rom_map_elem_t modest_buttons_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_buttons)},
	{MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&modest_buttons_value_obj)},
	{MP_ROM_QSTR(MP_QSTR_pressed), MP_ROM_PTR(&modest_buttons_pressed_obj)},
	{MP_ROM_QSTR(MP_QSTR_NONE), MP_ROM_INT(0)},
	{MP_ROM_QSTR(MP_QSTR_BACK), MP_ROM_INT(1U << EST_BUTTON_BACK)},
	{MP_ROM_QSTR(MP_QSTR_LEFT), MP_ROM_INT(1U << EST_BUTTON_LEFT)},
	{MP_ROM_QSTR(MP_QSTR_UP), MP_ROM_INT(1U << EST_BUTTON_UP)},
	{MP_ROM_QSTR(MP_QSTR_DOWN), MP_ROM_INT(1U << EST_BUTTON_DOWN)},
	{MP_ROM_QSTR(MP_QSTR_RIGHT), MP_ROM_INT(1U << EST_BUTTON_RIGHT)},
	{MP_ROM_QSTR(MP_QSTR_CONFIRM), MP_ROM_INT(1U << EST_BUTTON_CONFIRM)},
	{MP_ROM_QSTR(MP_QSTR_CENTER), MP_ROM_INT(1U << EST_BUTTON_CENTER)},
};
static MP_DEFINE_CONST_DICT(
	modest_buttons_globals, modest_buttons_globals_table);
static const mp_obj_module_t modest_buttons_module = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_buttons_globals,
};

static est_result_t modest_battery_read(est_battery_status_t *status)
{
	return est_battery_get_status(status);
}

static void modest_battery_require_ready(est_result_t result)
{
	if (result != EST_OK) {
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("battery not ready"));
	}
}

static mp_obj_t modest_battery_valid(void)
{
	est_battery_status_t status = {0};

	(void)modest_battery_read(&status);
	return mp_obj_new_bool(status.valid);
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_battery_valid_obj, modest_battery_valid);

static mp_obj_t modest_battery_level(void)
{
	est_battery_status_t status = {0};
	est_result_t result = modest_battery_read(&status);

	modest_battery_require_ready(result);
	return mp_obj_new_int_from_uint(status.level);
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_battery_level_obj, modest_battery_level);

static mp_obj_t modest_battery_percent(void)
{
	est_battery_status_t status = {0};
	est_result_t result = modest_battery_read(&status);

	modest_battery_require_ready(result);
	return mp_obj_new_int_from_uint(status.percent);
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_battery_percent_obj, modest_battery_percent);

static mp_obj_t modest_battery_low(void)
{
	est_battery_status_t status = {0};
	est_result_t result = modest_battery_read(&status);

	modest_battery_require_ready(result);
	return mp_obj_new_bool(status.low);
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_battery_low_obj, modest_battery_low);

static mp_obj_t modest_battery_status(void)
{
	est_battery_status_t status = {0};
	est_result_t result = modest_battery_read(&status);
	mp_obj_t values[7];

	values[0] = mp_obj_new_int((mp_int_t)result);
	values[1] = mp_obj_new_bool(status.valid);
	values[2] = mp_obj_new_int_from_uint(status.level);
	values[3] = mp_obj_new_int_from_uint(status.percent);
	values[4] = mp_obj_new_bool(status.low);
	values[5] = mp_obj_new_int_from_uint(status.adc_raw);
	values[6] = mp_obj_new_int_from_uint(status.sample_mv);
	return mp_obj_new_tuple(7U, values);
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_battery_status_obj, modest_battery_status);

static const mp_rom_map_elem_t modest_battery_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_battery)},
	{MP_ROM_QSTR(MP_QSTR_valid), MP_ROM_PTR(&modest_battery_valid_obj)},
	{MP_ROM_QSTR(MP_QSTR_level), MP_ROM_PTR(&modest_battery_level_obj)},
	{MP_ROM_QSTR(MP_QSTR_percent), MP_ROM_PTR(&modest_battery_percent_obj)},
	{MP_ROM_QSTR(MP_QSTR_low), MP_ROM_PTR(&modest_battery_low_obj)},
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_battery_status_obj)},
	{MP_ROM_QSTR(MP_QSTR_LEVEL_MAX), MP_ROM_INT(EST_BATTERY_LEVEL_MAX)},
};
static MP_DEFINE_CONST_DICT(
	modest_battery_globals, modest_battery_globals_table);
static const mp_obj_module_t modest_battery_module = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_battery_globals,
};

static mp_obj_t modest_force_gc(void)
{
	est_micropython_status_t before = {0};
	est_micropython_status_t after = {0};

	(void)est_micropython_get_status(&before);
	gc_collect();
	(void)est_micropython_get_status(&after);
	return mp_obj_new_int_from_uint(
		after.maximum_gc_pause_us >= before.maximum_gc_pause_us ?
		after.maximum_gc_pause_us : before.maximum_gc_pause_us);
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_force_gc_obj, modest_force_gc);

static mp_obj_t modest_runtime_ticks(void)
{
	est_runtime_status_t status = {0};

	(void)est_runtime_get_status(&status);
	return mp_obj_new_int_from_uint(status.tick_count);
}
static MP_DEFINE_CONST_FUN_OBJ_0(modest_runtime_ticks_obj, modest_runtime_ticks);

static mp_obj_t modest_self_test_complete(mp_obj_t value_object)
{
	int32_t value = require_integer_range(
		value_object, 0, UINT16_MAX,
		MP_ERROR_TEXT("self test value out of range"));

	est_micropython_mark_self_test((uint16_t)value);
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_self_test_complete_obj, modest_self_test_complete);

static mp_obj_t modest_program_result(mp_obj_t value_object)
{
	int32_t value = (int32_t)mp_obj_get_int(value_object);

	est_micropython_program_report(value);
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
	modest_program_result_obj, modest_program_result);

static mp_obj_t modest_stop_user_program(void)
{
	est_micropython_program_stop_from_vm();
	return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(
	modest_stop_user_program_obj, modest_stop_user_program);

static const mp_rom_map_elem_t modest_module_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_est)},
	{MP_ROM_QSTR(MP_QSTR_millis), MP_ROM_PTR(&modest_millis_obj)},
	{MP_ROM_QSTR(MP_QSTR_motor_type), MP_ROM_PTR(&modest_motor_type_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop_all), MP_ROM_PTR(&modest_stop_all_obj)},
	{MP_ROM_QSTR(MP_QSTR_Motor), MP_ROM_PTR(&modest_motor_class)},
	{MP_ROM_QSTR(MP_QSTR_MotorPair),
		MP_ROM_PTR(&modest_motor_pair_class)},
	{MP_ROM_QSTR(MP_QSTR_DriveBase),
		MP_ROM_PTR(&modest_drive_base_class)},
	{MP_ROM_QSTR(MP_QSTR_drive_mix), MP_ROM_PTR(&modest_drive_mix_obj)},
	{MP_ROM_QSTR(MP_QSTR_sensor), MP_ROM_PTR(&modest_sensor_obj)},
	{MP_ROM_QSTR(MP_QSTR_Sensor), MP_ROM_PTR(&modest_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_SoundSensor),
		MP_ROM_PTR(&modest_sound_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_TemperatureSensor),
		MP_ROM_PTR(&modest_temperature_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_TouchSensor),
		MP_ROM_PTR(&modest_touch_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_ColorSensor),
		MP_ROM_PTR(&modest_color_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_UltrasonicSensor),
		MP_ROM_PTR(&modest_ultrasonic_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_GyroSensor),
		MP_ROM_PTR(&modest_gyro_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_InfraredSensor),
		MP_ROM_PTR(&modest_infrared_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_display), MP_ROM_PTR(&modest_display_module)},
	{MP_ROM_QSTR(MP_QSTR_led), MP_ROM_PTR(&modest_led_module)},
	{MP_ROM_QSTR(MP_QSTR_backlight),
		MP_ROM_PTR(&modest_backlight_module)},
	{MP_ROM_QSTR(MP_QSTR_buttons), MP_ROM_PTR(&modest_buttons_module)},
	{MP_ROM_QSTR(MP_QSTR_battery), MP_ROM_PTR(&modest_battery_module)},
	{MP_ROM_QSTR(MP_QSTR_force_gc), MP_ROM_PTR(&modest_force_gc_obj)},
	{MP_ROM_QSTR(MP_QSTR__runtime_ticks),
		MP_ROM_PTR(&modest_runtime_ticks_obj)},
	{MP_ROM_QSTR(MP_QSTR__self_test_complete),
		MP_ROM_PTR(&modest_self_test_complete_obj)},
	{MP_ROM_QSTR(MP_QSTR__program_result),
		MP_ROM_PTR(&modest_program_result_obj)},
	{MP_ROM_QSTR(MP_QSTR__stop_user_program),
		MP_ROM_PTR(&modest_stop_user_program_obj)},
};
static MP_DEFINE_CONST_DICT(modest_module_globals, modest_module_globals_table);

const mp_obj_module_t mp_module_est = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_est, mp_module_est);
