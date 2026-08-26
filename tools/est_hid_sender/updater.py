from __future__ import annotations

import math
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Literal, Protocol

from .constants import (
    DEVICE_STATUS_TIMEOUT_SECONDS,
    DRIVE_STRAIGHT_ACTION_START,
    DRIVE_STRAIGHT_ACTION_STATUS,
    DRIVE_STRAIGHT_ACTION_STOP,
    DRIVE_RUN_ACTION_START,
    DRIVE_RUN_ACTION_STATUS,
    DRIVE_RUN_ACTION_STOP,
    DRIVE_STEER_ACTION_BRAKE,
    DRIVE_STEER_ACTION_COAST,
    DRIVE_STEER_ACTION_START,
    DRIVE_STEER_ACTION_STATUS,
    FIRST_PACKET_ACK_TIMEOUT_SECONDS,
    FLASH_DIAGNOSTIC_TIMEOUT_SECONDS,
    FLASH_ID_TIMEOUT_SECONDS,
    FRAGMENT_WRITE_DELAY_SECONDS,
    HEARTBEAT_TIMEOUT_SECONDS,
    HID_READ_TIMEOUT_MS,
    INPUT_SENSOR_ACTION_RESTART,
    INPUT_SENSOR_ACTION_SET_MODE,
    INPUT_SENSOR_ACTION_STATUS,
    INPUT_SENSOR_TIMEOUT_SECONDS,
    LEGACY_REPORT_SIZE,
    KEY_STATUS_TIMEOUT_SECONDS,
    MAX_PAYLOAD,
    MOTOR_CONTROL_ACTION_BRAKE,
    MOTOR_CONTROL_ACTION_COAST,
    MOTOR_CONTROL_ACTION_RESET_TACHO,
    MOTOR_CONTROL_ACTION_SET_POWER,
    MOTOR_CONTROL_ACTION_STATUS,
    MOTOR_POSITION_ACTION_START,
    MOTOR_POSITION_ACTION_STATUS,
    MOTOR_POSITION_ACTION_STOP,
    MOTOR_PAIR_POSITION_ACTION_START,
    MOTOR_PAIR_POSITION_ACTION_STATUS,
    MOTOR_PAIR_POSITION_ACTION_STOP,
    MOTOR_PAIR_SPEED_ACTION_BRAKE,
    MOTOR_PAIR_SPEED_ACTION_COAST,
    MOTOR_PAIR_SPEED_ACTION_START,
    MOTOR_PAIR_SPEED_ACTION_STATUS,
    MOTOR_SPEED_ACTION_BRAKE,
    MOTOR_SPEED_ACTION_COAST,
    MOTOR_SPEED_ACTION_START,
    MOTOR_SPEED_ACTION_STATUS,
    MOTOR_TYPE_ACTION_REFRESH,
    MOTOR_TEST_ACTION_START,
    MOTOR_TEST_ACTION_STATUS,
    MOTOR_TEST_ACTION_STOP,
    MOTOR_TEST_TIMEOUT_SECONDS,
    PACKET_ACK_TIMEOUT_SECONDS,
)
from .errors import (
    AckRejectedError,
    AckTimeoutError,
    DiagnosticTimeoutError,
    HeartbeatTimeoutError,
)
from .protocol import (
    build_drive_straight_frame,
    build_drive_run_frame,
    build_drive_steer_frame,
    build_device_status_frame,
    build_heartbeat_frame,
    build_input_sensor_frame,
    build_flash_id_frame,
    build_flash_scan_frame,
    build_flash_test_frame,
    build_flash_status_frame,
    build_flash_mode_probe_frame,
    build_key_status_frame,
    build_motor_control_frame,
    build_motor_dual_test_frame,
    build_motor_position_frame,
    build_motor_pair_position_frame,
    build_motor_pair_speed_frame,
    build_motor_speed_frame,
    build_motor_stop_test_frame,
    build_motor_tacho_test_frame,
    build_motor_test_frame,
    build_motor_type_frame,
    build_update_frame,
    parse_heartbeat_response,
    parse_drive_straight_response,
    parse_drive_run_response,
    parse_drive_steer_response,
    parse_device_status_response,
    parse_input_sensor_response,
    parse_flash_id_response,
    parse_flash_scan_response,
    parse_flash_test_response,
    parse_flash_status_response,
    parse_flash_mode_probe_response,
    parse_key_status_response,
    parse_motor_control_response,
    parse_motor_dual_test_response,
    parse_motor_position_response,
    parse_motor_pair_position_response,
    parse_motor_pair_speed_response,
    parse_motor_speed_response,
    parse_motor_stop_test_response,
    parse_motor_tacho_test_response,
    parse_motor_test_response,
    parse_motor_type_response,
    parse_update_ack,
    split_reports,
    FlashScanResult,
    FlashTestResult,
    FlashStatus,
    FlashModeProbe,
    MotorTestResult,
    MotorTachoTestResult,
    MotorStopTestResult,
    MotorDualTestResult,
    MotorPositionResult,
    MotorPairPositionResult,
    MotorPairSpeedResult,
    MotorSpeedResult,
    MotorControlResult,
    MotorTypeResult,
    InputSensorResult,
    DeviceStatus,
    DriveStraightResult,
    DriveRunResult,
)


