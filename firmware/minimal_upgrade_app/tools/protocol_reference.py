"""Host-side reference helpers for the EST firmware update protocol."""

from __future__ import annotations

from dataclasses import dataclass

FRAME_START = 0x68
FRAME_END = 0x16
HOST_DIRECTION = 0x11
DEVICE_DIRECTION = 0x21
UPDATE_COMMAND = 0x05
MAX_PAYLOAD = 1010
REPORT_SIZE = 1024


def checksum(data: bytes) -> int:
    return sum(data) & 0xFF


def build_update_frame(
    total_frames: int, frame_index: int, payload: bytes
) -> bytes:
    if not 0 < total_frames <= 0xFFFF:
        raise ValueError("total_frames must fit uint16 and be non-zero")
    if not 0 <= frame_index < total_frames:
        raise ValueError("frame_index is outside total_frames")
    if not 0 < len(payload) <= MAX_PAYLOAD:
        raise ValueError("payload must contain 1..1010 bytes")

    data_length = len(payload) + 4
    frame = bytearray(
        (
            FRAME_START,
            HOST_DIRECTION,
            UPDATE_COMMAND,
            data_length & 0xFF,
            data_length >> 8,
            total_frames & 0xFF,
            total_frames >> 8,
            frame_index & 0xFF,
            frame_index >> 8,
        )
    )
    frame.extend(payload)
    frame.append(checksum(frame))
    frame.append(FRAME_END)
    return bytes(frame)


def split_hid_reports(frame: bytes) -> list[bytes]:
    if not frame:
        raise ValueError("frame is empty")
    reports = []
    for offset in range(0, len(frame), REPORT_SIZE):
        report = frame[offset : offset + REPORT_SIZE]
        reports.append(report.ljust(REPORT_SIZE, b"\x00"))
    return reports


@dataclass(frozen=True)
class UpdateAck:
    total_frames: int
    frame_index: int
    flag: int


def parse_update_ack(report: bytes) -> UpdateAck:
    if len(report) != REPORT_SIZE:
        raise ValueError("ACK must be one complete 1024-byte input report")
    if report[:3] != bytes((FRAME_START, DEVICE_DIRECTION, UPDATE_COMMAND)):
        raise ValueError("not an EST update ACK")
    if report[3:5] != b"\x05\x00" or report[11] != FRAME_END:
        raise ValueError("invalid update ACK framing")
    if checksum(report[:10]) != report[10]:
        raise ValueError("invalid update ACK checksum")
    if any(report[12:]):
        raise ValueError("ACK padding is not zero-filled")
    return UpdateAck(
        total_frames=int.from_bytes(report[5:7], "little"),
        frame_index=int.from_bytes(report[7:9], "little"),
        flag=report[9],
    )
