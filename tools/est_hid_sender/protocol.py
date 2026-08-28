from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    DEVICE_DIRECTION,
    DEVICE_STATUS_COMMAND,
    DRIVE_STRAIGHT_ACTION_START,
    DRIVE_STRAIGHT_COMMAND,
    DRIVE_RUN_ACTION_START,
    DRIVE_RUN_COMMAND,
    DRIVE_RUN_MODE_DEGREES,
    DRIVE_RUN_MODE_TIME_MS,
    DRIVE_STEER_ACTION_START,
    DRIVE_STEER_COMMAND,
    DRIVE_STEER_FOR_ACTION_START,
    DRIVE_STEER_FOR_COMMAND,
    DRIVE_STEER_FOR_MODE_DEGREES,
    DRIVE_STEER_FOR_MODE_TIME_MS,
    FLASH_ID_COMMAND,
    FLASH_SCAN_COMMAND,
    FLASH_TEST_COMMAND,
    FLASH_STATUS_COMMAND,
    FLASH_MODE_PROBE_COMMAND,
    FRAME_END,
    FRAME_START,
    HEARTBEAT_COMMAND,
    HOST_DIRECTION,
    INPUT_SENSOR_ACTION_SET_MODE,
    INPUT_SENSOR_COMMAND,
    KEY_STATUS_COMMAND,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
    MICROPYTHON_STATUS_COMMAND,
    PERSISTENT_PROGRAM_ACTION_STATUS,
    PERSISTENT_PROGRAM_ACTION_SAVE,
    PERSISTENT_PROGRAM_ACTION_LOAD,
    PERSISTENT_PROGRAM_ACTION_CLEAR,
    PERSISTENT_PROGRAM_COMMAND,
    PERSISTENT_PROGRAM_NAME_MAX_BYTES,
    PERSISTENT_PROGRAM_SLOT_COUNT,
    PYTHON_PROGRAM_ACTION_BEGIN,
    PYTHON_PROGRAM_ACTION_CHUNK,
    PYTHON_PROGRAM_ACTION_CLEAR,
    PYTHON_PROGRAM_ACTION_RUN,
    PYTHON_PROGRAM_ACTION_STATUS,
    PYTHON_PROGRAM_ACTION_STOP,
    PYTHON_PROGRAM_COMMAND,
    PYTHON_PROGRAM_MAX_SIZE,
    PYTHON_PROGRAM_MAX_TIMEOUT_MS,
    PYTHON_PROGRAM_MIN_TIMEOUT_MS,
    MOTOR_CONTROL_ACTION_SET_POWER,
    MOTOR_CONTROL_COMMAND,
    MOTOR_DUAL_TEST_COMMAND,
    MOTOR_PAIR_POSITION_ACTION_START,
    MOTOR_PAIR_POSITION_COMMAND,
    MOTOR_PAIR_SPEED_ACTION_START,
    MOTOR_PAIR_SPEED_COMMAND,
    MOTOR_POSITION_ACTION_START,
    MOTOR_POSITION_COMMAND,
    MOTOR_SPEED_ACTION_START,
    MOTOR_SPEED_COMMAND,
    MOTOR_STOP_TEST_COMMAND,
    MOTOR_TACHO_TEST_COMMAND,
    MOTOR_TYPE_COMMAND,
    MOTOR_TEST_ACTION_START,
    MOTOR_TEST_COMMAND,
    UPDATE_COMMAND,
)


@dataclass(frozen=True)
class UpdateAck:
    total_frames: int
    frame_index: int
    flag: int


@dataclass(frozen=True)
class FlashScanResult:
    supported: bool
    erased: bool
    address: int


@dataclass(frozen=True)
class FlashTestResult:
    status: int
    restored: bool
    address: int


@dataclass(frozen=True)
class FlashStatus:
    status1: int
    status2: int
    status3: int


@dataclass(frozen=True)
class FlashModeProbe:
    status1_before: int
    status1_write_enabled: int
    status1_write_disabled: int
    status3_before: int
    status3_four_byte: int
    status3_restored: int


@dataclass(frozen=True)
class MotorTestResult:
    result: int
    state: int


@dataclass(frozen=True)
class MotorTachoTestResult:
    result: int
    state: int
    total_count: int
    forward_count: int
    reverse_count: int


@dataclass(frozen=True)
class MotorStopTestResult:
    result: int
    state: int
    mode: int
    total_count: int
    powered_count: int
    stopped_count: int


@dataclass(frozen=True)
class MotorDualTestResult:
    result: int
    state: int
    a_forward_count: int
    b_forward_count: int
    a_reverse_count: int
    b_reverse_count: int


@dataclass(frozen=True)
class MotorControlResult:
    result: int
    port: int
    output_state: int
    power_percent: int
    tacho_count: int


@dataclass(frozen=True)
class MotorPositionResult:
    result: int
    port: int
    state: int
    motor_type: int
    requested_speed_percent: int
    measured_speed_percent: int
    start_count: int
    target_count: int
    current_count: int
    error_count: int


@dataclass(frozen=True)
class MotorSpeedResult:
    result: int
    port: int
    state: int
    output_state: int
    motor_type: int
    requested_speed_percent: int
    measured_speed_percent: int
    power_percent: int
    tacho_count: int


@dataclass(frozen=True)
class MotorPairPositionResult:
    result: int
    state: int
    left_port: int
    right_port: int
    left_target_degrees: int
    right_target_degrees: int
    left_actual_degrees: int
    right_actual_degrees: int
    synchronization_error_degrees: int
    maximum_synchronization_error_degrees: int
    error: int


@dataclass(frozen=True)
class MotorPairSpeedResult:
    result: int
    state: int
    left_port: int
    right_port: int
    left_requested_speed_percent: int
    right_requested_speed_percent: int
    left_measured_speed_percent: int
    right_measured_speed_percent: int
    left_power_percent: int
    right_power_percent: int
    left_actual_degrees: int
    right_actual_degrees: int
    synchronization_error_degrees: int
    maximum_synchronization_error_degrees: int
    error: int


@dataclass(frozen=True)
class DriveStraightResult:
    result: int
    state: int
    left_port: int
    right_port: int
    target_distance_mm: int
    actual_distance_mm: int
    left_target_degrees: int
    right_target_degrees: int
    left_actual_degrees: int
    right_actual_degrees: int
    synchronization_error_degrees: int
    maximum_synchronization_error_degrees: int
    error: int


@dataclass(frozen=True)
class DriveRunResult:
    result: int
    state: int
    mode: int
    left_port: int
    right_port: int
    requested_speed_percent: int
    target_value: int
    actual_value: int
    left_actual_degrees: int
    right_actual_degrees: int
    synchronization_error_degrees: int
    maximum_synchronization_error_degrees: int
    error: int


