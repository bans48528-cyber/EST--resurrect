from __future__ import annotations

import argparse
import sys
import time
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

    keys = commands.add_parser("keys", help="查看六个按键的状态")
    add_device_options(keys)
    keys.add_argument("--watch", action="store_true", help="持续观察按键变化")
    keys.add_argument("--interval", type=float, default=0.1, help="读取间隔，默认 0.1 秒")
    keys.add_argument("--duration", type=float, help="观察秒数；默认持续到用户中止")

    flash_id = commands.add_parser("flash-id", help="只读检测外部 Flash 型号")
    add_device_options(flash_id)

    flash_scan = commands.add_parser("flash-scan", help="只读检查安全测试扇区")
    add_device_options(flash_scan)

    flash_test = commands.add_parser(
        "flash-test", help="测试 32MB Flash 高地址读写并自动恢复"
    )
    add_device_options(flash_test)

    flash_status = commands.add_parser("flash-status", help="只读查看 Flash 保护状态")
    add_device_options(flash_status)

    flash_mode_probe = commands.add_parser(
        "flash-mode-probe", help="检测写使能与 4 字节模式并立即恢复"
    )
    add_device_options(flash_mode_probe)

    motor_test = commands.add_parser(
        "motor-test", help="让输出 A 口低速正反转一次并自动停止"
    )
    add_device_options(motor_test)

    motor_tacho_test = commands.add_parser(
        "motor-tacho-test", help="测试指定输出口正反转并读取内部测速脉冲"
    )
    add_device_options(motor_tacho_test)
    motor_tacho_test.add_argument(
        "--power", type=int, default=30, help="测试功率百分比，范围 1-100，默认 30"
    )
    motor_tacho_test.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )

    motor_stop_compare = commands.add_parser(
        "motor-stop-compare", help="比较指定输出口的自由滑行和主动刹车状态"
    )
    add_device_options(motor_stop_compare)
    motor_stop_compare.add_argument(
        "--power", type=int, default=60, help="测试功率百分比，范围 1-100，默认 60"
    )
    motor_stop_compare.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )

    motor_dual_test = commands.add_parser(
        "motor-dual-test", help="同时测试 A、B 两个马达的正反转、测速和刹车"
    )
    add_device_options(motor_dual_test)
    motor_dual_test.add_argument(
        "--power", type=int, default=30, help="测试功率百分比，范围 1-100，默认 30"
    )

    motor_control = commands.add_parser(
        "motor-control", help="按端口和正负功率短时运行马达，并读取转动量"
    )
    add_device_options(motor_control)
    motor_control.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )
    motor_control.add_argument(
        "--power", type=int, default=30,
        help="有方向的功率百分比，范围 -100 到 100，负数反转，默认 30",
    )
    motor_control.add_argument(
        "--duration", type=float, default=1.0,
        help="运行秒数，范围 0.1-5.0，默认 1.0",
    )
    motor_control.add_argument(
        "--stop", choices=("coast", "brake"), default="coast",
        help="测试结束时自由滑行或短暂主动刹车，默认 coast",
    )

    motor_pair_control = commands.add_parser(
        "motor-pair-control", help="同时以不同方向和功率控制两个输出口"
    )
    add_device_options(motor_pair_control)
    motor_pair_control.add_argument(
        "--first-port", choices=("A", "B", "C", "D"), default="A",
        help="第一个输出端口，默认 A",
    )
    motor_pair_control.add_argument(
        "--first-power", type=int, default=30,
        help="第一个端口功率，范围 -100 到 100，默认 30",
    )
    motor_pair_control.add_argument(
        "--second-port", choices=("A", "B", "C", "D"), default="D",
        help="第二个输出端口，默认 D",
    )
    motor_pair_control.add_argument(
        "--second-power", type=int, default=-30,
        help="第二个端口功率，范围 -100 到 100，默认 -30",
    )
    motor_pair_control.add_argument(
        "--duration", type=float, default=1.0,
        help="同时运行秒数，范围 0.1-5.0，默认 1.0",
    )
    motor_pair_control.add_argument(
        "--stop", choices=("coast", "brake"), default="coast",
        help="测试结束时自由滑行或短暂主动刹车，默认 coast",
    )

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


