from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .errors import EstUpdaterError, VersionSafetyError
from .firmware import FirmwarePackage, compare_versions, load_firmware_package
from .hid_transport import HidTransport
from .logging_utils import UpgradeLog
from .updater import FirmwareUpdater, PacketProgress


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="EST USB HID firmware update tool")
    commands = parser.add_subparsers(dest="mode", required=True)

    ping = commands.add_parser("ping", help="读取当前设备版本")
    add_device_options(ping)

    info = commands.add_parser("info", help="显示升级包信息，不连接设备")
    add_package_options(info)

    verify = commands.add_parser("verify", help="严格校验升级包及 manifest，不连接设备")
    add_package_options(verify)

    flash = commands.add_parser("flash", help="校验并发送升级包")
    add_package_options(flash)
    add_device_options(flash)
    flash.add_argument("--skip-ping", action="store_true", help="跳过当前版本读取")
    flash.add_argument(
        "--allow-missing-manifest",
        action="store_true",
        help="允许无 manifest 刷写（不推荐，无法显示目标版本）",
    )
    flash.add_argument(
        "--force",
        action="store_true",
        help="允许同版本重刷或降级",
    )
    flash.add_argument("--log-dir", type=Path, help="自定义升级日志目录")
    flash.add_argument("--no-log", action="store_true", help="禁用升级日志")
    return parser.parse_args(argv)


def add_package_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--file", required=True, type=Path, help=".upgrade.bin 升级包")
    parser.add_argument("--manifest", type=Path, help="manifest 路径；默认自动查找同名文件")


def add_device_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--force-input-len", type=int)
    parser.add_argument("--force-output-len", type=int)


def print_package_info(package: FirmwarePackage) -> None:
    print(f"firmware={package.path}", flush=True)
    print(f"header={package.header.decode('ascii')}", flush=True)
    print(f"bytes={package.size}", flush=True)
    print(f"frames={package.total_frames}", flush=True)
    print(f"sha256={package.sha256}", flush=True)
    if package.manifest is None:
        print("manifest=not-found", flush=True)
        print("target_version=unknown", flush=True)
        return
    print(f"manifest={package.manifest.path}", flush=True)
    print("manifest_status=verified", flush=True)
    print(f"target_version={package.manifest.version}", flush=True)


def open_transport(args: argparse.Namespace) -> HidTransport:
    transport = HidTransport.open()
    if args.force_input_len is not None:
        transport.input_len = args.force_input_len
    if args.force_output_len is not None:
        transport.output_len = args.force_output_len
    return transport


def run_ping(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        version = FirmwareUpdater(transport).ping()
        print(f"heartbeat={version}", flush=True)
    return 0


def run_package_check(args: argparse.Namespace, *, require_manifest: bool) -> int:
    package = load_firmware_package(
        args.file,
        args.manifest,
        require_manifest=require_manifest,
    )
    print_package_info(package)
    print("verified" if require_manifest else "info complete", flush=True)
    return 0


def enforce_version_safety(current: str, target: str | None, *, force: bool) -> None:
    if force or target is None:
        return
    comparison = compare_versions(current, target)
    if current == target or comparison == 0:
        raise VersionSafetyError(
            f"当前版本与目标版本相同（{current}）；如确认需要重刷，请加 --force"
        )
    if comparison == -1:
        raise VersionSafetyError(
            f"目标版本 {target} 低于当前版本 {current}；如确认需要降级，请加 --force"
        )
    if comparison is None:
        raise VersionSafetyError(
            f"无法自动比较当前版本 {current} 与目标版本 {target}；确认版本链路后请加 --force"
        )


def run_flash(args: argparse.Namespace) -> int:
    upgrade_log = None if args.no_log else UpgradeLog(args.log_dir)
    if upgrade_log is not None:
        print(f"log={upgrade_log.path}", flush=True)
        upgrade_log.info("command=flash")
    try:
        package = load_firmware_package(
            args.file,
            args.manifest,
            require_manifest=not args.allow_missing_manifest,
        )
        print_package_info(package)
        if upgrade_log is not None:
            upgrade_log.info(
                "firmware=%s bytes=%d frames=%d header=%s sha256=%s manifest=%s target_version=%s",
                package.path,
                package.size,
                package.total_frames,
                package.header.decode("ascii"),
                package.sha256,
                package.manifest.path if package.manifest else "not-found",
                package.target_version or "unknown",
            )

        with open_transport(args) as transport:
            print(f"device={transport.path}", flush=True)
            print(
                f"report input={transport.input_len} output={transport.output_len}",
                flush=True,
            )
            updater = FirmwareUpdater(transport)
            current_version = None
            if args.skip_ping:
                print("current_version=skipped", flush=True)
            else:
                current_version = updater.ping()
                print(f"current_version={current_version}", flush=True)
                enforce_version_safety(
                    current_version,
                    package.target_version,
                    force=args.force,
                )
            print(f"target_version={package.target_version or 'unknown'}", flush=True)
            if upgrade_log is not None:
                upgrade_log.info(
                    "device=%s input_report=%d output_report=%d current_version=%s force=%s",
                    transport.path,
                    transport.input_len,
                    transport.output_len,
                    current_version or "skipped",
                    args.force,
                )

            def report_progress(progress: PacketProgress) -> None:
                print_progress(progress)
                if upgrade_log is not None:
                    upgrade_log.info(
                        "packet=%d total=%d phase=%s",
                        progress.sent,
                        progress.total,
                        progress.phase,
                    )

            updater.flash(package.data, progress=report_progress)
            print("done", flush=True)
            if upgrade_log is not None:
                upgrade_log.success()
        return 0
    except BaseException as exc:
        if upgrade_log is not None:
            upgrade_log.failure(exc)
        raise
    finally:
        if upgrade_log is not None:
            upgrade_log.close()


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.mode == "ping":
            return run_ping(args)
        if args.mode == "info":
            return run_package_check(args, require_manifest=False)
        if args.mode == "verify":
            return run_package_check(args, require_manifest=True)
        return run_flash(args)
    except EstUpdaterError as exc:
        print(f"error[{exc.code}]: {exc}", file=sys.stderr, flush=True)
        return 2
    except OSError as exc:
        print(f"error[io]: {exc}", file=sys.stderr, flush=True)
        return 3
    except KeyboardInterrupt:
        print("error[cancelled]: 用户中止操作", file=sys.stderr, flush=True)
        return 130


def print_progress(progress: PacketProgress) -> None:
    if progress.phase == "sending":
        print(f"sending {progress.sent}/{progress.total}", flush=True)
        return
    print(f"{progress.sent}/{progress.total}", flush=True)
