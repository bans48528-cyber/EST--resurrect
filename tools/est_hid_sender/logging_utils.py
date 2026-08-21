from __future__ import annotations

import logging
import os
from datetime import datetime
from pathlib import Path


def default_log_dir() -> Path:
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        return Path(local_app_data) / "ESTHidSender" / "logs"
    return Path.home() / ".est_hid_sender" / "logs"


class UpgradeLog:
    def __init__(self, directory: Path | None = None) -> None:
        log_dir = directory or default_log_dir()
        log_dir.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        self.path = log_dir / f"upgrade_{timestamp}.log"
        self._logger = logging.Logger(f"est_hid_sender.{timestamp}")
        self._logger.setLevel(logging.INFO)
        handler = logging.FileHandler(self.path, encoding="utf-8")
        handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
        self._logger.addHandler(handler)
        self._handler = handler
        self._closed = False

    def info(self, message: str, *args: object) -> None:
        self._logger.info(message, *args)

    def failure(self, error: BaseException) -> None:
        self._logger.error("result=failed error_type=%s error=%s", type(error).__name__, error)

    def success(self) -> None:
        self._logger.info("result=success")

    def close(self) -> None:
        if self._closed:
            return
        self._handler.flush()
        self._handler.close()
        self._logger.removeHandler(self._handler)
        self._closed = True

    def __enter__(self) -> "UpgradeLog":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.close()