@dataclass(frozen=True)
class DriveSteerForResult:
    result: int
    state: int
    mode: int
    left_port: int
    right_port: int
    steering: int
    requested_speed_percent: int
    left_requested_speed_percent: int
    right_requested_speed_percent: int
    target_value: int
    actual_value: int
    left_target_degrees: int
    right_target_degrees: int
    left_actual_degrees: int
    right_actual_degrees: int
    synchronization_error_degrees: int
    maximum_synchronization_error_degrees: int
    error: int


@dataclass(frozen=True)
class MotorIdentification:
    motor_type: int
    adc_raw: int
    millivolts: int
    pin6_low_adc_raw: int = 0
    pin6_low_millivolts: int = 0
    pin5_pullup_adc_raw: int = 0
    pin5_pullup_millivolts: int = 0
    pin5_pullup_high: bool = False


@dataclass(frozen=True)
class MotorTypeResult:
    result: int
    motors: tuple[MotorIdentification, ...]


@dataclass(frozen=True)
class InputSensorResult:
    result: int
    port: int
    state: int
    sensor_type: int
    mode: int
    value_valid: bool
    value: int
    adc0_raw: int
    adc1_raw: int
    digital_mask: int
    rx_count: int
    checksum_errors: int


@dataclass(frozen=True)
class DeviceMotorStatus:
    output_state: int
    power_percent: int
    tacho_count: int


@dataclass(frozen=True)
class DeviceSensorStatus:
    state: int
    sensor_type: int
    mode: int
    value_valid: bool
    value: int


@dataclass(frozen=True)
class DeviceStatus:
    protocol_major: int
    protocol_minor: int
    firmware_version: str
    motor_port_count: int
    sensor_port_count: int
    key_mask: int
    battery_level: int
    battery_adc_raw: int
    battery_sample_mv: int
    capabilities: int
    uptime_ms: int
    motors: tuple[DeviceMotorStatus, ...]
    sensors: tuple[DeviceSensorStatus, ...]


@dataclass(frozen=True)
class MicroPythonStatus:
    schema_version: int
    state: int
    flags: int
    heap_total_bytes: int
    heap_used_bytes: int
    heap_free_bytes: int
    startup_duration_ms: int
    maximum_gc_pause_us: int
    gc_count: int
    self_test_value: int


@dataclass(frozen=True)
class PythonProgramStatus:
    schema_version: int
    result: int
    state: int
    error: int
    flags: int
    expected_length: int
    received_length: int
    run_count: int
    expected_crc32: int
    actual_crc32: int
    duration_ms: int
    timeout_ms: int
    result_value: int


@dataclass(frozen=True)
class PersistentProgramStatus:
    schema_version: int
    result: int
    state: int
    flags: int
    erased_sector_mask: int
    occupied_sector_mask: int
    slot_count: int
    sectors_per_slot: int
    region_start: int
    region_size: int
    slot_size: int
    flash_size: int
    jedec_id: bytes
    active_slot: int
    generation: int
    source_length: int
    record_type: int
    last_error: int
    source_crc32: int
    program_slot_id: int
    program_slot_count: int
    program_name: str


def checksum(data: bytes | bytearray) -> int:
    return sum(data) & 0xFF


def build_frame(command: int, payload: bytes = b"") -> bytes:
    frame = bytearray((FRAME_START, HOST_DIRECTION, command))
    frame += len(payload).to_bytes(2, "little")
    frame += payload
    frame.append(checksum(frame))
    frame.append(FRAME_END)
    return bytes(frame)


def build_motor_type_frame(
    action: int | None = None, motor_port: int | None = None
) -> bytes:
    if action is None and motor_port is None:
        return build_frame(MOTOR_TYPE_COMMAND)
    if action is None or not 0 <= action <= 0xFF:
        raise ValueError("motor type action must fit uint8")
    if motor_port not in (0, 1, 2, 3):
        raise ValueError("motor port must be 0, 1, 2, or 3")
    return build_frame(MOTOR_TYPE_COMMAND, bytes((action, motor_port)))


def build_heartbeat_frame() -> bytes:
    return build_frame(HEARTBEAT_COMMAND)


def build_key_status_frame() -> bytes:
    return build_frame(KEY_STATUS_COMMAND)


def build_device_status_frame() -> bytes:
    return build_frame(DEVICE_STATUS_COMMAND)


def build_micropython_status_frame() -> bytes:
    return build_frame(MICROPYTHON_STATUS_COMMAND)


def build_python_program_status_frame() -> bytes:
    return build_frame(PYTHON_PROGRAM_COMMAND, bytes((PYTHON_PROGRAM_ACTION_STATUS,)))


def build_python_program_begin_frame(length: int, crc32: int) -> bytes:
    if not 1 <= length <= PYTHON_PROGRAM_MAX_SIZE:
        raise ValueError("Python program length must be 1..8192 bytes")
    if not 0 <= crc32 <= 0xFFFFFFFF:
        raise ValueError("Python program CRC32 must fit uint32")
    payload = bytes((PYTHON_PROGRAM_ACTION_BEGIN,))
    payload += length.to_bytes(2, "little")
    payload += crc32.to_bytes(4, "little")
    return build_frame(PYTHON_PROGRAM_COMMAND, payload)


def build_python_program_chunk_frame(offset: int, chunk: bytes) -> bytes:
    if not 0 <= offset < PYTHON_PROGRAM_MAX_SIZE:
        raise ValueError("Python program offset must be 0..8191")
    if not chunk or len(chunk) > 1000:
        raise ValueError("Python program chunk must be 1..1000 bytes")
    if offset + len(chunk) > PYTHON_PROGRAM_MAX_SIZE:
        raise ValueError("Python program chunk exceeds 8192 bytes")
    payload = bytes((PYTHON_PROGRAM_ACTION_CHUNK,))
    payload += offset.to_bytes(2, "little") + chunk
    return build_frame(PYTHON_PROGRAM_COMMAND, payload)


def build_python_program_run_frame(timeout_ms: int) -> bytes:
    if not PYTHON_PROGRAM_MIN_TIMEOUT_MS <= timeout_ms <= PYTHON_PROGRAM_MAX_TIMEOUT_MS:
        raise ValueError("Python program timeout must be 100..10000 ms")
    payload = bytes((PYTHON_PROGRAM_ACTION_RUN,)) + timeout_ms.to_bytes(4, "little")
    return build_frame(PYTHON_PROGRAM_COMMAND, payload)


def build_python_program_stop_frame() -> bytes:
    return build_frame(PYTHON_PROGRAM_COMMAND, bytes((PYTHON_PROGRAM_ACTION_STOP,)))


def build_python_program_clear_frame() -> bytes:
    return build_frame(PYTHON_PROGRAM_COMMAND, bytes((PYTHON_PROGRAM_ACTION_CLEAR,)))


def _validate_persistent_program_slot(program_slot_id: int) -> None:
    if not 0 <= program_slot_id < PERSISTENT_PROGRAM_SLOT_COUNT:
        raise ValueError("Persistent program slot must be 0..7")


def build_persistent_program_status_frame(program_slot_id: int = 0) -> bytes:
    _validate_persistent_program_slot(program_slot_id)
    payload = bytes((PERSISTENT_PROGRAM_ACTION_STATUS,))
    if program_slot_id != 0:
        payload += bytes((program_slot_id,))
    return build_frame(PERSISTENT_PROGRAM_COMMAND, payload)


