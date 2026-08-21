from __future__ import annotations

import argparse
from pathlib import Path

from .firmware import load_firmware_package
from .hid_transport import HidTransport
from .updater import FirmwareUpdater, PacketProgress


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="EST USB HID firmware update tool")
    parser.add_argument("mode", choices=("ping", "flash"))
    parser.add_argument("--file", type=Path)
    parser.add_argument("--skip-ping", action="store_true")
    parser.add_argument("--force-input-len", type=int)
    parser.add_argument("--force-output-len", type=int)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    with HidTransport.open() as transport:
        if args.force_input_len is not None:
            transport.input_len = args.force_input_len
        if args.force_output_len is not None:
            transport.output_len = args.force_output_len

        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)

        updater = FirmwareUpdater(transport)
        if not args.skip_ping:
            version = updater.ping()
            print(f"heartbeat={version}", flush=True)

        if args.mode == "flash":
            if args.file is None:
                raise SystemExit("--file 必填")
            package = load_firmware_package(args.file)
            print(f"firmware={package.path} bytes={package.size}", flush=True)
            updater.flash(package.data, progress=print_progress)
            print("done", flush=True)
    return 0


def print_progress(progress: PacketProgress) -> None:
    if progress.phase == "sending":
        print(f"sending {progress.sent}/{progress.total}", flush=True)
        return
    print(f"{progress.sent}/{progress.total}", flush=True)
