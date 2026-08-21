from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    DEVICE_DIRECTION,
    FRAME_END,
    FRAME_START,
    HEARTBEAT_COMMAND,
    HOST_DIRECTION,
    KEY_STATUS_COMMAND,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
    UPDATE_COMMAND,
)


@dataclass(frozen=True)
class UpdateAck:
    total_frames: int
    frame_index: int
    flag: int


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
