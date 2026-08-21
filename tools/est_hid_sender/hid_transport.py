from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
from dataclasses import dataclass

from .constants import (
    HID_READ_TIMEOUT_MS,
    HID_WRITE_TIMEOUT_MS,
    LEGACY_REPORT_SIZE,
    PID,
    VID,
)

HIDP_STATUS_SUCCESS = 0x00110000

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
OPEN_EXISTING = 3
INVALID_HANDLE_VALUE = wt.HANDLE(-1).value
FILE_FLAG_OVERLAPPED = 0x40000000
DIGCF_PRESENT = 0x00000002
DIGCF_DEVICEINTERFACE = 0x00000010
ERROR_IO_PENDING = 997
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 258

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid = ctypes.WinDLL("hid", use_last_error=True)


class GUID(ctypes.Structure):
    _fields_ = (
        ("Data1", wt.DWORD),
        ("Data2", wt.WORD),
        ("Data3", wt.WORD),
        ("Data4", wt.BYTE * 8),
    )


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = (
        ("cbSize", wt.DWORD),
        ("InterfaceClassGuid", GUID),
        ("Flags", wt.DWORD),
        ("Reserved", ctypes.c_void_p),
    )


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = (
        ("Size", wt.ULONG),
        ("VendorID", wt.USHORT),
        ("ProductID", wt.USHORT),
        ("VersionNumber", wt.USHORT),
    )


class OVERLAPPED(ctypes.Structure):
    _fields_ = (
        ("Internal", ctypes.c_void_p),
        ("InternalHigh", ctypes.c_void_p),
        ("Offset", wt.DWORD),
        ("OffsetHigh", wt.DWORD),
        ("hEvent", wt.HANDLE),
    )


class HIDP_CAPS(ctypes.Structure):
    _fields_ = (
        ("Usage", wt.USHORT),
        ("UsagePage", wt.USHORT),
        ("InputReportByteLength", wt.USHORT),
        ("OutputReportByteLength", wt.USHORT),
        ("FeatureReportByteLength", wt.USHORT),
        ("Reserved", wt.USHORT * 17),
        ("NumberLinkCollectionNodes", wt.USHORT),
        ("NumberInputButtonCaps", wt.USHORT),
        ("NumberInputValueCaps", wt.USHORT),
        ("NumberInputDataIndices", wt.USHORT),
        ("NumberOutputButtonCaps", wt.USHORT),
        ("NumberOutputValueCaps", wt.USHORT),
        ("NumberOutputDataIndices", wt.USHORT),
        ("NumberFeatureButtonCaps", wt.USHORT),
        ("NumberFeatureValueCaps", wt.USHORT),
        ("NumberFeatureDataIndices", wt.USHORT),
    )


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = (
        ("ReadIntervalTimeout", wt.DWORD),
        ("ReadTotalTimeoutMultiplier", wt.DWORD),
        ("ReadTotalTimeoutConstant", wt.DWORD),
        ("WriteTotalTimeoutMultiplier", wt.DWORD),
        ("WriteTotalTimeoutConstant", wt.DWORD),
    )


