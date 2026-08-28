#include <stdint.h>

#include "py/gc.h"
#include "py/obj.h"
#include "py/runtime.h"

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
