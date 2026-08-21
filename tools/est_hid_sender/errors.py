from __future__ import annotations


class EstUpdaterError(Exception):
    """Base class for expected, user-facing updater failures."""

    code = "updater"


class DeviceNotFoundError(EstUpdaterError, RuntimeError):
    code = "device-not-found"


class FirmwareValidationError(EstUpdaterError, ValueError):
    code = "firmware-invalid"


class ManifestNotFoundError(FirmwareValidationError):
    code = "manifest-not-found"


class ManifestMismatchError(FirmwareValidationError):
    code = "manifest-mismatch"


class VersionSafetyError(EstUpdaterError):
    code = "version-safety"


class HeartbeatTimeoutError(EstUpdaterError, TimeoutError):
    code = "heartbeat-timeout"


class DiagnosticTimeoutError(EstUpdaterError, TimeoutError):
    code = "diagnostic-timeout"


class AckTimeoutError(EstUpdaterError, TimeoutError):
    code = "ack-timeout"


class AckRejectedError(EstUpdaterError, RuntimeError):
    code = "ack-rejected"