def format_pressed_keys(mask: int) -> str:
    names = [f"KEY{index}" for index in range(6) if mask & (1 << index)]
    return ",".join(names) if names else "none"


def run_keys(args: argparse.Namespace) -> int:
    if args.interval <= 0:
        raise ValueError("--interval 必须大于 0")
    if args.duration is not None and args.duration <= 0:
        raise ValueError("--duration 必须大于 0")
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        started = time.monotonic()
        previous_mask = None
        try:
            while True:
                mask = updater.read_key_mask()
                if mask != previous_mask:
                    print(
                        f"pressed={format_pressed_keys(mask)} mask=0x{mask:02X}",
                        flush=True,
                    )
                    previous_mask = mask
                if not args.watch:
                    break
                if args.duration is not None and time.monotonic() - started >= args.duration:
                    break
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("key watch stopped", flush=True)
    return 0


FLASH_MODELS = {
    bytes.fromhex("EF4017"): "W25Q64",
    bytes.fromhex("EF4019"): "W25Q256JV",
}


def describe_flash(jedec_id: bytes) -> tuple[str, str, int | None]:
    manufacturer = "Winbond" if jedec_id[0] == 0xEF else "unknown"
    model = FLASH_MODELS.get(jedec_id, "unknown")
    capacity = 1 << jedec_id[2] if 0x10 <= jedec_id[2] <= 0x1F else None
    return manufacturer, model, capacity