def build_persistent_program_save_frame(
    program_slot_id: int = 0, program_name: str | None = None
) -> bytes:
    _validate_persistent_program_slot(program_slot_id)
    if program_name is None:
        if program_slot_id != 0:
            raise ValueError("A program name is required outside slot 0")
        return build_frame(
            PERSISTENT_PROGRAM_COMMAND, bytes((PERSISTENT_PROGRAM_ACTION_SAVE,))
        )
    encoded_name = program_name.encode("utf-8")
    if not encoded_name or len(encoded_name) > PERSISTENT_PROGRAM_NAME_MAX_BYTES:
        raise ValueError("Persistent program name must be 1..31 UTF-8 bytes")
    if b"\x00" in encoded_name:
        raise ValueError("Persistent program name must not contain NUL bytes")
    payload = bytes(
        (PERSISTENT_PROGRAM_ACTION_SAVE, program_slot_id, len(encoded_name))
    ) + encoded_name
    return build_frame(PERSISTENT_PROGRAM_COMMAND, payload)


def build_persistent_program_load_frame(program_slot_id: int = 0) -> bytes:
    _validate_persistent_program_slot(program_slot_id)
    payload = bytes((PERSISTENT_PROGRAM_ACTION_LOAD,))
    if program_slot_id != 0:
        payload += bytes((program_slot_id,))
    return build_frame(PERSISTENT_PROGRAM_COMMAND, payload)


def build_persistent_program_clear_frame(program_slot_id: int = 0) -> bytes:
    _validate_persistent_program_slot(program_slot_id)
    payload = bytes((PERSISTENT_PROGRAM_ACTION_CLEAR,))
    if program_slot_id != 0:
        payload += bytes((program_slot_id,))
    return build_frame(PERSISTENT_PROGRAM_COMMAND, payload)


def build_flash_id_frame() -> bytes:
    return build_frame(FLASH_ID_COMMAND)


def build_flash_scan_frame() -> bytes:
    return build_frame(FLASH_SCAN_COMMAND)


def build_flash_test_frame() -> bytes:
    return build_frame(FLASH_TEST_COMMAND)


def build_flash_status_frame() -> bytes:
    return build_frame(FLASH_STATUS_COMMAND)


def build_flash_mode_probe_frame() -> bytes:
    return build_frame(FLASH_MODE_PROBE_COMMAND)


def build_motor_test_frame(action: int) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor test action must fit uint8")
    return build_frame(MOTOR_TEST_COMMAND, bytes((action,)))


