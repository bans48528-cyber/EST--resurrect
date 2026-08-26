from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    DEVICE_DIRECTION,
    DEVICE_STATUS_COMMAND,
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
    MOTOR_CONTROL_ACTION_SET_POWER,
    MOTOR_CONTROL_COMMAND,
    MOTOR_DUAL_TEST_COMMAND,
    MOTOR_PAIR_POSITION_ACTION_START,
    MOTOR_PAIR_POSITION_COMMAND,
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
        if abs(left_degrees) != abs(right_degrees):
            raise ValueError("first synchronized mode requires equal absolute degrees")
        payload += bytes((left_port, right_port, speed_percent))
        payload += left_degrees.to_bytes(4, "little", signed=True)
        payload += right_degrees.to_bytes(4, "little", signed=True)
    elif any(
        value is not None
        for value in (left_port, right_port, speed_percent, left_degrees, right_degrees)
    ):
        raise ValueError("pair parameters are valid only for the start action")
    return build_frame(MOTOR_PAIR_POSITION_COMMAND, payload)


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