hid.HidD_GetHidGuid.argtypes = (ctypes.POINTER(GUID),)
setupapi.SetupDiGetClassDevsW.argtypes = (
    ctypes.POINTER(GUID),
    wt.LPCWSTR,
    wt.HWND,
    wt.DWORD,
)
setupapi.SetupDiGetClassDevsW.restype = wt.HANDLE
setupapi.SetupDiEnumDeviceInterfaces.argtypes = (
    wt.HANDLE,
    ctypes.c_void_p,
    ctypes.POINTER(GUID),
    wt.DWORD,
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
)
setupapi.SetupDiEnumDeviceInterfaces.restype = wt.BOOL
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = (
    wt.HANDLE,
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
    ctypes.c_void_p,
    wt.DWORD,
    ctypes.POINTER(wt.DWORD),
    ctypes.c_void_p,
)
setupapi.SetupDiGetDeviceInterfaceDetailW.restype = wt.BOOL
setupapi.SetupDiDestroyDeviceInfoList.argtypes = (wt.HANDLE,)
kernel32.CreateFileW.argtypes = (
    wt.LPCWSTR,
    wt.DWORD,
    wt.DWORD,
    ctypes.c_void_p,
    wt.DWORD,
    wt.DWORD,
    wt.HANDLE,
)
kernel32.CreateFileW.restype = wt.HANDLE
kernel32.CloseHandle.argtypes = (wt.HANDLE,)
kernel32.CreateEventW.argtypes = (
    ctypes.c_void_p,
    wt.BOOL,
    wt.BOOL,
    wt.LPCWSTR,
)
kernel32.CreateEventW.restype = wt.HANDLE
kernel32.ReadFile.argtypes = (
    wt.HANDLE,
    ctypes.c_void_p,
    wt.DWORD,
    ctypes.POINTER(wt.DWORD),
    ctypes.POINTER(OVERLAPPED),
)
kernel32.ReadFile.restype = wt.BOOL
kernel32.WriteFile.argtypes = (
    wt.HANDLE,
    ctypes.c_void_p,
    wt.DWORD,
    ctypes.POINTER(wt.DWORD),
    ctypes.POINTER(OVERLAPPED),
)
kernel32.WriteFile.restype = wt.BOOL
kernel32.WaitForSingleObject.argtypes = (wt.HANDLE, wt.DWORD)
kernel32.WaitForSingleObject.restype = wt.DWORD
kernel32.GetOverlappedResult.argtypes = (
    wt.HANDLE,
    ctypes.POINTER(OVERLAPPED),
    ctypes.POINTER(wt.DWORD),
    wt.BOOL,
)
kernel32.GetOverlappedResult.restype = wt.BOOL
kernel32.CancelIo.argtypes = (wt.HANDLE,)
kernel32.CancelIo.restype = wt.BOOL
kernel32.SetCommTimeouts.argtypes = (wt.HANDLE, ctypes.c_void_p)
hid.HidD_GetAttributes.argtypes = (wt.HANDLE, ctypes.POINTER(HIDD_ATTRIBUTES))
hid.HidD_GetAttributes.restype = wt.BOOLEAN
hid.HidD_GetPreparsedData.argtypes = (wt.HANDLE, ctypes.POINTER(ctypes.c_void_p))
hid.HidD_GetPreparsedData.restype = wt.BOOLEAN
hid.HidD_FreePreparsedData.argtypes = (ctypes.c_void_p,)
hid.HidP_GetCaps.argtypes = (ctypes.c_void_p, ctypes.POINTER(HIDP_CAPS))
hid.HidP_GetCaps.restype = wt.LONG


@dataclass
class HidTransport:
    handle: int
    input_len: int
    output_len: int
    path: str

    @classmethod
    def open(cls, vid: int = VID, pid: int = PID) -> "HidTransport":
        return open_est_device(vid, pid)

    def close(self) -> None:
        if self.handle != INVALID_HANDLE_VALUE:
            kernel32.CloseHandle(self.handle)
            self.handle = INVALID_HANDLE_VALUE

    def __enter__(self) -> "HidTransport":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.close()

    def write_payload(self, payload: bytes) -> None:
        write_payload(self.handle, self.output_len, payload)

    def write_report(self, report: bytes) -> None:
        write_report(self.handle, self.output_len, report)

    def read_report(self, timeout_ms: int = HID_READ_TIMEOUT_MS) -> bytes | None:
        return read_report(self.handle, self.input_len, timeout_ms)


def fail_if_zero(ok: bool, action: str) -> None:
    if not ok:
        raise OSError(ctypes.get_last_error(), action)


def enumerate_hid_paths() -> list[str]:
    guid = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(guid))
    info = setupapi.SetupDiGetClassDevsW(
        ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    )
    if info == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), "SetupDiGetClassDevsW")
    paths: list[str] = []
    try:
        index = 0
        while True:
            data = SP_DEVICE_INTERFACE_DATA()
            data.cbSize = ctypes.sizeof(data)
            if not setupapi.SetupDiEnumDeviceInterfaces(
                info, None, ctypes.byref(guid), index, ctypes.byref(data)
            ):
                if ctypes.get_last_error() == 259:
                    break
                raise OSError(ctypes.get_last_error(), "SetupDiEnumDeviceInterfaces")
            needed = wt.DWORD()
            setupapi.SetupDiGetDeviceInterfaceDetailW(
                info, ctypes.byref(data), None, 0, ctypes.byref(needed), None
            )
            detail = ctypes.create_string_buffer(needed.value)
            ctypes.cast(detail, ctypes.POINTER(wt.DWORD))[0] = (
                8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            )
            fail_if_zero(
                setupapi.SetupDiGetDeviceInterfaceDetailW(
                    info,
                    ctypes.byref(data),
                    detail,
                    needed,
                    None,
                    None,
                ),
                "SetupDiGetDeviceInterfaceDetailW",
            )
            path = ctypes.wstring_at(ctypes.addressof(detail) + 4)
            paths.append(path)
            index += 1
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(info)
    return paths


