from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    DEVICE_DIRECTION,
    FLASH_ID_COMMAND,
    FLASH_SCAN_COMMAND,
    FLASH_TEST_COMMAND,
    FLASH_STATUS_COMMAND,
    FLASH_MODE_PROBE_COMMAND,
    FRAME_END,
    FRAME_START,
    HEARTBEAT_COMMAND,
    HOST_DIRECTION,
    KEY_STATUS_COMMAND,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
    MOTOR_CONTROL_ACTION_SET_POWER,
    MOTOR_CONTROL_COMMAND,
    MOTOR_DUAL_TEST_COMMAND,
    MOTOR_STOP_TEST_COMMAND,
    MOTOR_TACHO_TEST_COMMAND,
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


def checksum(data: bytes | bytearray) -> int:
    return sum(data) & 0xFF


def build_frame(command: int, payload: bytes = b"") -> bytes:
    frame = bytearray((FRAME_START, HOST_DIRECTION, command))
    frame += len(payload).to_bytes(2, "little")
    frame += payload
    frame.append(checksum(frame))
    frame.append(FRAME_END)
    return bytes(frame)


def build_heartbeat_frame() -> bytes:
    return build_frame(HEARTBEAT_COMMAND)


def build_key_status_frame() -> bytes:
    return build_frame(KEY_STATUS_COMMAND)


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
