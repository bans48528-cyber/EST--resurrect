#include <stdint.h>

#include "py/gc.h"
#include "py/obj.h"
#include "py/runtime.h"

#include "est_battery.h"
#include "est_buttons.h"
#include "est_drive.h"
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
} modest_sensor_instance_t;

static void modest_raise_sensor_error(est_result_t error)
{
	switch (error) {
	case EST_ERR_NOT_CONNECTED:
		mp_raise_msg(&mp_type_RuntimeError,
			MP_ERROR_TEXT("sensor not connected"));
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

static void modest_sensor_read(mp_obj_t self_object,
	est_sensor_status_t *status)
{
	modest_sensor_instance_t *self = MP_OBJ_TO_PTR(self_object);
	est_result_t result = est_sensor_get_status(self->port, status);

	if (result != EST_OK) {
		modest_raise_sensor_error(result);
	}
}

static mp_obj_t modest_sensor_make_new(const mp_obj_type_t *type,
	size_t argument_count, size_t keyword_count, const mp_obj_t *arguments)
{
	int32_t port;
	modest_sensor_instance_t *self;

	mp_arg_check_num(argument_count, keyword_count, 1U, 1U, false);
	port = require_integer_range(arguments[0], 1, EST_SENSOR_PORT_COUNT,
		MP_ERROR_TEXT("sensor port must be 1..4"));
	self = mp_obj_malloc(modest_sensor_instance_t, type);
	self->port = (est_sensor_port_t)(port - 1);
	return MP_OBJ_FROM_PTR(self);
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

static const mp_rom_map_elem_t modest_sensor_locals_table[] = {
	{MP_ROM_QSTR(MP_QSTR_port), MP_ROM_PTR(&modest_sensor_port_obj)},
	{MP_ROM_QSTR(MP_QSTR_type), MP_ROM_PTR(&modest_sensor_type_value_obj)},
	{MP_ROM_QSTR(MP_QSTR_state), MP_ROM_PTR(&modest_sensor_state_obj)},
	{MP_ROM_QSTR(MP_QSTR_mode), MP_ROM_PTR(&modest_sensor_mode_obj)},
	{MP_ROM_QSTR(MP_QSTR_value_format),
		MP_ROM_PTR(&modest_sensor_value_format_obj)},
	{MP_ROM_QSTR(MP_QSTR_valid), MP_ROM_PTR(&modest_sensor_valid_obj)},
	{MP_ROM_QSTR(MP_QSTR_error), MP_ROM_PTR(&modest_sensor_error_obj)},
	{MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&modest_sensor_value_obj)},
	{MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&modest_sensor_status_obj)},
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
};
static MP_DEFINE_CONST_DICT(
	modest_sensor_locals, modest_sensor_locals_table);

static MP_DEFINE_CONST_OBJ_TYPE(
	modest_sensor_type,
	MP_QSTR_Sensor,
	MP_TYPE_FLAG_NONE,
	make_new, modest_sensor_make_new,
	locals_dict, &modest_sensor_locals);

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

static const mp_rom_map_elem_t modest_module_globals_table[] = {
	{MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_est)},
	{MP_ROM_QSTR(MP_QSTR_millis), MP_ROM_PTR(&modest_millis_obj)},
	{MP_ROM_QSTR(MP_QSTR_motor_type), MP_ROM_PTR(&modest_motor_type_obj)},
	{MP_ROM_QSTR(MP_QSTR_stop_all), MP_ROM_PTR(&modest_stop_all_obj)},
	{MP_ROM_QSTR(MP_QSTR_drive_mix), MP_ROM_PTR(&modest_drive_mix_obj)},
	{MP_ROM_QSTR(MP_QSTR_sensor), MP_ROM_PTR(&modest_sensor_obj)},
	{MP_ROM_QSTR(MP_QSTR_Sensor), MP_ROM_PTR(&modest_sensor_type)},
	{MP_ROM_QSTR(MP_QSTR_buttons), MP_ROM_PTR(&modest_buttons_module)},
	{MP_ROM_QSTR(MP_QSTR_battery), MP_ROM_PTR(&modest_battery_module)},
	{MP_ROM_QSTR(MP_QSTR_force_gc), MP_ROM_PTR(&modest_force_gc_obj)},
	{MP_ROM_QSTR(MP_QSTR__runtime_ticks),
		MP_ROM_PTR(&modest_runtime_ticks_obj)},
	{MP_ROM_QSTR(MP_QSTR__self_test_complete),
		MP_ROM_PTR(&modest_self_test_complete_obj)},
	{MP_ROM_QSTR(MP_QSTR__program_result),
		MP_ROM_PTR(&modest_program_result_obj)},
};
static MP_DEFINE_CONST_DICT(modest_module_globals, modest_module_globals_table);

const mp_obj_module_t mp_module_est = {
	.base = {&mp_type_module},
	.globals = (mp_obj_dict_t *)&modest_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_est, mp_module_est);