def open_est_device(vid: int = VID, pid: int = PID) -> HidTransport:
    for path in enumerate_hid_paths():
        handle = kernel32.CreateFileW(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            None,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            None,
        )
        if handle == INVALID_HANDLE_VALUE:
            continue
        attrs = HIDD_ATTRIBUTES()
        attrs.Size = ctypes.sizeof(attrs)
        try:
            if not hid.HidD_GetAttributes(handle, ctypes.byref(attrs)):
                kernel32.CloseHandle(handle)
                continue
            if attrs.VendorID != vid or attrs.ProductID != pid:
                kernel32.CloseHandle(handle)
                continue
            preparsed = ctypes.c_void_p()
            input_len = output_len = LEGACY_REPORT_SIZE + 1
            if hid.HidD_GetPreparsedData(handle, ctypes.byref(preparsed)):
                caps = HIDP_CAPS()
                if hid.HidP_GetCaps(preparsed, ctypes.byref(caps)) == HIDP_STATUS_SUCCESS:
                    input_len = caps.InputReportByteLength
                    output_len = caps.OutputReportByteLength
                hid.HidD_FreePreparsedData(preparsed)
            timeouts = COMMTIMEOUTS(50, 0, 1200, 0, 1200)
            kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts))
            return HidTransport(handle, input_len, output_len, path)
        except Exception:
            kernel32.CloseHandle(handle)
            raise
    raise RuntimeError("未找到 EST HID 设备 VID_0483&PID_5750")


def wait_overlapped(handle: int, overlapped: OVERLAPPED, timeout_ms: int, action: str) -> int | None:
    wait = kernel32.WaitForSingleObject(overlapped.hEvent, timeout_ms)
    if wait == WAIT_TIMEOUT:
        kernel32.CancelIo(handle)
        return None
    if wait != WAIT_OBJECT_0:
        raise OSError(ctypes.get_last_error(), action)
    transferred = wt.DWORD()
    fail_if_zero(
        kernel32.GetOverlappedResult(
            handle, ctypes.byref(overlapped), ctypes.byref(transferred), False
        ),
        action,
    )
    return transferred.value


def write_payload(handle: int, output_len: int, payload: bytes) -> None:
    if len(payload) > output_len:
        raise ValueError("payload is too large for this HID report")
    if output_len <= LEGACY_REPORT_SIZE:
        data = payload[:output_len].ljust(output_len, b"\x00")
    else:
        data = (b"\x00" + payload).ljust(output_len, b"\x00")
    written = wt.DWORD()
    buf = ctypes.create_string_buffer(data)
    event = kernel32.CreateEventW(None, True, False, None)
    fail_if_zero(bool(event), "CreateEventW")
    overlapped = OVERLAPPED()
    overlapped.hEvent = event
    try:
        ok = kernel32.WriteFile(
            handle, buf, len(data), ctypes.byref(written), ctypes.byref(overlapped)
        )
        if not ok:
            error = ctypes.get_last_error()
            if error != ERROR_IO_PENDING:
                raise OSError(error, "WriteFile")
            transferred = wait_overlapped(
                handle, overlapped, HID_WRITE_TIMEOUT_MS, "WriteFile"
            )
            if transferred is None:
                raise TimeoutError("HID write timed out")
            written.value = transferred
    finally:
        kernel32.CloseHandle(event)
    if written.value != len(data):
        raise RuntimeError(f"HID 写入长度异常: {written.value}/{len(data)}")


def write_report(handle: int, output_len: int, report: bytes) -> None:
    if len(report) != LEGACY_REPORT_SIZE:
        raise ValueError("report must be 64 bytes")
    write_payload(handle, output_len, report)


def read_report(
    handle: int,
    input_len: int,
    timeout_ms: int = HID_READ_TIMEOUT_MS,
) -> bytes | None:
    buf = ctypes.create_string_buffer(input_len)
    read = wt.DWORD()
    event = kernel32.CreateEventW(None, True, False, None)
    fail_if_zero(bool(event), "CreateEventW")
    overlapped = OVERLAPPED()
    overlapped.hEvent = event
    try:
        ok = kernel32.ReadFile(
            handle, buf, input_len, ctypes.byref(read), ctypes.byref(overlapped)
        )
        if not ok:
            error = ctypes.get_last_error()
            if error != ERROR_IO_PENDING:
                return None
            transferred = wait_overlapped(handle, overlapped, timeout_ms, "ReadFile")
            if transferred is None:
                return None
            read.value = transferred
    finally:
        kernel32.CloseHandle(event)
    data = bytes(buf.raw[: read.value])
    if len(data) > LEGACY_REPORT_SIZE and data[:1] == b"\x00":
        data = data[1:]
    return data.ljust(LEGACY_REPORT_SIZE, b"\x00")[:LEGACY_REPORT_SIZE]