class Transport(Protocol):
    input_len: int
    output_len: int
    path: str

    def write_payload(self, payload: bytes) -> None:
        ...

    def write_report(self, report: bytes) -> None:
        ...

    def read_report(self, timeout_ms: int = HID_READ_TIMEOUT_MS) -> bytes | None:
        ...


@dataclass(frozen=True)
class PacketProgress:
    sent: int
    total: int
    phase: Literal["sending", "acked"]


ProgressCallback = Callable[[PacketProgress], None]


class FirmwareUpdater:
    def __init__(self, transport: Transport) -> None:
        self.transport = transport

    def ping(self) -> str:
        report = build_heartbeat_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + HEARTBEAT_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            report = self.transport.read_report()
            if not report:
                continue
            version = parse_heartbeat_response(report)
            if version is not None:
                return version
        raise HeartbeatTimeoutError(
            "设备已枚举，但心跳回应超时；请确认设备处于正常 APP 模式后重试"
        )

    def read_key_mask(self) -> int:
        report = build_key_status_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + KEY_STATUS_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            report = self.transport.read_report()
            if not report:
                continue
            key_mask = parse_key_status_response(report)
            if key_mask is not None:
                return key_mask
        raise DiagnosticTimeoutError(
            "设备没有返回按键状态；请确认固件支持 keys 命令"
        )

    def read_device_status(self) -> DeviceStatus:
        report = build_device_status_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + DEVICE_STATUS_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            status = parse_device_status_response(response)
            if status is not None:
                return status
        raise DiagnosticTimeoutError(
            "设备没有返回整机状态；请确认固件支持 device-status 命令"
        )

    def read_flash_id(self) -> bytes:
        report = build_flash_id_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + FLASH_ID_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            report = self.transport.read_report()
            if not report:
                continue
            jedec_id = parse_flash_id_response(report)
            if jedec_id is not None:
                return jedec_id
        raise DiagnosticTimeoutError(
            "设备没有返回外部 Flash ID；请确认固件支持 flash-id 命令"
        )

    def scan_flash_test_sector(self) -> FlashScanResult:
        report = build_flash_scan_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + FLASH_DIAGNOSTIC_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_flash_scan_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回 Flash 空白区检查结果；请确认固件支持 flash-scan 命令"
        )

    def test_flash_4byte_addressing(self) -> FlashTestResult:
        report = build_flash_test_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + FLASH_DIAGNOSTIC_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_flash_test_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回 Flash 读写测试结果；请确认设备仍在线"
        )

    def read_flash_status(self) -> FlashStatus:
        report = build_flash_status_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + FLASH_DIAGNOSTIC_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            status = parse_flash_status_response(response)
            if status is not None:
                return status
        raise DiagnosticTimeoutError(
            "设备没有返回 Flash 保护状态；请确认固件支持 flash-status 命令"
        )

    def probe_flash_modes(self) -> FlashModeProbe:
        report = build_flash_mode_probe_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + FLASH_DIAGNOSTIC_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            probe = parse_flash_mode_probe_response(response)
            if probe is not None:
                return probe
        raise DiagnosticTimeoutError(
            "设备没有返回 Flash 模式检测结果；请确认固件支持 flash-mode-probe 命令"
        )

    def start_motor_test(self) -> MotorTestResult:
        return self._motor_test_action(MOTOR_TEST_ACTION_START)

    def read_motor_test_status(self) -> MotorTestResult:
        return self._motor_test_action(MOTOR_TEST_ACTION_STATUS)

    def stop_motor_test(self) -> MotorTestResult:
        return self._motor_test_action(MOTOR_TEST_ACTION_STOP)

    def _motor_test_action(self, action: int) -> MotorTestResult:
        report = build_motor_test_frame(action).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_test_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达测试状态；请断开马达并确认固件支持 motor-test 命令"
        )

    def start_motor_tacho_test(
        self, power_percent: int | None = None, motor_port: int | None = None
    ) -> MotorTachoTestResult:
        return self._motor_tacho_test_action(
            MOTOR_TEST_ACTION_START,
            power_percent=power_percent,
            motor_port=motor_port,
        )

    def read_motor_tacho_test_status(self) -> MotorTachoTestResult:
        return self._motor_tacho_test_action(MOTOR_TEST_ACTION_STATUS)

    def stop_motor_tacho_test(self) -> MotorTachoTestResult:
        return self._motor_tacho_test_action(MOTOR_TEST_ACTION_STOP)

    def _motor_tacho_test_action(
        self,
        action: int,
        power_percent: int | None = None,
        motor_port: int | None = None,
    ) -> MotorTachoTestResult:
        report = build_motor_tacho_test_frame(
            action, power_percent, motor_port
        ).ljust(
            LEGACY_REPORT_SIZE, b"\x00"
        )
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_tacho_test_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达测速结果；请确认固件支持 motor-tacho-test 命令"
        )

    def start_motor_stop_test(
        self,
        stop_mode: int,
        power_percent: int,
        motor_port: int | None = None,
    ) -> MotorStopTestResult:
        return self._motor_stop_test_action(
            MOTOR_TEST_ACTION_START,
            stop_mode=stop_mode,
            power_percent=power_percent,
            motor_port=motor_port,
        )

    def read_motor_stop_test_status(self) -> MotorStopTestResult:
        return self._motor_stop_test_action(MOTOR_TEST_ACTION_STATUS)

    def stop_motor_stop_test(self) -> MotorStopTestResult:
        return self._motor_stop_test_action(MOTOR_TEST_ACTION_STOP)

    def _motor_stop_test_action(
        self,
        action: int,
        stop_mode: int | None = None,
        power_percent: int | None = None,
        motor_port: int | None = None,
    ) -> MotorStopTestResult:
        report = build_motor_stop_test_frame(
            action, stop_mode, power_percent, motor_port
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_stop_test_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回停车对比结果；请确认固件支持 motor-stop-compare 命令"
        )

    def start_motor_dual_test(self, power_percent: int) -> MotorDualTestResult:
        return self._motor_dual_test_action(
            MOTOR_TEST_ACTION_START, power_percent=power_percent
        )

    def read_motor_dual_test_status(self) -> MotorDualTestResult:
        return self._motor_dual_test_action(MOTOR_TEST_ACTION_STATUS)

    def stop_motor_dual_test(self) -> MotorDualTestResult:
        return self._motor_dual_test_action(MOTOR_TEST_ACTION_STOP)

    def _motor_dual_test_action(
        self, action: int, power_percent: int | None = None
    ) -> MotorDualTestResult:
        report = build_motor_dual_test_frame(action, power_percent).ljust(
            LEGACY_REPORT_SIZE, b"\x00"
        )
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_dual_test_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回 A/B 双马达结果；请确认固件支持 motor-dual-test 命令"
        )

    def read_motor_control_status(self, motor_port: int) -> MotorControlResult:
        return self._motor_control_action(MOTOR_CONTROL_ACTION_STATUS, motor_port)

    def read_motor_types(self) -> MotorTypeResult:
        report = build_motor_type_frame().ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_type_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达类型；请确认固件支持 motor-types 命令"
        )

    def refresh_motor_type(self, motor_port: int) -> MotorTypeResult:
        report = build_motor_type_frame(
            MOTOR_TYPE_ACTION_REFRESH, motor_port
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_type_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达静止识别状态；请确认固件支持 motor-identify 命令"
        )

    def set_motor_power(
        self, motor_port: int, power_percent: int
    ) -> MotorControlResult:
        return self._motor_control_action(
            MOTOR_CONTROL_ACTION_SET_POWER,
            motor_port,
            power_percent=power_percent,
        )

    def coast_motor(self, motor_port: int) -> MotorControlResult:
        return self._motor_control_action(MOTOR_CONTROL_ACTION_COAST, motor_port)

    def brake_motor(self, motor_port: int) -> MotorControlResult:
        return self._motor_control_action(MOTOR_CONTROL_ACTION_BRAKE, motor_port)

    def reset_motor_tacho(self, motor_port: int) -> MotorControlResult:
        return self._motor_control_action(
            MOTOR_CONTROL_ACTION_RESET_TACHO, motor_port
        )

    def _motor_control_action(
        self,
        action: int,
        motor_port: int,
        power_percent: int | None = None,
    ) -> MotorControlResult:
        report = build_motor_control_frame(
            action, motor_port, power_percent
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_control_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回通用马达控制状态；请确认固件支持 motor-control 命令"
        )

    def start_motor_position(
        self, motor_port: int, speed_percent: int, degrees: int
    ) -> MotorPositionResult:
        return self._motor_position_action(
            MOTOR_POSITION_ACTION_START,
            motor_port,
            speed_percent=speed_percent,
            degrees=degrees,
        )

    def read_motor_position_status(self, motor_port: int) -> MotorPositionResult:
        return self._motor_position_action(MOTOR_POSITION_ACTION_STATUS, motor_port)

    def stop_motor_position(self, motor_port: int) -> MotorPositionResult:
        return self._motor_position_action(MOTOR_POSITION_ACTION_STOP, motor_port)

    def _motor_position_action(
        self,
        action: int,
        motor_port: int,
        speed_percent: int | None = None,
        degrees: int | None = None,
    ) -> MotorPositionResult:
        report = build_motor_position_frame(
            action, motor_port, speed_percent, degrees
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_position_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达位置状态；请确认固件支持 motor-position 命令"
        )

    def start_motor_pair_position(
        self,
        left_port: int,
        left_degrees: int,
        right_port: int,
        right_degrees: int,
        speed_percent: int,
    ) -> MotorPairPositionResult:
        return self._motor_pair_position_action(
            MOTOR_PAIR_POSITION_ACTION_START,
            left_port=left_port,
            left_degrees=left_degrees,
            right_port=right_port,
            right_degrees=right_degrees,
            speed_percent=speed_percent,
        )

    def read_motor_pair_position_status(self) -> MotorPairPositionResult:
        return self._motor_pair_position_action(
            MOTOR_PAIR_POSITION_ACTION_STATUS
        )

    def stop_motor_pair_position(self) -> MotorPairPositionResult:
        return self._motor_pair_position_action(
            MOTOR_PAIR_POSITION_ACTION_STOP
        )

    def _motor_pair_position_action(
        self,
        action: int,
        left_port: int | None = None,
        left_degrees: int | None = None,
        right_port: int | None = None,
        right_degrees: int | None = None,
        speed_percent: int | None = None,
    ) -> MotorPairPositionResult:
        report = build_motor_pair_position_frame(
            action,
            left_port=left_port,
            right_port=right_port,
            speed_percent=speed_percent,
            left_degrees=left_degrees,
            right_degrees=right_degrees,
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_pair_position_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回双马达同步状态；请确认固件支持 motor-pair-position 命令"
        )

    def start_motor_pair_speed(
        self,
        left_port: int,
        left_speed_percent: int,
        right_port: int,
        right_speed_percent: int,
    ) -> MotorPairSpeedResult:
        return self._motor_pair_speed_action(
            MOTOR_PAIR_SPEED_ACTION_START,
            left_port=left_port,
            right_port=right_port,
            left_speed_percent=left_speed_percent,
            right_speed_percent=right_speed_percent,
        )

    def read_motor_pair_speed_status(self) -> MotorPairSpeedResult:
        return self._motor_pair_speed_action(MOTOR_PAIR_SPEED_ACTION_STATUS)

    def stop_motor_pair_speed(
        self, stop_mode: Literal["coast", "brake"] = "coast"
    ) -> MotorPairSpeedResult:
        action = (
            MOTOR_PAIR_SPEED_ACTION_BRAKE
            if stop_mode == "brake"
            else MOTOR_PAIR_SPEED_ACTION_COAST
        )
        return self._motor_pair_speed_action(action)

    def _motor_pair_speed_action(
        self,
        action: int,
        left_port: int | None = None,
        right_port: int | None = None,
        left_speed_percent: int | None = None,
        right_speed_percent: int | None = None,
    ) -> MotorPairSpeedResult:
        report = build_motor_pair_speed_frame(
            action,
            left_port=left_port,
            right_port=right_port,
            left_speed_percent=left_speed_percent,
            right_speed_percent=right_speed_percent,
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_pair_speed_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回双马达持续定速状态；请确认固件支持 motor-pair-speed 命令"
        )

    def start_drive_steer(
        self,
        left_port: int,
        right_port: int,
        steering: int,
        speed_percent: int,
    ) -> MotorPairSpeedResult:
        return self._drive_steer_action(
            DRIVE_STEER_ACTION_START,
            left_port=left_port,
            right_port=right_port,
            steering=steering,
            speed_percent=speed_percent,
        )

    def read_drive_steer_status(self) -> MotorPairSpeedResult:
        return self._drive_steer_action(DRIVE_STEER_ACTION_STATUS)

    def stop_drive_steer(
        self, stop_mode: Literal["coast", "brake"] = "coast"
    ) -> MotorPairSpeedResult:
        action = (
            DRIVE_STEER_ACTION_BRAKE
            if stop_mode == "brake"
            else DRIVE_STEER_ACTION_COAST
        )
        return self._drive_steer_action(action)

    def _drive_steer_action(
        self,
        action: int,
        left_port: int | None = None,
        right_port: int | None = None,
        steering: int | None = None,
        speed_percent: int | None = None,
    ) -> MotorPairSpeedResult:
        report = build_drive_steer_frame(
            action,
            left_port=left_port,
            right_port=right_port,
            steering=steering,
            speed_percent=speed_percent,
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_drive_steer_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回持续转向状态；请确认固件支持 drive-steer 命令"
        )

    def start_drive_straight(
        self,
        left_port: int,
        right_port: int,
        wheel_diameter_mm: int,
        axle_track_mm: int,
        distance_mm: int,
        speed_percent: int,
    ) -> DriveStraightResult:
        return self._drive_straight_action(
            DRIVE_STRAIGHT_ACTION_START,
            left_port=left_port,
            right_port=right_port,
            wheel_diameter_mm=wheel_diameter_mm,
            axle_track_mm=axle_track_mm,
            distance_mm=distance_mm,
            speed_percent=speed_percent,
            stop_mode=0,
        )

    def read_drive_straight_status(self) -> DriveStraightResult:
        return self._drive_straight_action(DRIVE_STRAIGHT_ACTION_STATUS)

    def stop_drive_straight(self) -> DriveStraightResult:
        return self._drive_straight_action(DRIVE_STRAIGHT_ACTION_STOP)

    def _drive_straight_action(
        self,
        action: int,
        left_port: int | None = None,
        right_port: int | None = None,
        wheel_diameter_mm: int | None = None,
        axle_track_mm: int | None = None,
        distance_mm: int | None = None,
        speed_percent: int | None = None,
        stop_mode: int | None = None,
    ) -> DriveStraightResult:
        report = build_drive_straight_frame(
            action,
            left_port=left_port,
            right_port=right_port,
            wheel_diameter_mm=wheel_diameter_mm,
            axle_track_mm=axle_track_mm,
            distance_mm=distance_mm,
            speed_percent=speed_percent,
            stop_mode=stop_mode,
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_drive_straight_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回底盘直行状态；请确认固件支持 drive-straight 命令"
        )

    def start_drive_run(
        self,
        left_port: int,
        right_port: int,
        mode: int,
        target_value: int,
        speed_percent: int,
        stop_mode: Literal["coast", "brake"] = "coast",
    ) -> DriveRunResult:
        return self._drive_run_action(
            DRIVE_RUN_ACTION_START,
            left_port=left_port,
            right_port=right_port,
            mode=mode,
            target_value=target_value,
            speed_percent=speed_percent,
            stop_mode=1 if stop_mode == "brake" else 0,
        )

    def read_drive_run_status(self) -> DriveRunResult:
        return self._drive_run_action(DRIVE_RUN_ACTION_STATUS)

    def stop_drive_run(self) -> DriveRunResult:
        return self._drive_run_action(DRIVE_RUN_ACTION_STOP)

    def _drive_run_action(
        self,
        action: int,
        left_port: int | None = None,
        right_port: int | None = None,
        mode: int | None = None,
        target_value: int | None = None,
        speed_percent: int | None = None,
        stop_mode: int | None = None,
    ) -> DriveRunResult:
        report = build_drive_run_frame(
            action,
            left_port=left_port,
            right_port=right_port,
            mode=mode,
            target_value=target_value,
            speed_percent=speed_percent,
            stop_mode=stop_mode,
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_drive_run_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回圈/度/秒直行状态；请确认固件支持 drive-run 命令"
        )

    def start_motor_speed(
        self, motor_port: int, speed_percent: int
    ) -> MotorSpeedResult:
        return self._motor_speed_action(
            MOTOR_SPEED_ACTION_START,
            motor_port,
            speed_percent=speed_percent,
        )

    def read_motor_speed_status(self, motor_port: int) -> MotorSpeedResult:
        return self._motor_speed_action(MOTOR_SPEED_ACTION_STATUS, motor_port)

    def stop_motor_speed(
        self, motor_port: int, stop_mode: Literal["coast", "brake"] = "coast"
    ) -> MotorSpeedResult:
        action = (
            MOTOR_SPEED_ACTION_BRAKE
            if stop_mode == "brake"
            else MOTOR_SPEED_ACTION_COAST
        )
        return self._motor_speed_action(action, motor_port)

    def _motor_speed_action(
        self,
        action: int,
        motor_port: int,
        speed_percent: int | None = None,
    ) -> MotorSpeedResult:
        report = build_motor_speed_frame(
            action, motor_port, speed_percent
        ).ljust(LEGACY_REPORT_SIZE, b"\x00")
        self.transport.write_report(report)
        deadline = time.monotonic() + MOTOR_TEST_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_motor_speed_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回马达定速状态；请确认固件支持 motor-speed 命令"
        )

    def read_input_sensor(self, sensor_port: int = 0) -> InputSensorResult:
        return self._input_sensor_action(INPUT_SENSOR_ACTION_STATUS, sensor_port)

    def set_input_sensor_mode(
        self, sensor_port: int, mode: int
    ) -> InputSensorResult:
        return self._input_sensor_action(
            INPUT_SENSOR_ACTION_SET_MODE, sensor_port, mode=mode
        )

    def restart_input_sensor(self, sensor_port: int = 0) -> InputSensorResult:
        return self._input_sensor_action(INPUT_SENSOR_ACTION_RESTART, sensor_port)

    def _input_sensor_action(
        self,
        action: int,
        sensor_port: int,
        mode: int | None = None,
    ) -> InputSensorResult:
        report = build_input_sensor_frame(action, sensor_port, mode).ljust(
            LEGACY_REPORT_SIZE, b"\x00"
        )
        self.transport.write_report(report)
        deadline = time.monotonic() + INPUT_SENSOR_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            response = self.transport.read_report()
            if not response:
                continue
            result = parse_input_sensor_response(response)
            if result is not None:
                return result
        raise DiagnosticTimeoutError(
            "设备没有返回输入传感器状态；请确认固件支持 sensor-read 命令"
        )

    def flash(
        self,
        firmware: bytes,
        progress: ProgressCallback | None = None,
    ) -> None:
        total = math.ceil(len(firmware) / MAX_PAYLOAD)
        for index in range(total):
            payload = firmware[index * MAX_PAYLOAD : (index + 1) * MAX_PAYLOAD]
            frame = build_update_frame(total, index, payload)
            if progress is not None:
                progress(PacketProgress(index + 1, total, "sending"))
            self._write_frame(frame)
            flag = self._read_matching_ack(total, index)
            if flag != 1:
                raise AckRejectedError(
                    f"第 {index + 1}/{total} 包被设备拒绝：ACK flag={flag}（成功值应为 1）"
                )
            if progress is not None:
                progress(PacketProgress(index + 1, total, "acked"))

    def _write_frame(self, frame: bytes) -> None:
        if self._can_send_as_single_hid_payload(frame):
            self.transport.write_payload(frame)
            return
        for report in split_reports(frame):
            self.transport.write_report(report)
            time.sleep(FRAGMENT_WRITE_DELAY_SECONDS)

    def _can_send_as_single_hid_payload(self, frame: bytes) -> bool:
        return (
            self.transport.output_len > LEGACY_REPORT_SIZE + 1
            and len(frame) <= self.transport.output_len - 1
        )

    def _read_matching_ack(self, total: int, index: int) -> int:
        # Packet zero makes the bootloader erase four flash sectors before its ACK.
        timeout = (
            FIRST_PACKET_ACK_TIMEOUT_SECONDS
            if index == 0
            else PACKET_ACK_TIMEOUT_SECONDS
        )
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            report = self.transport.read_report()
            if not report:
                continue
            ack = parse_update_ack(report)
            if ack is None:
                continue
            if ack.total_frames == total and ack.frame_index == index:
                return ack.flag
        raise AckTimeoutError(
            f"等待第 {index + 1}/{total} 包 ACK 超时；请保留日志并重新连接设备后重试"
        )
