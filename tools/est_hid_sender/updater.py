from __future__ import annotations

import math
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Literal, Protocol

from .constants import (
    FIRST_PACKET_ACK_TIMEOUT_SECONDS,
    FRAGMENT_WRITE_DELAY_SECONDS,
    HEARTBEAT_TIMEOUT_SECONDS,
    HID_READ_TIMEOUT_MS,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
    PACKET_ACK_TIMEOUT_SECONDS,
)
from .protocol import (
    build_heartbeat_frame,
    build_update_frame,
    parse_heartbeat_response,
    parse_update_ack,
    split_reports,
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
        raise TimeoutError("心跳无回应")

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
                raise RuntimeError(f"第 {index + 1}/{total} 包失败，设备返回 {flag}")
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
        raise TimeoutError(f"等待第 {index + 1}/{total} 包回应超时")