def run_flash_id(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        jedec_id = updater.read_flash_id()
        manufacturer, model, capacity = describe_flash(jedec_id)
        print(f"jedec_id={jedec_id.hex().upper()}", flush=True)
        print(f"manufacturer={manufacturer}", flush=True)
        print(f"model={model}", flush=True)
        print(f"capacity_bytes={capacity if capacity is not None else 'unknown'}", flush=True)
        print(f"model_known={'yes' if model != 'unknown' else 'no'}", flush=True)
    return 0


def print_flash_scan(result: object) -> None:
    print(f"test_address=0x{result.address:08X}", flush=True)
    print(f"device_supported={'yes' if result.supported else 'no'}", flush=True)
    print(f"test_sector_empty={'yes' if result.erased else 'no'}", flush=True)


def run_flash_scan(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        result = updater.scan_flash_test_sector()
        print_flash_scan(result)
    return 0


FLASH_TEST_STATUS = {
    1: "success",
    2: "test-sector-not-empty",
    3: "program-timeout",
    4: "program-verify-failed",
    5: "low-address-alias-changed",
    6: "erase-timeout",
    7: "erase-verify-failed",
    8: "unsupported-device",
    9: "write-enable-failed",
    10: "enter-4byte-mode-failed",
    11: "restore-3byte-mode-failed",
}


def run_flash_test(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        scan = updater.scan_flash_test_sector()
        print_flash_scan(scan)
        if not scan.supported:
            raise EstUpdaterError("Flash 型号不支持此项测试，已停止且未写入")
        if not scan.erased:
            raise EstUpdaterError("测试扇区不是空白，已停止且未写入")
        result = updater.test_flash_4byte_addressing()
        print(f"test_status={FLASH_TEST_STATUS.get(result.status, 'unknown')}", flush=True)
        print(f"restored_empty={'yes' if result.restored else 'no'}", flush=True)
        print(f"test_address=0x{result.address:08X}", flush=True)
        if result.status != 1 or not result.restored:
            raise EstUpdaterError("Flash 高地址读写测试失败；请保留当前状态，不要继续操作")
    return 0


def run_flash_status(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        status = updater.read_flash_status()
        block_protect = (status.status1 >> 2) & 0x07
        print(f"status1=0x{status.status1:02X}", flush=True)
        print(f"status2=0x{status.status2:02X}", flush=True)
        print(f"status3=0x{status.status3:02X}", flush=True)
        print(f"block_protect={block_protect}", flush=True)
        print(f"top_bottom={(status.status1 >> 5) & 1}", flush=True)
        print(f"sector_protect={(status.status1 >> 6) & 1}", flush=True)
        print(f"complement_protect={(status.status2 >> 6) & 1}", flush=True)
        print(f"address_mode_4byte={status.status3 & 1}", flush=True)
    return 0


def run_flash_mode_probe(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        probe = updater.probe_flash_modes()
        print(f"status1_before=0x{probe.status1_before:02X}", flush=True)
        print(f"status1_write_enabled=0x{probe.status1_write_enabled:02X}", flush=True)
        print(f"status1_write_disabled=0x{probe.status1_write_disabled:02X}", flush=True)
        print(f"status3_before=0x{probe.status3_before:02X}", flush=True)
        print(f"status3_four_byte=0x{probe.status3_four_byte:02X}", flush=True)
        print(f"status3_restored=0x{probe.status3_restored:02X}", flush=True)
        print(
            f"write_enable_works={'yes' if probe.status1_write_enabled & 0x02 else 'no'}",
            flush=True,
        )
        mode_entered = bool(probe.status3_four_byte & 0x01)
        mode_restored = not bool(probe.status3_restored & 0x01)
        print(f"four_byte_mode_entered={'yes' if mode_entered else 'no'}", flush=True)
        print(f"three_byte_mode_restored={'yes' if mode_restored else 'no'}", flush=True)
    return 0


MOTOR_TEST_STATES = {
    0: "idle",
    1: "forward",
    2: "pause",
    3: "reverse",
    4: "complete",
}


def run_motor_test(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print("motor_port=A", flush=True)
        print("sequence=forward,pause,reverse,stop", flush=True)
        started = False
        try:
            result = updater.start_motor_test()
            if result.result == 2:
                raise EstUpdaterError("马达测试正在运行，未重复启动")
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了马达测试命令")
            started = True
            previous_state = None
            deadline = time.monotonic() + 4.0
            while time.monotonic() < deadline:
                if result.state != previous_state:
                    print(
                        f"motor_state={MOTOR_TEST_STATES.get(result.state, 'unknown')}",
                        flush=True,
                    )
                    previous_state = result.state
                if result.state == 4:
                    print("motor_test=complete", flush=True)
                    return 0
                time.sleep(0.1)
                result = updater.read_motor_test_status()
            raise EstUpdaterError("马达测试没有按时结束，已发送停止命令")
        finally:
            if started:
                try:
                    updater.stop_motor_test()
                    print("motor_stopped=yes", flush=True)
                except EstUpdaterError:
                    print("motor_stopped=unconfirmed", flush=True)


def run_motor_tacho_test(args: argparse.Namespace) -> int:
    if not 1 <= args.power <= 100:
        raise ValueError("--power 必须在 1 到 100 之间")
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
        protocol_port = None if motor_port == 0 else motor_port
        print(f"motor_port={args.port}", flush=True)
        tacho_pins = {
            0: "PE5,PE6",
            1: "PE13,PE14",
            2: "PC7,PC6",
            3: "PC9,PC8",
        }
        print(f"tacho_pins={tacho_pins[motor_port]}", flush=True)
        print(f"motor_power={args.power}%", flush=True)
        started = False
        try:
            result = updater.start_motor_tacho_test(args.power, protocol_port)
            if result.result == 2:
                raise EstUpdaterError("马达测试正在运行，未重复启动")
            if result.result != 1:
                raise EstUpdaterError(f"设备拒绝了 {args.port} 口测速命令")
            started = True
            previous_state = None
            deadline = time.monotonic() + 4.0
            while time.monotonic() < deadline:
                if result.state != previous_state:
                    print(
                        f"motor_state={MOTOR_TEST_STATES.get(result.state, 'unknown')}",
                        flush=True,
                    )
                    previous_state = result.state
                if result.state == 4:
                    print(f"tacho_total={result.total_count}", flush=True)
                    print(f"tacho_forward={result.forward_count}", flush=True)
                    print(f"tacho_reverse={result.reverse_count}", flush=True)
                    motion_detected = (
                        result.forward_count != 0 and result.reverse_count != 0
                    )
                    directions_opposite = (
                        result.forward_count * result.reverse_count < 0
                    )
                    print(
                        f"motion_detected={'yes' if motion_detected else 'no'}",
                        flush=True,
                    )
                    print(
                        "directions_opposite="
                        f"{'yes' if directions_opposite else 'no'}",
                        flush=True,
                    )
                    if not motion_detected:
                        raise EstUpdaterError(
                            f"马达转动了，但 {args.port} 口没有读到完整测速脉冲"
                        )
                    if not directions_opposite:
                        raise EstUpdaterError("读取到测速脉冲，但正反转计数方向没有相反")
                    print("motor_tacho_test=complete", flush=True)
                    return 0
                time.sleep(0.1)
                result = updater.read_motor_tacho_test_status()
            raise EstUpdaterError(f"{args.port} 口测速没有按时结束，已发送停止命令")
        finally:
            if started:
                try:
                    updater.stop_motor_tacho_test()
                    print("motor_stopped=yes", flush=True)
                except EstUpdaterError:
                    print("motor_stopped=unconfirmed", flush=True)


MOTOR_STOP_TEST_STATES = {
    0: "idle",
    1: "driving",
    2: "measuring-after-stop",
    3: "complete",
}

MOTOR_STOP_MODES = {
    0: "A-coast-low-open-drain",
    1: "B-brake-high-push-pull",
}


def run_one_motor_stop_test(
    updater: FirmwareUpdater,
    stop_mode: int,
    power: int,
    motor_port: int | None,
) -> object:
    result = updater.start_motor_stop_test(stop_mode, power, motor_port)
    if result.result == 2:
        raise EstUpdaterError("马达测试正在运行，未重复启动")
    if result.result != 1:
        raise EstUpdaterError("设备拒绝了停车对比命令")
    previous_state = None
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if result.state != previous_state:
            print(
                f"stop_test_state={MOTOR_STOP_TEST_STATES.get(result.state, 'unknown')}",
                flush=True,
            )
            previous_state = result.state
        if result.state == 3:
            return result
        time.sleep(0.1)
        result = updater.read_motor_stop_test_status()
    raise EstUpdaterError("停车对比测试没有按时结束，已发送停止命令")


def run_motor_stop_compare(args: argparse.Namespace) -> int:
    if not 1 <= args.power <= 100:
        raise ValueError("--power 必须在 1 到 100 之间")
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
        protocol_port = None if motor_port == 0 else motor_port
        print(f"motor_port={args.port}", flush=True)
        print(f"motor_power={args.power}%", flush=True)
        results = []
        try:
            for stop_mode in (0, 1):
                print(f"testing_stop_state={MOTOR_STOP_MODES[stop_mode]}", flush=True)
                result = run_one_motor_stop_test(
                    updater, stop_mode, args.power, protocol_port
                )
                results.append(result)
                print(f"powered_count={result.powered_count}", flush=True)
                print(f"after_stop_count={result.stopped_count}", flush=True)
                updater.stop_motor_stop_test()
                time.sleep(0.5)
        finally:
            try:
                updater.stop_motor_stop_test()
                print("motor_stopped=yes", flush=True)
            except EstUpdaterError:
                print("motor_stopped=unconfirmed", flush=True)

        if any(abs(result.powered_count) < 10 for result in results):
            raise EstUpdaterError("测试时没有读到足够的马达转动，无法比较停车方式")
        stopped = [abs(result.stopped_count) for result in results]
        brake_index = 0 if stopped[0] < stopped[1] else 1
        coast_index = 1 - brake_index
        clear_difference = (
            stopped[coast_index] >= stopped[brake_index] + 5
            and stopped[coast_index] * 4 >= stopped[brake_index] * 5
        )
        print(f"state_A_after_stop={stopped[0]}", flush=True)
        print(f"state_B_after_stop={stopped[1]}", flush=True)
        if not clear_difference:
            print("comparison=inconclusive", flush=True)
            raise EstUpdaterError("两种状态的停车差异不够明显，需要调整测试条件")
        print(f"brake_state={'A' if brake_index == 0 else 'B'}", flush=True)
        print(f"coast_state={'A' if coast_index == 0 else 'B'}", flush=True)
        print("comparison=clear", flush=True)
        print("motor_stop_compare=complete", flush=True)
    return 0


MOTOR_DUAL_TEST_STATES = {
    0: "idle",
    1: "both-forward",
    2: "both-brake",
    3: "both-reverse",
    4: "both-final-brake",
    5: "complete",
}


def motor_count_balance(first: int, second: int) -> int:
    larger = max(abs(first), abs(second))
    if larger == 0:
        return 0
    return round(min(abs(first), abs(second)) * 100 / larger)


def run_motor_dual_test(args: argparse.Namespace) -> int:
    if not 1 <= args.power <= 100:
        raise ValueError("--power 必须在 1 到 100 之间")
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print("motor_ports=A,B", flush=True)
        print(f"motor_power={args.power}%", flush=True)
        started = False
        try:
            result = updater.start_motor_dual_test(args.power)
            if result.result == 2:
                raise EstUpdaterError("其他马达测试正在运行，未启动双马达测试")
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了 A/B 双马达测试命令")
            started = True
            previous_state = None
            deadline = time.monotonic() + 5.0
            while time.monotonic() < deadline:
                if result.state != previous_state:
                    print(
                        "dual_motor_state="
                        f"{MOTOR_DUAL_TEST_STATES.get(result.state, 'unknown')}",
                        flush=True,
                    )
                    previous_state = result.state
                if result.state == 5:
                    counts = (
                        result.a_forward_count,
                        result.b_forward_count,
                        result.a_reverse_count,
                        result.b_reverse_count,
                    )
                    print(f"a_forward={counts[0]}", flush=True)
                    print(f"b_forward={counts[1]}", flush=True)
                    print(f"a_reverse={counts[2]}", flush=True)
                    print(f"b_reverse={counts[3]}", flush=True)
                    both_detected = all(abs(count) >= 10 for count in counts)
                    directions_ok = (
                        counts[0] * counts[2] < 0
                        and counts[1] * counts[3] < 0
                    )
                    print(
                        f"both_motors_detected={'yes' if both_detected else 'no'}",
                        flush=True,
                    )
                    print(
                        f"both_directions_ok={'yes' if directions_ok else 'no'}",
                        flush=True,
                    )
                    print(
                        "forward_balance_percent="
                        f"{motor_count_balance(counts[0], counts[1])}",
                        flush=True,
                    )
                    print(
                        "reverse_balance_percent="
                        f"{motor_count_balance(counts[2], counts[3])}",
                        flush=True,
                    )
                    if not both_detected:
                        raise EstUpdaterError("A、B 中至少一个马达没有完整测速反馈")
                    if not directions_ok:
                        raise EstUpdaterError("A、B 中至少一个马达的正反转方向异常")
                    print("motor_dual_test=complete", flush=True)
                    return 0
                time.sleep(0.1)
                result = updater.read_motor_dual_test_status()
            raise EstUpdaterError("A/B 双马达测试没有按时结束，已发送停止命令")
        finally:
            if started:
                try:
                    updater.stop_motor_dual_test()
                    print("motors_stopped=yes", flush=True)
                except EstUpdaterError:
                    print("motors_stopped=unconfirmed", flush=True)


MOTOR_OUTPUT_STATES = {
    0: "coast",
    1: "drive",
    2: "brake",
}


def require_motor_control_success(result: object, operation: str) -> None:
    if result.result == 2:
        raise EstUpdaterError("旧的马达诊断测试正在运行，通用控制暂时忙碌")
    if result.result != 1:
        raise EstUpdaterError(f"设备拒绝了通用马达控制操作：{operation}")


def run_motor_control(args: argparse.Namespace) -> int:
    if not -100 <= args.power <= 100 or args.power == 0:
        raise ValueError("--power 必须在 -100 到 100 之间且不能为 0")
    if not 0.1 <= args.duration <= 5.0:
        raise ValueError("--duration 必须在 0.1 到 5.0 秒之间")

    motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_port={args.port}", flush=True)
        print(f"commanded_power={args.power}%", flush=True)
        print(f"duration_seconds={args.duration:g}", flush=True)
        started = False
        try:
            result = updater.reset_motor_tacho(motor_port)
            require_motor_control_success(result, "reset-tacho")
            result = updater.set_motor_power(motor_port, args.power)
            require_motor_control_success(result, "set-power")
            started = True
            print(
                f"running_state={MOTOR_OUTPUT_STATES.get(result.output_state, 'unknown')}",
                flush=True,
            )
            time.sleep(args.duration)
            if args.stop == "brake":
                stopped = updater.brake_motor(motor_port)
            else:
                stopped = updater.coast_motor(motor_port)
            require_motor_control_success(stopped, args.stop)
            print(f"reported_power={result.power_percent}%", flush=True)
            print(f"tacho_count={stopped.tacho_count}", flush=True)
            print(f"requested_stop={args.stop}", flush=True)
            print(
                f"stop_state={MOTOR_OUTPUT_STATES.get(stopped.output_state, 'unknown')}",
                flush=True,
            )
            if args.stop == "brake":
                time.sleep(0.3)
            print("motor_control=complete", flush=True)
            return 0
        finally:
            if started:
                try:
                    safe = updater.coast_motor(motor_port)
                    require_motor_control_success(safe, "final-coast")
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


def run_motor_pair_control(args: argparse.Namespace) -> int:
    powers = (args.first_power, args.second_power)
    if any(power == 0 or not -100 <= power <= 100 for power in powers):
        raise ValueError("两个功率都必须在 -100 到 100 之间且不能为 0")
    if args.first_port == args.second_port:
        raise ValueError("两个端口不能相同")
    if not 0.1 <= args.duration <= 5.0:
        raise ValueError("--duration 必须在 0.1 到 5.0 秒之间")

    port_numbers = {"A": 0, "B": 1, "C": 2, "D": 3}
    motors = (
        (args.first_port, port_numbers[args.first_port], args.first_power),
        (args.second_port, port_numbers[args.second_port], args.second_power),
    )
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_ports={args.first_port},{args.second_port}", flush=True)
        print(f"duration_seconds={args.duration:g}", flush=True)
        started_ports: list[tuple[str, int]] = []
        try:
            for name, port, _power in motors:
                reset = updater.reset_motor_tacho(port)
                require_motor_control_success(reset, f"{name}-reset-tacho")
            for name, port, power in motors:
                result = updater.set_motor_power(port, power)
                require_motor_control_success(result, f"{name}-set-power")
                started_ports.append((name, port))
                print(f"{name}_commanded_power={power}%", flush=True)
                print(
                    f"{name}_running_state="
                    f"{MOTOR_OUTPUT_STATES.get(result.output_state, 'unknown')}",
                    flush=True,
                )
            time.sleep(args.duration)
            for name, port, power in motors:
                if args.stop == "brake":
                    stopped = updater.brake_motor(port)
                else:
                    stopped = updater.coast_motor(port)
                require_motor_control_success(stopped, f"{name}-{args.stop}")
                print(f"{name}_reported_power={power}%", flush=True)
                print(f"{name}_tacho_count={stopped.tacho_count}", flush=True)
                print(
                    f"{name}_stop_state="
                    f"{MOTOR_OUTPUT_STATES.get(stopped.output_state, 'unknown')}",
                    flush=True,
                )
            if args.stop == "brake":
                time.sleep(0.3)
            print("motor_pair_control=complete", flush=True)
            return 0
        finally:
            for name, port in started_ports:
                try:
                    safe = updater.coast_motor(port)
                    require_motor_control_success(safe, f"{name}-final-coast")
                    print(f"{name}_safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print(f"{name}_safe_final_state=unconfirmed", flush=True)


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
        if args.mode == "keys":
            return run_keys(args)
        if args.mode == "flash-id":
            return run_flash_id(args)
        if args.mode == "flash-scan":
            return run_flash_scan(args)
        if args.mode == "flash-test":
            return run_flash_test(args)
        if args.mode == "flash-status":
            return run_flash_status(args)
        if args.mode == "flash-mode-probe":
            return run_flash_mode_probe(args)
        if args.mode == "motor-test":
            return run_motor_test(args)
        if args.mode == "motor-tacho-test":
            return run_motor_tacho_test(args)
        if args.mode == "motor-stop-compare":
            return run_motor_stop_compare(args)
        if args.mode == "motor-dual-test":
            return run_motor_dual_test(args)
        if args.mode == "motor-control":
            return run_motor_control(args)
        if args.mode == "motor-pair-control":
            return run_motor_pair_control(args)
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