def build_motor_tacho_test_frame(
    action: int,
    power_percent: int | None = None,
    motor_port: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor tacho test action must fit uint8")
    payload = bytes((action,))
    if power_percent is not None:
        if action != MOTOR_TEST_ACTION_START:
            raise ValueError("custom motor power is valid only for the start action")
        if not 1 <= power_percent <= 100:
            raise ValueError("motor power must be between 1 and 100 percent")
        payload += bytes((power_percent,))
        if motor_port is not None:
            if motor_port not in (0, 1, 2, 3):
                raise ValueError("motor port must be 0, 1, 2, or 3")
            payload += bytes((motor_port,))
    elif motor_port is not None:
        raise ValueError("motor port requires a custom power start action")
    return build_frame(MOTOR_TACHO_TEST_COMMAND, payload)


def build_motor_stop_test_frame(
    action: int,
    stop_mode: int | None = None,
    power_percent: int | None = None,
    motor_port: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor stop test action must fit uint8")
    payload = bytes((action,))
    if action == MOTOR_TEST_ACTION_START:
        if stop_mode not in (0, 1):
            raise ValueError("motor stop mode must be 0 or 1")
        if power_percent is None or not 1 <= power_percent <= 100:
            raise ValueError("motor power must be between 1 and 100 percent")
        payload += bytes((stop_mode, power_percent))
        if motor_port is not None:
            if motor_port not in (0, 1, 2, 3):
                raise ValueError("motor port must be 0, 1, 2, or 3")
            payload += bytes((motor_port,))
    elif stop_mode is not None or power_percent is not None or motor_port is not None:
        raise ValueError("stop mode and power are valid only for the start action")
    return build_frame(MOTOR_STOP_TEST_COMMAND, payload)


def build_motor_dual_test_frame(
    action: int, power_percent: int | None = None
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor dual test action must fit uint8")
    payload = bytes((action,))
    if action == MOTOR_TEST_ACTION_START:
        if power_percent is None or not 1 <= power_percent <= 100:
            raise ValueError("motor power must be between 1 and 100 percent")
        payload += bytes((power_percent,))
    elif power_percent is not None:
        raise ValueError("motor power is valid only for the start action")
    return build_frame(MOTOR_DUAL_TEST_COMMAND, payload)


def build_motor_control_frame(
    action: int,
    motor_port: int,
    power_percent: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor control action must fit uint8")
    if motor_port not in (0, 1, 2, 3):
        raise ValueError("motor port must be 0, 1, 2, or 3")
    payload = bytes((action, motor_port))
    if action == MOTOR_CONTROL_ACTION_SET_POWER:
        if power_percent is None or not -100 <= power_percent <= 100:
            raise ValueError("motor power must be between -100 and 100 percent")
        payload += bytes((power_percent & 0xFF,))
    elif power_percent is not None:
        raise ValueError("motor power is valid only for the set-power action")
    return build_frame(MOTOR_CONTROL_COMMAND, payload)


def build_motor_position_frame(
    action: int,
    motor_port: int,
    speed_percent: int | None = None,
    degrees: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor position action must fit uint8")
    if motor_port not in (0, 1, 2, 3):
        raise ValueError("motor port must be 0, 1, 2, or 3")
    payload = bytes((action, motor_port))
    if action == MOTOR_POSITION_ACTION_START:
        if speed_percent is None or not 10 <= speed_percent <= 100:
            raise ValueError("motor speed must be between 10 and 100 percent")
        if degrees is None or degrees == 0 or not -3600 <= degrees <= 3600:
            raise ValueError("motor degrees must be between -3600 and 3600, excluding 0")
        payload += bytes((speed_percent,))
        payload += degrees.to_bytes(4, "little", signed=True)
    elif speed_percent is not None or degrees is not None:
        raise ValueError("speed and degrees are valid only for the start action")
    return build_frame(MOTOR_POSITION_COMMAND, payload)


def build_motor_speed_frame(
    action: int,
    motor_port: int,
    speed_percent: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor speed action must fit uint8")
    if motor_port not in (0, 1, 2, 3):
        raise ValueError("motor port must be 0, 1, 2, or 3")
    payload = bytes((action, motor_port))
    if action == MOTOR_SPEED_ACTION_START:
        if (
            speed_percent is None
            or speed_percent == 0
            or not -100 <= speed_percent <= 100
            or abs(speed_percent) < 10
        ):
            raise ValueError("motor speed must be between -100 and 100 percent with magnitude at least 10")
        payload += bytes((speed_percent & 0xFF,))
    elif speed_percent is not None:
        raise ValueError("speed is valid only for the start action")
    return build_frame(MOTOR_SPEED_COMMAND, payload)


def build_motor_pair_position_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    speed_percent: int | None = None,
    left_degrees: int | None = None,
    right_degrees: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor pair position action must fit uint8")
    payload = bytes((action,))
    if action == MOTOR_PAIR_POSITION_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("motor pair ports must differ")
        if speed_percent is None or not 10 <= speed_percent <= 100:
            raise ValueError("motor speed must be between 10 and 100 percent")
        if (
            left_degrees is None
            or right_degrees is None
            or left_degrees == 0
            or right_degrees == 0
            or not -3600 <= left_degrees <= 3600
            or not -3600 <= right_degrees <= 3600
        ):
            raise ValueError("motor pair degrees must be between -3600 and 3600, excluding 0")
        payload += bytes((left_port, right_port, speed_percent))
        payload += left_degrees.to_bytes(4, "little", signed=True)
        payload += right_degrees.to_bytes(4, "little", signed=True)
    elif any(
        value is not None
        for value in (left_port, right_port, speed_percent, left_degrees, right_degrees)
    ):
        raise ValueError("pair parameters are valid only for the start action")
    return build_frame(MOTOR_PAIR_POSITION_COMMAND, payload)


def build_motor_pair_speed_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    left_speed_percent: int | None = None,
    right_speed_percent: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("motor pair speed action must fit uint8")
    payload = bytes((action,))
    if action == MOTOR_PAIR_SPEED_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("motor pair ports must differ")
        if left_speed_percent is None or right_speed_percent is None:
            raise ValueError("both motor speeds are required")
        if (
            not -100 <= left_speed_percent <= 100
            or not -100 <= right_speed_percent <= 100
            or abs(left_speed_percent) < 10
            or abs(right_speed_percent) < 10
        ):
            raise ValueError("motor speeds must have magnitude between 10 and 100")
        payload += bytes(
            (
                left_port,
                right_port,
                left_speed_percent & 0xFF,
                right_speed_percent & 0xFF,
            )
        )
    elif any(
        value is not None
        for value in (
            left_port,
            right_port,
            left_speed_percent,
            right_speed_percent,
        )
    ):
        raise ValueError("pair speed parameters are valid only for the start action")
    return build_frame(MOTOR_PAIR_SPEED_COMMAND, payload)


def build_drive_steer_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    steering: int | None = None,
    speed_percent: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("drive steer action must fit uint8")
    payload = bytes((action,))
    if action == DRIVE_STEER_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("drive motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("drive motor ports must differ")
        if steering is None or not -100 <= steering <= 100:
            raise ValueError("drive steering must be between -100 and 100")
        if speed_percent is None or speed_percent == 0 or not -100 <= speed_percent <= 100:
            raise ValueError("drive speed must be between -100 and 100, excluding 0")
        payload += bytes(
            (left_port, right_port, steering & 0xFF, speed_percent & 0xFF)
        )
    elif any(
        value is not None
        for value in (left_port, right_port, steering, speed_percent)
    ):
        raise ValueError("drive steer parameters are valid only for the start action")
    return build_frame(DRIVE_STEER_COMMAND, payload)


def build_drive_steer_for_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    mode: int | None = None,
    steering: int | None = None,
    speed_percent: int | None = None,
    target_value: int | None = None,
    stop_mode: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("finite drive steer action must fit uint8")
    payload = bytes((action,))
    if action == DRIVE_STEER_FOR_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("drive motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("drive motor ports must differ")
        if mode not in (
            DRIVE_STEER_FOR_MODE_DEGREES,
            DRIVE_STEER_FOR_MODE_TIME_MS,
        ):
            raise ValueError("finite drive steer mode must be degrees or time")
        if steering is None or not -100 <= steering <= 100:
            raise ValueError("drive steering must be between -100 and 100")
        if speed_percent is None or speed_percent == 0 or not -100 <= speed_percent <= 100:
            raise ValueError("drive speed must be between -100 and 100, excluding 0")
        if target_value is None or target_value <= 0:
            raise ValueError("finite drive target must be greater than zero")
        if mode == DRIVE_STEER_FOR_MODE_DEGREES and target_value > 3600:
            raise ValueError("finite drive degrees must be between 1 and 3600")
        if mode == DRIVE_STEER_FOR_MODE_TIME_MS and target_value > 600000:
            raise ValueError("finite drive time must be between 1 and 600000 ms")
        if stop_mode not in (0, 1):
            raise ValueError("drive stop mode must be coast or brake")
        if mode == DRIVE_STEER_FOR_MODE_DEGREES and stop_mode != 0:
            raise ValueError("finite steering by degrees supports coast stop only")
        payload += bytes(
            (
                left_port,
                right_port,
                mode,
                steering & 0xFF,
                speed_percent & 0xFF,
            )
        )
        payload += target_value.to_bytes(4, "little", signed=True)
        payload += bytes((stop_mode,))
    elif any(
        value is not None
        for value in (
            left_port,
            right_port,
            mode,
            steering,
            speed_percent,
            target_value,
            stop_mode,
        )
    ):
        raise ValueError(
            "finite drive steer parameters are valid only for the start action"
        )
    return build_frame(DRIVE_STEER_FOR_COMMAND, payload)


def build_drive_straight_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    wheel_diameter_mm: int | None = None,
    axle_track_mm: int | None = None,
    distance_mm: int | None = None,
    speed_percent: int | None = None,
    stop_mode: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("drive straight action must fit uint8")
    payload = bytes((action,))
    if action == DRIVE_STRAIGHT_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("drive motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("drive motor ports must differ")
        if wheel_diameter_mm is None or not 1 <= wheel_diameter_mm <= 0xFFFF:
            raise ValueError("wheel diameter must be between 1 and 65535 mm")
        if axle_track_mm is None or not 1 <= axle_track_mm <= 0xFFFF:
            raise ValueError("axle track must be between 1 and 65535 mm")
        if distance_mm is None or distance_mm == 0 or not -(1 << 31) <= distance_mm < (1 << 31):
            raise ValueError("drive distance must be a nonzero signed 32-bit value")
        if speed_percent is None or not 10 <= speed_percent <= 100:
            raise ValueError("drive speed must be between 10 and 100 percent")
        if stop_mode != 0:
            raise ValueError("first drive straight mode supports coast stop only")
        payload += bytes((left_port, right_port))
        payload += wheel_diameter_mm.to_bytes(2, "little")
        payload += axle_track_mm.to_bytes(2, "little")
        payload += distance_mm.to_bytes(4, "little", signed=True)
        payload += bytes((speed_percent, stop_mode))
    elif any(
        value is not None
        for value in (
            left_port,
            right_port,
            wheel_diameter_mm,
            axle_track_mm,
            distance_mm,
            speed_percent,
            stop_mode,
        )
    ):
        raise ValueError("drive parameters are valid only for the start action")
    return build_frame(DRIVE_STRAIGHT_COMMAND, payload)


def build_drive_run_frame(
    action: int,
    left_port: int | None = None,
    right_port: int | None = None,
    mode: int | None = None,
    target_value: int | None = None,
    speed_percent: int | None = None,
    stop_mode: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("drive run action must fit uint8")
    payload = bytes((action,))
    if action == DRIVE_RUN_ACTION_START:
        if left_port not in (0, 1, 2, 3) or right_port not in (0, 1, 2, 3):
            raise ValueError("drive motor ports must be 0, 1, 2, or 3")
        if left_port == right_port:
            raise ValueError("drive motor ports must differ")
        if mode not in (DRIVE_RUN_MODE_DEGREES, DRIVE_RUN_MODE_TIME_MS):
            raise ValueError("drive run mode must be degrees or time")
        if target_value is None or target_value == 0:
            raise ValueError("drive target must not be zero")
        if mode == DRIVE_RUN_MODE_DEGREES and not -3600 <= target_value <= 3600:
            raise ValueError("drive degrees must be between -3600 and 3600")
        if mode == DRIVE_RUN_MODE_TIME_MS and not -600000 <= target_value <= 600000:
            raise ValueError("drive time must be between -600000 and 600000 ms")
        if speed_percent is None or not 10 <= speed_percent <= 100:
            raise ValueError("drive speed must be between 10 and 100 percent")
        if stop_mode not in (0, 1):
            raise ValueError("drive stop mode must be coast or brake")
        if mode == DRIVE_RUN_MODE_DEGREES and stop_mode != 0:
            raise ValueError("drive degrees currently supports coast stop only")
        payload += bytes((left_port, right_port, mode))
        payload += target_value.to_bytes(4, "little", signed=True)
        payload += bytes((speed_percent, stop_mode))
    elif any(
        value is not None
        for value in (
            left_port,
            right_port,
            mode,
            target_value,
            speed_percent,
            stop_mode,
        )
    ):
        raise ValueError("drive run parameters are valid only for the start action")
    return build_frame(DRIVE_RUN_COMMAND, payload)


def build_input_sensor_frame(
    action: int,
    sensor_port: int,
    mode: int | None = None,
) -> bytes:
    if not 0 <= action <= 0xFF:
        raise ValueError("input sensor action must fit uint8")
    if sensor_port not in (0, 1, 2, 3):
        raise ValueError("input sensor port must be 0, 1, 2, or 3")
    payload = bytes((action, sensor_port))
    if action == INPUT_SENSOR_ACTION_SET_MODE:
        if mode not in (0, 1, 2):
            raise ValueError("input sensor mode must be 0, 1, or 2")
        payload += bytes((mode,))
    elif mode is not None:
        raise ValueError("input sensor mode is valid only for set-mode")
    return build_frame(INPUT_SENSOR_COMMAND, payload)


def build_update_frame(total_frames: int, frame_index: int, payload: bytes) -> bytes:
    if not 0 < total_frames <= 0xFFFF:
        raise ValueError("total_frames must fit uint16 and be non-zero")
    if not 0 <= frame_index < total_frames:
        raise ValueError("frame_index is outside total_frames")
    if not 0 < len(payload) <= MAX_PAYLOAD:
        raise ValueError("payload must contain 1..1010 bytes")

    data = total_frames.to_bytes(2, "little")
    data += frame_index.to_bytes(2, "little")
    data += payload
    return build_frame(UPDATE_COMMAND, data)


def split_reports(frame: bytes, report_size: int = LEGACY_REPORT_SIZE) -> list[bytes]:
    return [
        frame[offset : offset + report_size].ljust(report_size, b"\x00")
        for offset in range(0, len(frame), report_size)
    ]


def parse_heartbeat_response(report: bytes) -> str | None:
    if len(report) < 13:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != HEARTBEAT_COMMAND:
        return None
    if report[3:5] != b"\x06\x00" or report[12] != FRAME_END:
        return None
    if checksum(report[:11]) != report[11]:
        return None
    return report[5:11].decode("ascii", errors="replace")


def parse_key_status_response(report: bytes) -> int | None:
    if len(report) < 8:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != KEY_STATUS_COMMAND:
        return None
    if report[3:5] != b"\x01\x00" or report[7] != FRAME_END:
        return None
    if checksum(report[:6]) != report[6]:
        return None
    return report[5] & 0x3F


def parse_flash_id_response(report: bytes) -> bytes | None:
    if len(report) < 10:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != FLASH_ID_COMMAND:
        return None
    if report[3:5] != b"\x03\x00" or report[9] != FRAME_END:
        return None
    if checksum(report[:8]) != report[8]:
        return None
    return bytes(report[5:8])


def _parse_flash_diagnostic_response(
    report: bytes, command: int
) -> tuple[int, bool, int] | None:
    if len(report) < 13:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != command:
        return None
    if report[3:5] != b"\x06\x00" or report[12] != FRAME_END:
        return None
    if checksum(report[:11]) != report[11]:
        return None
    return report[5], bool(report[6]), int.from_bytes(report[7:11], "little")


def parse_flash_scan_response(report: bytes) -> FlashScanResult | None:
    parsed = _parse_flash_diagnostic_response(report, FLASH_SCAN_COMMAND)
    if parsed is None:
        return None
    supported, erased, address = parsed
    return FlashScanResult(bool(supported), erased, address)


def parse_flash_test_response(report: bytes) -> FlashTestResult | None:
    parsed = _parse_flash_diagnostic_response(report, FLASH_TEST_COMMAND)
    if parsed is None:
        return None
    status, restored, address = parsed
    return FlashTestResult(status, restored, address)


def parse_flash_status_response(report: bytes) -> FlashStatus | None:
    if len(report) < 10:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != FLASH_STATUS_COMMAND:
        return None
    if report[3:5] != b"\x03\x00" or report[9] != FRAME_END:
        return None
    if checksum(report[:8]) != report[8]:
        return None
    return FlashStatus(report[5], report[6], report[7])


def parse_flash_mode_probe_response(report: bytes) -> FlashModeProbe | None:
    if len(report) < 13:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != FLASH_MODE_PROBE_COMMAND:
        return None
    if report[3:5] != b"\x06\x00" or report[12] != FRAME_END:
        return None
    if checksum(report[:11]) != report[11]:
        return None
    return FlashModeProbe(*report[5:11])


def parse_motor_test_response(report: bytes) -> MotorTestResult | None:
    if len(report) < 9:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_TEST_COMMAND:
        return None
    if report[3:5] != b"\x02\x00" or report[8] != FRAME_END:
        return None
    if checksum(report[:7]) != report[7]:
        return None
    return MotorTestResult(report[5], report[6])


def parse_motor_tacho_test_response(report: bytes) -> MotorTachoTestResult | None:
    if len(report) < 21:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_TACHO_TEST_COMMAND:
        return None
    if report[3:5] != b"\x0e\x00" or report[20] != FRAME_END:
        return None
    if checksum(report[:19]) != report[19]:
        return None
    return MotorTachoTestResult(
        result=report[5],
        state=report[6],
        total_count=int.from_bytes(report[7:11], "little", signed=True),
        forward_count=int.from_bytes(report[11:15], "little", signed=True),
        reverse_count=int.from_bytes(report[15:19], "little", signed=True),
    )


def parse_motor_stop_test_response(report: bytes) -> MotorStopTestResult | None:
    if len(report) < 22:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_STOP_TEST_COMMAND:
        return None
    if report[3:5] != b"\x0f\x00" or report[21] != FRAME_END:
        return None
    if checksum(report[:20]) != report[20]:
        return None
    return MotorStopTestResult(
        result=report[5],
        state=report[6],
        mode=report[7],
        total_count=int.from_bytes(report[8:12], "little", signed=True),
        powered_count=int.from_bytes(report[12:16], "little", signed=True),
        stopped_count=int.from_bytes(report[16:20], "little", signed=True),
    )


def parse_motor_dual_test_response(report: bytes) -> MotorDualTestResult | None:
    if len(report) < 25:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_DUAL_TEST_COMMAND:
        return None
    if report[3:5] != b"\x12\x00" or report[24] != FRAME_END:
        return None
    if checksum(report[:23]) != report[23]:
        return None
    return MotorDualTestResult(
        result=report[5],
        state=report[6],
        a_forward_count=int.from_bytes(report[7:11], "little", signed=True),
        b_forward_count=int.from_bytes(report[11:15], "little", signed=True),
        a_reverse_count=int.from_bytes(report[15:19], "little", signed=True),
        b_reverse_count=int.from_bytes(report[19:23], "little", signed=True),
    )


def parse_motor_control_response(report: bytes) -> MotorControlResult | None:
    if len(report) < 15:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_CONTROL_COMMAND:
        return None
    if report[3:5] != b"\x08\x00" or report[14] != FRAME_END:
        return None
    if checksum(report[:13]) != report[13]:
        return None
    return MotorControlResult(
        result=report[5],
        port=report[6],
        output_state=report[7],
        power_percent=int.from_bytes(report[8:9], "little", signed=True),
        tacho_count=int.from_bytes(report[9:13], "little", signed=True),
    )


def parse_motor_position_response(report: bytes) -> MotorPositionResult | None:
    if len(report) < 29:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_POSITION_COMMAND:
        return None
    if report[3:5] != b"\x16\x00" or report[28] != FRAME_END:
        return None
    if checksum(report[:27]) != report[27]:
        return None
    return MotorPositionResult(
        result=report[5],
        port=report[6],
        state=report[7],
        motor_type=report[8],
        requested_speed_percent=int.from_bytes(report[9:10], "little", signed=True),
        measured_speed_percent=int.from_bytes(report[10:11], "little", signed=True),
        start_count=int.from_bytes(report[11:15], "little", signed=True),
        target_count=int.from_bytes(report[15:19], "little", signed=True),
        current_count=int.from_bytes(report[19:23], "little", signed=True),
        error_count=int.from_bytes(report[23:27], "little", signed=True),
    )


def parse_motor_speed_response(report: bytes) -> MotorSpeedResult | None:
    if len(report) < 19:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_SPEED_COMMAND:
        return None
    if report[3:5] != b"\x0c\x00" or report[18] != FRAME_END:
        return None
    if checksum(report[:17]) != report[17]:
        return None
    return MotorSpeedResult(
        result=report[5],
        port=report[6],
        state=report[7],
        output_state=report[8],
        motor_type=report[9],
        requested_speed_percent=int.from_bytes(report[10:11], "little", signed=True),
        measured_speed_percent=int.from_bytes(report[11:12], "little", signed=True),
        power_percent=int.from_bytes(report[12:13], "little", signed=True),
        tacho_count=int.from_bytes(report[13:17], "little", signed=True),
    )


def parse_motor_pair_position_response(
    report: bytes,
) -> MotorPairPositionResult | None:
    if len(report) < 36:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_PAIR_POSITION_COMMAND:
        return None
    if report[3:5] != b"\x1d\x00" or report[35] != FRAME_END:
        return None
    if checksum(report[:34]) != report[34]:
        return None
    return MotorPairPositionResult(
        result=report[5],
        state=report[6],
        left_port=report[7],
        right_port=report[8],
        left_target_degrees=int.from_bytes(report[9:13], "little", signed=True),
        right_target_degrees=int.from_bytes(report[13:17], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[17:21], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[21:25], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(report[25:29], "little", signed=True),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[29:33], "little", signed=True
        ),
        error=int.from_bytes(report[33:34], "little", signed=True),
    )


def parse_motor_pair_speed_response(report: bytes) -> MotorPairSpeedResult | None:
    if len(report) < 34:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_PAIR_SPEED_COMMAND:
        return None
    if report[3:5] != b"\x1b\x00" or report[33] != FRAME_END:
        return None
    if checksum(report[:32]) != report[32]:
        return None
    return MotorPairSpeedResult(
        result=report[5],
        state=report[6],
        left_port=report[7],
        right_port=report[8],
        left_requested_speed_percent=int.from_bytes(
            report[9:10], "little", signed=True
        ),
        right_requested_speed_percent=int.from_bytes(
            report[10:11], "little", signed=True
        ),
        left_measured_speed_percent=int.from_bytes(
            report[11:12], "little", signed=True
        ),
        right_measured_speed_percent=int.from_bytes(
            report[12:13], "little", signed=True
        ),
        left_power_percent=int.from_bytes(report[13:14], "little", signed=True),
        right_power_percent=int.from_bytes(report[14:15], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[15:19], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[19:23], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(
            report[23:27], "little", signed=True
        ),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[27:31], "little", signed=True
        ),
        error=int.from_bytes(report[31:32], "little", signed=True),
    )


def parse_drive_steer_response(report: bytes) -> MotorPairSpeedResult | None:
    if len(report) < 34:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != DRIVE_STEER_COMMAND:
        return None
    if report[3:5] != b"\x1b\x00" or report[33] != FRAME_END:
        return None
    if checksum(report[:32]) != report[32]:
        return None
    return MotorPairSpeedResult(
        result=report[5],
        state=report[6],
        left_port=report[7],
        right_port=report[8],
        left_requested_speed_percent=int.from_bytes(
            report[9:10], "little", signed=True
        ),
        right_requested_speed_percent=int.from_bytes(
            report[10:11], "little", signed=True
        ),
        left_measured_speed_percent=int.from_bytes(
            report[11:12], "little", signed=True
        ),
        right_measured_speed_percent=int.from_bytes(
            report[12:13], "little", signed=True
        ),
        left_power_percent=int.from_bytes(report[13:14], "little", signed=True),
        right_power_percent=int.from_bytes(report[14:15], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[15:19], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[19:23], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(
            report[23:27], "little", signed=True
        ),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[27:31], "little", signed=True
        ),
        error=int.from_bytes(report[31:32], "little", signed=True),
    )


def parse_drive_steer_for_response(report: bytes) -> DriveSteerForResult | None:
    if len(report) < 49:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != DRIVE_STEER_FOR_COMMAND:
        return None
    if report[3:5] != b"\x2a\x00" or report[48] != FRAME_END:
        return None
    if checksum(report[:47]) != report[47]:
        return None
    return DriveSteerForResult(
        result=report[5],
        state=report[6],
        mode=report[7],
        left_port=report[8],
        right_port=report[9],
        steering=int.from_bytes(report[10:11], "little", signed=True),
        requested_speed_percent=int.from_bytes(
            report[11:12], "little", signed=True
        ),
        left_requested_speed_percent=int.from_bytes(
            report[12:13], "little", signed=True
        ),
        right_requested_speed_percent=int.from_bytes(
            report[13:14], "little", signed=True
        ),
        target_value=int.from_bytes(report[14:18], "little", signed=True),
        actual_value=int.from_bytes(report[18:22], "little", signed=True),
        left_target_degrees=int.from_bytes(report[22:26], "little", signed=True),
        right_target_degrees=int.from_bytes(report[26:30], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[30:34], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[34:38], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(
            report[38:42], "little", signed=True
        ),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[42:46], "little", signed=True
        ),
        error=int.from_bytes(report[46:47], "little", signed=True),
    )


def parse_drive_straight_response(report: bytes) -> DriveStraightResult | None:
    if len(report) < 44:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != DRIVE_STRAIGHT_COMMAND:
        return None
    if report[3:5] != b"\x25\x00" or report[43] != FRAME_END:
        return None
    if checksum(report[:42]) != report[42]:
        return None
    return DriveStraightResult(
        result=report[5],
        state=report[6],
        left_port=report[7],
        right_port=report[8],
        target_distance_mm=int.from_bytes(report[9:13], "little", signed=True),
        actual_distance_mm=int.from_bytes(report[13:17], "little", signed=True),
        left_target_degrees=int.from_bytes(report[17:21], "little", signed=True),
        right_target_degrees=int.from_bytes(report[21:25], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[25:29], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[29:33], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(
            report[33:37], "little", signed=True
        ),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[37:41], "little", signed=True
        ),
        error=int.from_bytes(report[41:42], "little", signed=True),
    )


def parse_drive_run_response(report: bytes) -> DriveRunResult | None:
    if len(report) < 38:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != DRIVE_RUN_COMMAND:
        return None
    if report[3:5] != b"\x1f\x00" or report[37] != FRAME_END:
        return None
    if checksum(report[:36]) != report[36]:
        return None
    return DriveRunResult(
        result=report[5],
        state=report[6],
        mode=report[7],
        left_port=report[8],
        right_port=report[9],
        requested_speed_percent=int.from_bytes(
            report[10:11], "little", signed=True
        ),
        target_value=int.from_bytes(report[11:15], "little", signed=True),
        actual_value=int.from_bytes(report[15:19], "little", signed=True),
        left_actual_degrees=int.from_bytes(report[19:23], "little", signed=True),
        right_actual_degrees=int.from_bytes(report[23:27], "little", signed=True),
        synchronization_error_degrees=int.from_bytes(
            report[27:31], "little", signed=True
        ),
        maximum_synchronization_error_degrees=int.from_bytes(
            report[31:35], "little", signed=True
        ),
        error=int.from_bytes(report[35:36], "little", signed=True),
    )


def parse_motor_type_response(report: bytes) -> MotorTypeResult | None:
    if len(report) < 5:
        return None
    payload_length = int.from_bytes(report[3:5], "little")
    if payload_length not in (21, 53, 57):
        return None
    checksum_index = 5 + payload_length
    end_index = checksum_index + 1
    if len(report) <= end_index:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MOTOR_TYPE_COMMAND:
        return None
    if report[end_index] != FRAME_END:
        return None
    if checksum(report[:checksum_index]) != report[checksum_index]:
        return None
    motors = []
    entry_size = {21: 5, 53: 13, 57: 14}[payload_length]
    for index in range(4):
        offset = 6 + (index * entry_size)
        motors.append(
            MotorIdentification(
                motor_type=report[offset],
                adc_raw=int.from_bytes(report[offset + 1 : offset + 3], "little"),
                millivolts=int.from_bytes(report[offset + 3 : offset + 5], "little"),
                pin6_low_adc_raw=(
                    int.from_bytes(report[offset + 5 : offset + 7], "little")
                    if entry_size >= 13
                    else 0
                ),
                pin6_low_millivolts=(
                    int.from_bytes(report[offset + 7 : offset + 9], "little")
                    if entry_size >= 13
                    else 0
                ),
                pin5_pullup_adc_raw=(
                    int.from_bytes(report[offset + 9 : offset + 11], "little")
                    if entry_size >= 13
                    else 0
                ),
                pin5_pullup_millivolts=(
                    int.from_bytes(report[offset + 11 : offset + 13], "little")
                    if entry_size >= 13
                    else 0
                ),
                pin5_pullup_high=(
                    report[offset + 13] != 0 if entry_size == 14 else False
                ),
            )
        )
    return MotorTypeResult(result=report[5], motors=tuple(motors))


def parse_input_sensor_response(report: bytes) -> InputSensorResult | None:
    if len(report) < 26:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != INPUT_SENSOR_COMMAND:
        return None
    if report[3:5] != b"\x13\x00" or report[25] != FRAME_END:
        return None
    if checksum(report[:24]) != report[24]:
        return None
    return InputSensorResult(
        result=report[5],
        port=report[6],
        state=report[7],
        sensor_type=report[8],
        mode=report[9],
        value_valid=report[10] == 1,
        value=int.from_bytes(report[11:13], "little"),
        adc0_raw=int.from_bytes(report[13:15], "little"),
        adc1_raw=int.from_bytes(report[15:17], "little"),
        digital_mask=report[17],
        rx_count=int.from_bytes(report[18:22], "little"),
        checksum_errors=int.from_bytes(report[22:24], "little"),
    )


def parse_device_status_response(report: bytes) -> DeviceStatus | None:
    payload_length = 72
    checksum_index = 5 + payload_length
    end_index = checksum_index + 1
    if len(report) <= end_index:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != DEVICE_STATUS_COMMAND:
        return None
    if report[3:5] != payload_length.to_bytes(2, "little"):
        return None
    if report[end_index] != FRAME_END:
        return None
    if checksum(report[:checksum_index]) != report[checksum_index]:
        return None

    payload = report[5:checksum_index]
    motors = []
    for index in range(4):
        offset = 24 + (index * 6)
        motors.append(
            DeviceMotorStatus(
                output_state=payload[offset],
                power_percent=int.from_bytes(
                    payload[offset + 1 : offset + 2], "little", signed=True
                ),
                tacho_count=int.from_bytes(
                    payload[offset + 2 : offset + 6], "little", signed=True
                ),
            )
        )
    sensors = []
    for index in range(4):
        offset = 48 + (index * 6)
        sensors.append(
            DeviceSensorStatus(
                state=payload[offset],
                sensor_type=payload[offset + 1],
                mode=payload[offset + 2],
                value_valid=payload[offset + 3] == 1,
                value=int.from_bytes(payload[offset + 4 : offset + 6], "little"),
            )
        )
    return DeviceStatus(
        protocol_major=payload[0],
        protocol_minor=payload[1],
        firmware_version=payload[2:8].decode("ascii", errors="replace"),
        motor_port_count=payload[8],
        sensor_port_count=payload[9],
        key_mask=payload[10] & 0x3F,
        battery_level=payload[11],
        battery_adc_raw=int.from_bytes(payload[12:14], "little"),
        battery_sample_mv=int.from_bytes(payload[14:16], "little"),
        capabilities=int.from_bytes(payload[16:20], "little"),
        uptime_ms=int.from_bytes(payload[20:24], "little"),
        motors=tuple(motors),
        sensors=tuple(sensors),
    )


def parse_micropython_status_response(report: bytes) -> MicroPythonStatus | None:
    payload_length = 28
    checksum_index = 5 + payload_length
    end_index = checksum_index + 1
    if len(report) <= end_index:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != MICROPYTHON_STATUS_COMMAND:
        return None
    if report[3:5] != payload_length.to_bytes(2, "little"):
        return None
    if report[end_index] != FRAME_END:
        return None
    if checksum(report[:checksum_index]) != report[checksum_index]:
        return None

    payload = report[5:checksum_index]
    return MicroPythonStatus(
        schema_version=payload[0],
        state=payload[1],
        flags=payload[2],
        heap_total_bytes=int.from_bytes(payload[4:8], "little"),
        heap_used_bytes=int.from_bytes(payload[8:12], "little"),
        heap_free_bytes=int.from_bytes(payload[12:16], "little"),
        startup_duration_ms=int.from_bytes(payload[16:20], "little"),
        maximum_gc_pause_us=int.from_bytes(payload[20:24], "little"),
        gc_count=int.from_bytes(payload[24:26], "little"),
        self_test_value=int.from_bytes(payload[26:28], "little"),
    )


def parse_python_program_response(report: bytes) -> PythonProgramStatus | None:
    payload_length = 32
    checksum_index = 5 + payload_length
    end_index = checksum_index + 1
    if len(report) <= end_index:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != PYTHON_PROGRAM_COMMAND:
        return None
    if report[3:5] != payload_length.to_bytes(2, "little"):
        return None
    if report[end_index] != FRAME_END:
        return None
    if checksum(report[:checksum_index]) != report[checksum_index]:
        return None

    payload = report[5:checksum_index]
    return PythonProgramStatus(
        schema_version=payload[0],
        result=payload[1],
        state=payload[2],
        error=payload[3],
        flags=payload[4],
        expected_length=int.from_bytes(payload[6:8], "little"),
        received_length=int.from_bytes(payload[8:10], "little"),
        run_count=int.from_bytes(payload[10:12], "little"),
        expected_crc32=int.from_bytes(payload[12:16], "little"),
        actual_crc32=int.from_bytes(payload[16:20], "little"),
        duration_ms=int.from_bytes(payload[20:24], "little"),
        timeout_ms=int.from_bytes(payload[24:28], "little"),
        result_value=int.from_bytes(payload[28:32], "little", signed=True),
    )


def parse_persistent_program_response(
    report: bytes,
) -> PersistentProgramStatus | None:
    if len(report) < 5:
        return None
    payload_length = int.from_bytes(report[3:5], "little")
    if payload_length not in (28, 40, 76):
        return None
    checksum_index = 5 + payload_length
    end_index = checksum_index + 1
    if len(report) <= end_index:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != PERSISTENT_PROGRAM_COMMAND:
        return None
    if report[end_index] != FRAME_END:
        return None
    if checksum(report[:checksum_index]) != report[checksum_index]:
        return None

    payload = report[5:checksum_index]
    if payload_length == 76:
        name_length = min(payload[14], PERSISTENT_PROGRAM_NAME_MAX_BYTES)
        return PersistentProgramStatus(
            schema_version=payload[0],
            result=payload[1],
            state=payload[2],
            flags=payload[3],
            erased_sector_mask=payload[39],
            occupied_sector_mask=payload[40],
            slot_count=payload[72],
            sectors_per_slot=payload[73],
            region_start=int.from_bytes(payload[20:24], "little"),
            region_size=int.from_bytes(payload[24:28], "little"),
            slot_size=int.from_bytes(payload[28:32], "little"),
            flash_size=int.from_bytes(payload[32:36], "little"),
            jedec_id=bytes(payload[36:39]),
            active_slot=payload[6],
            generation=int.from_bytes(payload[8:12], "little"),
            source_length=int.from_bytes(payload[12:14], "little"),
            record_type=payload[7],
            last_error=payload[15],
            source_crc32=int.from_bytes(payload[16:20], "little"),
            program_slot_id=payload[4],
            program_slot_count=payload[5],
            program_name=bytes(payload[41 : 41 + name_length]).decode(
                "utf-8", errors="replace"
            ),
        )
    return PersistentProgramStatus(
        schema_version=payload[0],
        result=payload[1],
        state=payload[2],
        flags=payload[3],
        erased_sector_mask=payload[4],
        occupied_sector_mask=payload[5],
        slot_count=payload[6],
        sectors_per_slot=payload[7],
        region_start=int.from_bytes(payload[8:12], "little"),
        region_size=int.from_bytes(payload[12:16], "little"),
        slot_size=int.from_bytes(payload[16:20], "little"),
        flash_size=int.from_bytes(payload[20:24], "little"),
        jedec_id=bytes(payload[24:27]),
        active_slot=payload[27] if payload_length == 40 else 0xFF,
        generation=(
            int.from_bytes(payload[28:32], "little") if payload_length == 40 else 0
        ),
        source_length=(
            int.from_bytes(payload[32:34], "little") if payload_length == 40 else 0
        ),
        record_type=payload[34] if payload_length == 40 else 0,
        last_error=payload[35] if payload_length == 40 else 0,
        source_crc32=(
            int.from_bytes(payload[36:40], "little") if payload_length == 40 else 0
        ),
        program_slot_id=0,
        program_slot_count=1,
        program_name="",
    )


def parse_update_ack(report: bytes) -> UpdateAck | None:
    if len(report) < 12:
        return None
    if report[0] != FRAME_START:
        return None
    if report[1] != DEVICE_DIRECTION or report[2] != UPDATE_COMMAND:
        return None
    if report[3:5] != b"\x05\x00" or report[11] != FRAME_END:
        return None
    if checksum(report[:10]) != report[10]:
        return None
    return UpdateAck(
        total_frames=int.from_bytes(report[5:7], "little"),
        frame_index=int.from_bytes(report[7:9], "little"),
        flag=report[9],
    )
