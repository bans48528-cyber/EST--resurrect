from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from .constants import (
    DEVICE_CAPABILITY_BATTERY,
    DEVICE_CAPABILITY_DRIVE_STRAIGHT,
    DEVICE_CAPABILITY_DRIVE_RUN,
    DEVICE_CAPABILITY_INPUT_SENSOR,
    DEVICE_CAPABILITY_KEYS,
    DEVICE_CAPABILITY_MOTOR_CONTROL,
    DEVICE_CAPABILITY_MOTOR_PAIR_POSITION,
    DEVICE_CAPABILITY_MOTOR_PAIR_SPEED,
    DEVICE_CAPABILITY_MOTOR_POSITION,
    DEVICE_CAPABILITY_MOTOR_TACHO,
    DEVICE_CAPABILITY_MOTOR_TYPE,
    DEVICE_CAPABILITY_UPDATE,
)
from .errors import EstUpdaterError, VersionSafetyError
from .firmware import FirmwarePackage, compare_versions, load_firmware_package
from .hid_transport import HidTransport
from .logging_utils import UpgradeLog
from .updater import FirmwareUpdater, PacketProgress


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="EST USB HID device and firmware tool")
    commands = parser.add_subparsers(dest="mode", required=True)

    ping = commands.add_parser("ping", help="读取当前设备版本")
    add_device_options(ping)

    device_status = commands.add_parser(
        "device-status", help="一次读取版本、电量、按键、四个马达和四个输入口"
    )
    add_device_options(device_status)

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

    motor_types = commands.add_parser(
        "motor-types", help="只读识别 A-D 输出口的大型或中型电机"
    )
    add_device_options(motor_types)

    motor_identify = commands.add_parser(
        "motor-identify", help="不转动马达，刷新指定输出口的大型或中型识别"
    )
    add_device_options(motor_identify)
    motor_identify.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )

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

    motor_position = commands.add_parser(
        "motor-position", help="按已识别的大型或中型电机参数运行指定角度或圈数"
    )
    add_device_options(motor_position)
    motor_position.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )
    motor_position.add_argument(
        "--speed", type=int, default=30,
        help="目标转速百分比，范围 10-100，默认 30；短行程会自动限速",
    )
    position_target = motor_position.add_mutually_exclusive_group(required=True)
    position_target.add_argument(
        "--degrees", type=int, help="目标角度，范围 -3600 到 3600，负数反转"
    )
    position_target.add_argument(
        "--rotations", type=float, help="目标圈数，范围 -10 到 10，负数反转"
    )

    motor_speed = commands.add_parser(
        "motor-speed", help="按已识别的马达类型持续闭环维持目标转速"
    )
    add_device_options(motor_speed)
    motor_speed.add_argument(
        "--port", choices=("A", "B", "C", "D"), default="A", help="输出端口，默认 A"
    )
    motor_speed.add_argument(
        "--speed", type=int, default=30,
        help="有方向的目标转速百分比，范围 -100 到 100，绝对值至少 10，默认 30",
    )
    motor_speed.add_argument(
        "--duration", type=float, default=5.0,
        help="测试秒数，范围 0.5-30，默认 5",
    )
    motor_speed.add_argument(
        "--stop", choices=("coast", "brake"), default="coast",
        help="测试结束时自由滑行或短暂主动刹车，默认 coast",
    )

    motor_pair_position = commands.add_parser(
        "motor-pair-position", help="两路马达以编码器进度差修正同步运行"
    )
    add_device_options(motor_pair_position)
    motor_pair_position.add_argument(
        "--left-port", choices=("A", "B", "C", "D"), default="A",
        help="左侧输出端口，默认 A",
    )
    motor_pair_position.add_argument(
        "--right-port", choices=("A", "B", "C", "D"), default="C",
        help="右侧输出端口，默认 C",
    )
    motor_pair_position.add_argument(
        "--left-degrees", type=int, default=360,
        help="左侧目标角度，默认 360",
    )
    motor_pair_position.add_argument(
        "--right-degrees", type=int, default=360,
        help="右侧目标角度，默认 360；首版要求绝对值与左侧相同",
    )
    motor_pair_position.add_argument(
        "--speed", type=int, default=20,
        help="最大目标速度，范围 10-100，默认 20",
    )

    motor_pair_speed = commands.add_parser(
        "motor-pair-speed", help="持续同步维持两路闭环转速，直到显式停止"
    )
    add_device_options(motor_pair_speed)
    motor_pair_speed.add_argument(
        "--left-port", choices=("A", "B", "C", "D"), default="A",
        help="左侧输出端口，默认 A",
    )
    motor_pair_speed.add_argument(
        "--right-port", choices=("A", "B", "C", "D"), default="C",
        help="右侧输出端口，默认 C",
    )
    motor_pair_speed.add_argument(
        "--left-speed", type=int, default=20,
        help="左侧有方向目标速度，默认 20",
    )
    motor_pair_speed.add_argument(
        "--right-speed", type=int, default=20,
        help="右侧有方向目标速度，默认 20；绝对值须与左侧相同",
    )
    motor_pair_speed.add_argument(
        "--duration", type=float, default=5.0,
        help="电脑端观察秒数，范围 0.5-60，默认 5；固件没有运行时限",
    )
    motor_pair_speed.add_argument(
        "--stop", choices=("coast", "brake"), default="coast",
        help="观察结束时显式自由滑行或刹车，默认 coast",
    )

    drive_straight = commands.add_parser(
        "drive-straight", help="按轮径把毫米距离换算为双电机同步直行"
    )
    add_device_options(drive_straight)
    drive_straight.add_argument(
        "--left-port", choices=("A", "B", "C", "D"), default="A",
        help="左轮输出端口，默认 A",
    )
    drive_straight.add_argument(
        "--right-port", choices=("A", "B", "C", "D"), default="C",
        help="右轮输出端口，默认 C",
    )
    drive_straight.add_argument(
        "--wheel-diameter", type=int, default=56,
        help="轮胎直径，单位毫米，默认 56",
    )
    drive_straight.add_argument(
        "--axle-track", type=int, default=120,
        help="左右轮中心距，单位毫米，默认 120；首版直行暂不参与换算",
    )
    drive_straight.add_argument(
        "--distance", type=int, default=500,
        help="直行距离，单位毫米，负数后退，默认 500",
    )
    drive_straight.add_argument(
        "--speed", type=int, default=40,
        help="最大目标速度，范围 10-100，默认 40",
    )

    drive_run = commands.add_parser(
        "drive-run", help="按圈数、角度或秒数控制双电机直行"
    )
    add_device_options(drive_run)
    drive_run.add_argument(
        "--left-port", choices=("A", "B", "C", "D"), default="A",
        help="左轮输出端口，默认 A",
    )
    drive_run.add_argument(
        "--right-port", choices=("A", "B", "C", "D"), default="C",
        help="右轮输出端口，默认 C",
    )
    drive_target = drive_run.add_mutually_exclusive_group(required=True)
    drive_target.add_argument(
        "--rotations", type=float, help="有方向的轮子圈数，负数后退"
    )
    drive_target.add_argument(
        "--degrees", type=int, help="有方向的轮子角度，负数后退"
    )
    drive_target.add_argument(
        "--seconds", type=float, help="有方向的运行秒数，负数后退"
    )
    drive_run.add_argument(
        "--speed", type=int, default=40,
        help="目标速度，范围 10-100，默认 40",
    )
    drive_run.add_argument(
        "--stop", choices=("coast", "brake"), default="coast",
        help="按秒模式到时自由滑行或主动刹车；角度/圈数当前仅支持 coast",
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

    sensor_read = commands.add_parser(
        "sensor-read", help="读取 1-4 号输入口传感器"
    )
    add_device_options(sensor_read)
    sensor_read.add_argument(
        "--port", choices=("1", "2", "3", "4"), default="1",
		help="输入端口，默认 1；M0.46A 起支持 1-4 号口",
    )
    sensor_read.add_argument(
        "--mode", dest="sensor_mode",
        choices=(
            "reflect", "ambient", "color", "cm", "inch", "presence",
            "celsius", "fahrenheit", "angle", "rate",
            "sound", "db", "proximity", "beacon", "remote",
        ),
        default="reflect",
        help=(
            "颜色传感器模式、超声波 cm/inch/presence，或温度传感器 "
            "celsius/fahrenheit；陀螺仪 angle/rate；声音 sound/db；"
            "红外 proximity/beacon/remote；默认 reflect"
        ),
    )
    sensor_read.add_argument("--watch", action="store_true", help="持续读取传感器")
    sensor_read.add_argument(
        "--interval", type=float, default=0.2, help="读取间隔，默认 0.2 秒"
    )
    sensor_read.add_argument("--duration", type=float, help="持续读取秒数")

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


SENSOR_STATES = {
    0: "off",
    1: "syncing",
    2: "streaming",
    3: "stale",
}
SENSOR_MODES = {
    "reflect": 0,
    "ambient": 1,
    "color": 2,
    "cm": 0,
    "inch": 1,
    "presence": 2,
    "celsius": 0,
    "fahrenheit": 1,
    "angle": 0,
    "rate": 1,
    "sound": 0,
    "db": 0,
    "proximity": 0,
    "beacon": 1,
    "remote": 2,
}
SENSOR_MODE_NAMES = {0: "reflect", 1: "ambient", 2: "color"}
ULTRASONIC_MODE_NAMES = {0: "cm", 1: "inch", 2: "presence"}
TEMPERATURE_MODE_NAMES = {0: "celsius", 1: "fahrenheit"}
GYRO_MODE_NAMES = {0: "angle", 1: "rate"}
SOUND_MODE_NAMES = {0: "db"}
INFRARED_MODE_NAMES = {0: "proximity", 1: "beacon", 2: "remote"}
SENSOR_MODEL_NAMES = {
    0x03: "NXT-sound",
    0x06: "EST-temperature",
    0x10: "EST/EV3-touch",
    0x1D: "EST/EV3-color",
    0x1E: "EST/EV3-ultrasonic",
    0x20: "EST/EV3-gyro",
    0x21: "EST/EV3-infrared",
}
SENSOR_COLOR_NAMES = {
    0: "none",
    1: "black",
    2: "blue",
    3: "green",
    4: "yellow",
    5: "red",
    6: "white",
    7: "brown",
}
MOTOR_OUTPUT_NAMES = {0: "coast", 1: "drive", 2: "brake"}
DEVICE_CAPABILITY_NAMES = (
    (DEVICE_CAPABILITY_UPDATE, "firmware-update"),
    (DEVICE_CAPABILITY_MOTOR_CONTROL, "motor-control"),
    (DEVICE_CAPABILITY_MOTOR_TACHO, "motor-tacho"),
    (DEVICE_CAPABILITY_INPUT_SENSOR, "input-sensor"),
    (DEVICE_CAPABILITY_BATTERY, "battery"),
    (DEVICE_CAPABILITY_KEYS, "keys"),
    (DEVICE_CAPABILITY_MOTOR_TYPE, "motor-type"),
    (DEVICE_CAPABILITY_MOTOR_POSITION, "motor-position"),
    (DEVICE_CAPABILITY_MOTOR_PAIR_POSITION, "motor-pair-position"),
    (DEVICE_CAPABILITY_MOTOR_PAIR_SPEED, "motor-pair-speed"),
    (DEVICE_CAPABILITY_DRIVE_STRAIGHT, "drive-straight"),
    (DEVICE_CAPABILITY_DRIVE_RUN, "drive-run"),
)


def device_sensor_mode_name(sensor_type: int, mode: int) -> str:
    if sensor_type == 0x03:
        names = SOUND_MODE_NAMES
    elif sensor_type == 0x1E:
        names = ULTRASONIC_MODE_NAMES
    elif sensor_type == 0x06:
        names = TEMPERATURE_MODE_NAMES
    elif sensor_type == 0x20:
        names = GYRO_MODE_NAMES
    elif sensor_type == 0x21:
        names = INFRARED_MODE_NAMES
    else:
        names = SENSOR_MODE_NAMES
    return names.get(mode, "unknown")


def device_sensor_value_text(sensor: object) -> str:
    if not sensor.value_valid:
        return "unavailable"
    if sensor.sensor_type == 0x10:
        return "down" if sensor.value else "up"
    if sensor.sensor_type == 0x03:
        return f"{sensor.value}dB"
    if sensor.sensor_type == 0x1D and sensor.mode == 2:
        return SENSOR_COLOR_NAMES.get(sensor.value, "unknown")
    if sensor.sensor_type == 0x1E:
        if sensor.mode == 0:
            return f"{sensor.value / 10:.1f}cm"
        if sensor.mode == 1:
            return f"{sensor.value / 10:.1f}inch"
        return "yes" if sensor.value else "no"
    if sensor.sensor_type == 0x06:
        signed = sensor.value if sensor.value < 0x8000 else sensor.value - 0x10000
        unit = "F" if sensor.mode == 1 else "C"
        return f"{signed / 10:.1f}{unit}"
    if sensor.sensor_type == 0x20:
        signed = sensor.value if sensor.value < 0x8000 else sensor.value - 0x10000
        return str(signed)
    if sensor.sensor_type == 0x21:
        if sensor.mode == 1:
            heading_byte = sensor.value & 0xFF
            heading = heading_byte if heading_byte <= 180 else -(255 - heading_byte)
            distance = (sensor.value >> 8) & 0xFF
            if distance == 128:
                return "beacon unavailable"
            return f"heading {heading}, distance {distance}"
        return str(sensor.value & 0xFF)
    return str(sensor.value)


def run_device_status(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        status = FirmwareUpdater(transport).read_device_status()
        capabilities = ",".join(
            name for bit, name in DEVICE_CAPABILITY_NAMES if status.capabilities & bit
        )
        print(f"firmware={status.firmware_version}", flush=True)
        print(f"protocol={status.protocol_major}.{status.protocol_minor}", flush=True)
        print(f"capabilities={capabilities or 'none'}", flush=True)
        print(f"uptime_ms={status.uptime_ms}", flush=True)
        print(f"pressed={format_pressed_keys(status.key_mask)}", flush=True)
        print(f"battery_level={status.battery_level}/4", flush=True)
        print(f"battery_percent={min(status.battery_level, 4) * 25}", flush=True)
        print(f"battery_adc_raw={status.battery_adc_raw}", flush=True)
        print(f"battery_sample_mv={status.battery_sample_mv}", flush=True)
        for index, motor in enumerate(status.motors[: status.motor_port_count]):
            port = chr(ord("A") + index)
            print(
                f"motor_{port}=state:{MOTOR_OUTPUT_NAMES.get(motor.output_state, 'unknown')} "
                f"power:{motor.power_percent} tacho:{motor.tacho_count}",
                flush=True,
            )
        for index, sensor in enumerate(status.sensors[: status.sensor_port_count]):
            print(
                f"input_{index + 1}=state:{SENSOR_STATES.get(sensor.state, 'unknown')} "
                f"model:{SENSOR_MODEL_NAMES.get(sensor.sensor_type, 'none')} "
                f"mode:{device_sensor_mode_name(sensor.sensor_type, sensor.mode)} "
                f"value:{device_sensor_value_text(sensor)}",
                flush=True,
            )
    return 0


def require_sensor_success(result: object, operation: str) -> None:
    if result.result == 2:
        raise EstUpdaterError(f"传感器尚未完成连接，暂时不能执行 {operation}")
    if result.result != 1:
        raise EstUpdaterError(f"当前固件不支持该输入口或参数：{operation}")


def print_sensor_sample(result: object) -> None:
    print(f"sensor_state={SENSOR_STATES.get(result.state, 'unknown')}", flush=True)
    print(f"sensor_type=0x{result.sensor_type:02X}", flush=True)
    print(f"sensor_model={SENSOR_MODEL_NAMES.get(result.sensor_type, 'unknown')}", flush=True)
    if result.sensor_type == 0x03:
        mode_names = SOUND_MODE_NAMES
    elif result.sensor_type == 0x1E:
        mode_names = ULTRASONIC_MODE_NAMES
    elif result.sensor_type == 0x06:
        mode_names = TEMPERATURE_MODE_NAMES
    elif result.sensor_type == 0x20:
        mode_names = GYRO_MODE_NAMES
    elif result.sensor_type == 0x21:
        mode_names = INFRARED_MODE_NAMES
    else:
        mode_names = SENSOR_MODE_NAMES
    print(f"sensor_mode={mode_names.get(result.mode, 'unknown')}", flush=True)
    print(f"value_valid={'yes' if result.value_valid else 'no'}", flush=True)
    if result.value_valid and result.sensor_type == 0x20:
        sensor_value = (
            result.value if result.value < 0x8000 else result.value - 0x10000
        )
    else:
        sensor_value = result.value
    print(
        f"sensor_value={sensor_value if result.value_valid else 'unavailable'}",
        flush=True,
    )
    if result.value_valid and result.sensor_type == 0x1D and result.mode == 2:
        print(f"color_name={SENSOR_COLOR_NAMES.get(result.value, 'unknown')}", flush=True)
    if result.value_valid and result.sensor_type == 0x1E:
        if result.mode == 0:
            print(f"distance_cm={result.value / 10:.1f}", flush=True)
        elif result.mode == 1:
            print(f"distance_inch={result.value / 10:.1f}", flush=True)
        elif result.mode == 2:
            print(f"object_present={'yes' if result.value else 'no'}", flush=True)
    if result.value_valid and result.sensor_type == 0x06:
        signed_value = result.value if result.value < 0x8000 else result.value - 0x10000
        if result.mode == 1:
            print(f"temperature_f={signed_value / 10:.1f}", flush=True)
        else:
            print(f"temperature_c={signed_value / 10:.1f}", flush=True)
    if result.value_valid and result.sensor_type == 0x20:
        signed_value = result.value if result.value < 0x8000 else result.value - 0x10000
        label = "gyro_rate" if result.mode == 1 else "gyro_angle"
        print(f"{label}={signed_value}", flush=True)
    if result.value_valid and result.sensor_type == 0x03:
        print(f"sound_level_db={result.value}", flush=True)
    if result.value_valid and result.sensor_type == 0x21:
        if result.mode == 0:
            print(f"infrared_proximity={result.value & 0xFF}", flush=True)
        elif result.mode == 1:
            heading_byte = result.value & 0xFF
            heading = heading_byte if heading_byte <= 180 else -(255 - heading_byte)
            distance = (result.value >> 8) & 0xFF
            print(f"beacon_heading={heading}", flush=True)
            print(
                f"beacon_distance={distance if distance != 128 else 'unavailable'}",
                flush=True,
            )
        else:
            print(f"remote_code={result.value & 0xFF}", flush=True)
    print(f"adc0_raw={result.adc0_raw}", flush=True)
    print(f"adc1_raw={result.adc1_raw}", flush=True)
    print(f"digital_mask=0x{result.digital_mask:02X}", flush=True)
    print(f"uart_rx_bytes={result.rx_count}", flush=True)
    print(f"checksum_errors={result.checksum_errors}", flush=True)


def run_sensor_read(args: argparse.Namespace) -> int:
    if args.interval <= 0:
        raise ValueError("--interval 必须大于 0")
    if args.duration is not None and args.duration <= 0:
        raise ValueError("--duration 必须大于 0")
    sensor_port = int(args.port) - 1
    requested_mode = SENSOR_MODES[args.sensor_mode]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"input_port={args.port}", flush=True)

        status = updater.read_input_sensor(sensor_port)
        require_sensor_success(status, "status")
        sync_started = time.monotonic()
        while status.state != 2 and time.monotonic() - sync_started < 4.0:
            time.sleep(0.1)
            status = updater.read_input_sensor(sensor_port)
            require_sensor_success(status, "status")
        if status.state != 2:
            print_sensor_sample(status)
            raise EstUpdaterError("在 4 秒内没有识别到输入传感器")

        selected = updater.set_input_sensor_mode(sensor_port, requested_mode)
        require_sensor_success(selected, "set-mode")
        value_started = time.monotonic()
        while (
            (not selected.value_valid or selected.mode != requested_mode)
            and time.monotonic() - value_started < 2.0
        ):
            time.sleep(0.05)
            selected = updater.read_input_sensor(sensor_port)
            require_sensor_success(selected, "status")

        started = time.monotonic()
        while True:
            print_sensor_sample(selected)
            if not args.watch:
                break
            if args.duration is not None and time.monotonic() - started >= args.duration:
                break
            time.sleep(args.interval)
            selected = updater.read_input_sensor(sensor_port)
            require_sensor_success(selected, "status")
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

MOTOR_TYPE_NAMES = {
    0: "none",
    4: "large",
    5: "medium",
    0xFF: "unknown",
}


def run_motor_types(args: argparse.Namespace) -> int:
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        result = updater.read_motor_types()
        if result.result != 1:
            raise EstUpdaterError("设备未能读取马达类型")
        for index, motor in enumerate(result.motors):
            port = chr(ord("A") + index)
            print(
                f"motor_{port}=type:{MOTOR_TYPE_NAMES.get(motor.motor_type, 'unknown')} "
                f"id_mv:{motor.millivolts} adc_raw:{motor.adc_raw} "
                f"pin6_low_mv:{motor.pin6_low_millivolts} "
                f"pin5_pullup_mv:{motor.pin5_pullup_millivolts} "
                f"pin5_pullup_high:{int(motor.pin5_pullup_high)}",
                flush=True,
            )
    return 0


def run_motor_identify(args: argparse.Namespace) -> int:
    motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        print(f"report input={transport.input_len} output={transport.output_len}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_port={args.port}", flush=True)
        print("identification_motion=none", flush=True)
        result = updater.refresh_motor_type(motor_port)
        if result.result == 2:
            raise EstUpdaterError("该端口正在运行或其他马达任务尚未结束，不能刷新类型")
        if result.result != 1:
            raise EstUpdaterError("设备拒绝了马达静止识别命令")
        deadline = time.monotonic() + 2.0
        motor = result.motors[motor_port]
        while motor.motor_type == 0xFF and time.monotonic() < deadline:
            time.sleep(0.05)
            result = updater.read_motor_types()
            motor = result.motors[motor_port]
        print(f"motor_type={MOTOR_TYPE_NAMES.get(motor.motor_type, 'unknown')}", flush=True)
        print(f"id_mv={motor.millivolts}", flush=True)
        print(f"adc_raw={motor.adc_raw}", flush=True)
        print(f"pin6_low_mv={motor.pin6_low_millivolts}", flush=True)
        print(f"pin6_low_adc_raw={motor.pin6_low_adc_raw}", flush=True)
        print(f"pin5_pullup_mv={motor.pin5_pullup_millivolts}", flush=True)
        print(f"pin5_pullup_adc_raw={motor.pin5_pullup_adc_raw}", flush=True)
        print(f"pin5_pullup_high={int(motor.pin5_pullup_high)}", flush=True)
        if motor.motor_type == 0xFF:
            raise EstUpdaterError("静止刷新后仍未得到有效马达类型，请检查插头和线缆")
        print("motor_identify=complete", flush=True)
    return 0


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


MOTOR_POSITION_STATES = {
    0: "idle",
    1: "running",
    2: "complete",
    3: "timeout",
}


def run_motor_position(args: argparse.Namespace) -> int:
    if not 10 <= args.speed <= 100:
        raise ValueError("--speed 必须在 10 到 100 之间")
    if args.rotations is not None:
        if args.rotations == 0 or not -10 <= args.rotations <= 10:
            raise ValueError("--rotations 必须在 -10 到 10 之间且不能为 0")
        degrees = round(args.rotations * 360)
    else:
        degrees = args.degrees
    if degrees == 0 or not -3600 <= degrees <= 3600:
        raise ValueError("目标角度必须在 -3600 到 3600 之间且不能为 0")

    motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_port={args.port}", flush=True)
        print(f"requested_degrees={degrees}", flush=True)
        print(f"requested_rotations={degrees / 360:g}", flush=True)
        print(f"requested_speed={args.speed}%", flush=True)
        started = False
        try:
            result = updater.start_motor_position(motor_port, args.speed, degrees)
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动位置控制；请确认该端口已稳定识别为大型或中型电机，且没有其他马达任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了马达位置控制命令")
            started = True
            print(
                f"motor_type={MOTOR_TYPE_NAMES.get(result.motor_type, 'unknown')}",
                flush=True,
            )
            print(f"effective_speed={result.requested_speed_percent}%", flush=True)
            previous_state = None
            deadline = time.monotonic() + 18.0
            while time.monotonic() < deadline:
                if result.state != previous_state:
                    print(
                        f"position_state={MOTOR_POSITION_STATES.get(result.state, 'unknown')}",
                        flush=True,
                    )
                    previous_state = result.state
                if result.state == 2:
                    actual_degrees = result.current_count - result.start_count
                    print(f"target_count={result.target_count}", flush=True)
                    print(f"current_count={result.current_count}", flush=True)
                    print(f"actual_degrees={actual_degrees}", flush=True)
                    print(f"actual_rotations={actual_degrees / 360:g}", flush=True)
                    print(f"position_error={result.error_count}", flush=True)
                    print(f"measured_speed={result.measured_speed_percent}%", flush=True)
                    print("motor_position=complete", flush=True)
                    return 0
                if result.state == 3:
                    raise EstUpdaterError(
                        f"马达位置控制超时，剩余误差 {result.error_count} 度"
                    )
                time.sleep(0.1)
                result = updater.read_motor_position_status(motor_port)
            raise EstUpdaterError("马达位置控制没有按时返回，已发送停止命令")
        finally:
            if started:
                try:
                    updater.stop_motor_position(motor_port)
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


def run_motor_speed(args: argparse.Namespace) -> int:
    if args.speed == 0 or not -100 <= args.speed <= 100 or abs(args.speed) < 10:
        raise ValueError("--speed 必须在 -100 到 100 之间，且绝对值至少为 10")
    if not 0.5 <= args.duration <= 30.0:
        raise ValueError("--duration 必须在 0.5 到 30 秒之间")

    motor_port = {"A": 0, "B": 1, "C": 2, "D": 3}[args.port]
    sample_count = max(1, int(args.duration / 0.25 + 0.999))
    sample_delay = args.duration / sample_count
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_port={args.port}", flush=True)
        print(f"requested_speed={args.speed}%", flush=True)
        print(f"duration_seconds={args.duration:g}", flush=True)
        started = False
        samples: list[int] = []
        try:
            result = updater.start_motor_speed(motor_port, args.speed)
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动定速控制；请先静止识别马达类型并确认没有其他马达任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了马达定速控制命令")
            started = True
            print(f"motor_type={MOTOR_TYPE_NAMES.get(result.motor_type, 'unknown')}", flush=True)
            for _ in range(sample_count):
                time.sleep(sample_delay)
                result = updater.read_motor_speed_status(motor_port)
                if result.result != 1 or result.state != 1:
                    raise EstUpdaterError("马达定速控制意外停止")
                samples.append(result.measured_speed_percent)
            stopped = updater.stop_motor_speed(motor_port, args.stop)
            if stopped.result != 1:
                raise EstUpdaterError(f"设备未能按 {args.stop} 停止定速控制")
            print(f"measured_speed_last={samples[-1]}%", flush=True)
            print(f"measured_speed_min={min(samples)}%", flush=True)
            print(f"measured_speed_max={max(samples)}%", flush=True)
            print(f"measured_speed_average={sum(samples) / len(samples):.1f}%", flush=True)
            print(f"applied_power_last={result.power_percent}%", flush=True)
            print(f"tacho_count={stopped.tacho_count}", flush=True)
            print(f"requested_stop={args.stop}", flush=True)
            print(
                f"stop_state={MOTOR_OUTPUT_STATES.get(stopped.output_state, 'unknown')}",
                flush=True,
            )
            if args.stop == "brake":
                time.sleep(0.3)
            print("motor_speed=complete", flush=True)
            return 0
        finally:
            if started:
                try:
                    safe = updater.stop_motor_speed(motor_port, "coast")
                    if safe.result != 1:
                        raise EstUpdaterError("最终自由滑行命令被设备拒绝")
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


DRIVE_STATES = {
    0: "idle",
    1: "running",
    2: "complete",
    3: "fault",
}


def run_motor_pair_position(args: argparse.Namespace) -> int:
    if args.left_port == args.right_port:
        raise ValueError("两个端口不能相同")
    if not 10 <= args.speed <= 100:
        raise ValueError("--speed 必须在 10 到 100 之间")
    degrees = (args.left_degrees, args.right_degrees)
    if any(value == 0 or not -3600 <= value <= 3600 for value in degrees):
        raise ValueError("两路目标角度必须在 -3600 到 3600 之间且不能为 0")
    if abs(args.left_degrees) != abs(args.right_degrees):
        raise ValueError("首版同步控制要求两路目标角度绝对值相同")

    port_numbers = {"A": 0, "B": 1, "C": 2, "D": 3}
    left_port = port_numbers[args.left_port]
    right_port = port_numbers[args.right_port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_ports={args.left_port},{args.right_port}", flush=True)
        print(
            f"target_degrees={args.left_degrees},{args.right_degrees}",
            flush=True,
        )
        print(f"maximum_speed={args.speed}%", flush=True)
        started = False
        try:
            result = updater.start_motor_pair_position(
                left_port,
                args.left_degrees,
                right_port,
                args.right_degrees,
                args.speed,
            )
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动双马达同步；请确认两端口均已识别为大型或中型且没有其他任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了双马达同步参数")
            started = True
            while True:
                telemetry = ""
                if result.state == 1:
                    left_status = updater.read_motor_speed_status(left_port)
                    right_status = updater.read_motor_speed_status(right_port)
                    telemetry = (
                        f" speed={left_status.measured_speed_percent},"
                        f"{right_status.measured_speed_percent}"
                        f" power={left_status.power_percent},"
                        f"{right_status.power_percent}"
                    )
                print(
                    f"pair_state={DRIVE_STATES.get(result.state, 'unknown')} "
                    f"actual={result.left_actual_degrees},{result.right_actual_degrees} "
                    f"sync_error={result.synchronization_error_degrees} "
                    f"max_sync_error={result.maximum_synchronization_error_degrees}"
                    f"{telemetry}",
                    flush=True,
                )
                if result.state == 2:
                    print("motor_pair_position=complete", flush=True)
                    return 0
                if result.state == 3:
                    raise EstUpdaterError(
                        f"双马达同步故障，错误码 {result.error}"
                    )
                time.sleep(0.2)
                result = updater.read_motor_pair_position_status()
        finally:
            if started:
                try:
                    stopped = updater.stop_motor_pair_position()
                    if stopped.result != 1:
                        raise EstUpdaterError("最终联动自由滑行命令被拒绝")
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


def run_motor_pair_speed(args: argparse.Namespace) -> int:
    speeds = (args.left_speed, args.right_speed)
    if args.left_port == args.right_port:
        raise ValueError("两个端口不能相同")
    if any(speed == 0 or not -100 <= speed <= 100 or abs(speed) < 10 for speed in speeds):
        raise ValueError("两个速度的绝对值都必须在 10 到 100 之间")
    if abs(args.left_speed) != abs(args.right_speed):
        raise ValueError("首版持续同步要求两个速度的绝对值相同")
    if not 0.5 <= args.duration <= 60.0:
        raise ValueError("--duration 必须在 0.5 到 60 秒之间")

    port_numbers = {"A": 0, "B": 1, "C": 2, "D": 3}
    left_port = port_numbers[args.left_port]
    right_port = port_numbers[args.right_port]
    sample_count = max(1, int(args.duration / 0.2))
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"motor_ports={args.left_port},{args.right_port}", flush=True)
        print(f"requested_speed={args.left_speed},{args.right_speed}", flush=True)
        print(f"observation_seconds={args.duration:g}", flush=True)
        print("firmware_timeout=none", flush=True)
        started = False
        try:
            result = updater.start_motor_pair_speed(
                left_port,
                args.left_speed,
                right_port,
                args.right_speed,
            )
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动双马达持续定速；请确认两端口已有马达且没有其他任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了双马达持续定速参数")
            started = True
            for _ in range(sample_count):
                time.sleep(0.2)
                result = updater.read_motor_pair_speed_status()
                if result.result != 1 or result.state != 1:
                    raise EstUpdaterError("双马达持续定速意外停止")
                print(
                    "pair_speed_state=running "
                    f"actual={result.left_actual_degrees},{result.right_actual_degrees} "
                    f"sync_error={result.synchronization_error_degrees} "
                    f"max_sync_error={result.maximum_synchronization_error_degrees} "
                    f"speed={result.left_measured_speed_percent},"
                    f"{result.right_measured_speed_percent} "
                    f"power={result.left_power_percent},{result.right_power_percent}",
                    flush=True,
                )
            stopped = updater.stop_motor_pair_speed(args.stop)
            if stopped.result != 1 or stopped.state != 0:
                raise EstUpdaterError(f"设备未能按 {args.stop} 停止双马达定速")
            print(f"stop_state={args.stop}", flush=True)
            if args.stop == "brake":
                time.sleep(0.3)
                left_safe = updater.stop_motor_speed(left_port, "coast")
                right_safe = updater.stop_motor_speed(right_port, "coast")
                if left_safe.result != 1 or right_safe.result != 1:
                    raise EstUpdaterError("刹车后未能恢复双路自由滑行")
            started = False
            print("motor_pair_speed=complete", flush=True)
            print("safe_final_state=coast", flush=True)
            return 0
        finally:
            if started:
                try:
                    safe = updater.stop_motor_pair_speed("coast")
                    if safe.result != 1:
                        raise EstUpdaterError("最终双路自由滑行命令被拒绝")
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


def run_drive_straight(args: argparse.Namespace) -> int:
    if args.left_port == args.right_port:
        raise ValueError("左右轮端口不能相同")
    if not 1 <= args.wheel_diameter <= 0xFFFF:
        raise ValueError("--wheel-diameter 必须在 1 到 65535 毫米之间")
    if not 1 <= args.axle_track <= 0xFFFF:
        raise ValueError("--axle-track 必须在 1 到 65535 毫米之间")
    if args.distance == 0:
        raise ValueError("--distance 不能为 0")
    if not 10 <= args.speed <= 100:
        raise ValueError("--speed 必须在 10 到 100 之间")
    numerator = args.distance * 360 * 113
    denominator = args.wheel_diameter * 355
    target_degrees = (
        (numerator + denominator // 2) // denominator
        if numerator >= 0
        else -((-numerator + denominator // 2) // denominator)
    )
    if target_degrees == 0 or not -3600 <= target_degrees <= 3600:
        raise ValueError("当前轮径和距离换算后的轮角度必须在 -3600 到 3600 之间且不能为 0")

    port_numbers = {"A": 0, "B": 1, "C": 2, "D": 3}
    left_port = port_numbers[args.left_port]
    right_port = port_numbers[args.right_port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"drive_ports={args.left_port},{args.right_port}", flush=True)
        print(f"wheel_diameter_mm={args.wheel_diameter}", flush=True)
        print(f"axle_track_mm={args.axle_track}", flush=True)
        print(f"target_distance_mm={args.distance}", flush=True)
        print(f"expected_wheel_degrees={target_degrees}", flush=True)
        print(f"maximum_speed={args.speed}%", flush=True)
        started = False
        try:
            result = updater.start_drive_straight(
                left_port,
                right_port,
                args.wheel_diameter,
                args.axle_track,
                args.distance,
                args.speed,
            )
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动直行；请确认两端口已有马达且没有其他任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了底盘直行参数")
            started = True
            while True:
                print(
                    f"drive_state={DRIVE_STATES.get(result.state, 'unknown')} "
                    f"distance={result.actual_distance_mm}/"
                    f"{result.target_distance_mm}mm "
                    f"actual={result.left_actual_degrees},"
                    f"{result.right_actual_degrees} "
                    f"sync_error={result.synchronization_error_degrees} "
                    f"max_sync_error={result.maximum_synchronization_error_degrees}",
                    flush=True,
                )
                if result.state == 2:
                    print("drive_straight=complete", flush=True)
                    return 0
                if result.state == 3:
                    raise EstUpdaterError(
                        f"底盘直行故障，错误码 {result.error}"
                    )
                time.sleep(0.2)
                result = updater.read_drive_straight_status()
        finally:
            if started:
                try:
                    stopped = updater.stop_drive_straight()
                    if stopped.result != 1:
                        raise EstUpdaterError("最终底盘自由滑行命令被拒绝")
                    print("safe_final_state=coast", flush=True)
                except (EstUpdaterError, OSError):
                    print("safe_final_state=unconfirmed", flush=True)


def run_drive_run(args: argparse.Namespace) -> int:
    if args.left_port == args.right_port:
        raise ValueError("左右轮端口不能相同")
    if not 10 <= args.speed <= 100:
        raise ValueError("--speed 必须在 10 到 100 之间")
    if args.rotations is not None:
        if args.rotations == 0 or not -10 <= args.rotations <= 10:
            raise ValueError("--rotations 必须在 -10 到 10 圈之间且不能为 0")
        target_value = int(round(args.rotations * 360.0))
        if target_value == 0:
            raise ValueError("圈数换算后的角度不能为 0")
        mode = 0
        target_unit = "rotations"
        target_input = args.rotations
    elif args.degrees is not None:
        if args.degrees == 0 or not -3600 <= args.degrees <= 3600:
            raise ValueError("--degrees 必须在 -3600 到 3600 之间且不能为 0")
        target_value = args.degrees
        mode = 0
        target_unit = "degrees"
        target_input = args.degrees
    else:
        if args.seconds == 0 or not -600.0 <= args.seconds <= 600.0:
            raise ValueError("--seconds 必须在 -600 到 600 秒之间且不能为 0")
        target_value = int(round(args.seconds * 1000.0))
        if target_value == 0:
            raise ValueError("秒数换算后的毫秒数不能为 0")
        mode = 1
        target_unit = "seconds"
        target_input = args.seconds
    if mode == 0 and args.stop != "coast":
        raise ValueError("角度和圈数模式当前只支持 --stop coast")

    port_numbers = {"A": 0, "B": 1, "C": 2, "D": 3}
    left_port = port_numbers[args.left_port]
    right_port = port_numbers[args.right_port]
    with open_transport(args) as transport:
        print(f"device={transport.path}", flush=True)
        updater = FirmwareUpdater(transport)
        print(f"current_version={updater.ping()}", flush=True)
        print(f"drive_ports={args.left_port},{args.right_port}", flush=True)
        print(f"target_unit={target_unit}", flush=True)
        print(f"target_input={target_input:g}", flush=True)
        print(f"firmware_target={target_value}", flush=True)
        print(f"speed={args.speed}%", flush=True)
        print(f"stop_mode={args.stop}", flush=True)
        started = False
        try:
            result = updater.start_drive_run(
                left_port,
                right_port,
                mode,
                target_value,
                args.speed,
                args.stop,
            )
            if result.result == 2:
                raise EstUpdaterError(
                    "设备未启动直行；请确认两端口已有马达且没有其他任务"
                )
            if result.result != 1:
                raise EstUpdaterError("设备拒绝了圈/度/秒直行参数")
            started = True
            while True:
                value_suffix = "ms" if result.mode == 1 else "deg"
                print(
                    f"drive_state={DRIVE_STATES.get(result.state, 'unknown')} "
                    f"progress={result.actual_value}/"
                    f"{result.target_value}{value_suffix} "
                    f"actual={result.left_actual_degrees},"
                    f"{result.right_actual_degrees} "
                    f"sync_error={result.synchronization_error_degrees} "
                    f"max_sync_error={result.maximum_synchronization_error_degrees}",
                    flush=True,
                )
                if result.state == 2:
                    print("drive_run=complete", flush=True)
                    return 0
                if result.state == 3:
                    raise EstUpdaterError(
                        f"圈/度/秒直行故障，错误码 {result.error}"
                    )
                if result.state == 0:
                    raise EstUpdaterError("圈/度/秒直行在完成前意外停止")
                time.sleep(0.2)
                result = updater.read_drive_run_status()
        finally:
            if started:
                try:
                    stopped = updater.stop_drive_run()
                    if stopped.result != 1:
                        raise EstUpdaterError("最终底盘自由滑行命令被拒绝")
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
        if args.mode == "device-status":
            return run_device_status(args)
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
        if args.mode == "motor-types":
            return run_motor_types(args)
        if args.mode == "motor-identify":
            return run_motor_identify(args)
        if args.mode == "motor-tacho-test":
            return run_motor_tacho_test(args)
        if args.mode == "motor-stop-compare":
            return run_motor_stop_compare(args)
        if args.mode == "motor-dual-test":
            return run_motor_dual_test(args)
        if args.mode == "motor-control":
            return run_motor_control(args)
        if args.mode == "motor-position":
            return run_motor_position(args)
        if args.mode == "motor-speed":
            return run_motor_speed(args)
        if args.mode == "motor-pair-position":
            return run_motor_pair_position(args)
        if args.mode == "motor-pair-speed":
            return run_motor_pair_speed(args)
        if args.mode == "drive-straight":
            return run_drive_straight(args)
        if args.mode == "drive-run":
            return run_drive_run(args)
        if args.mode == "motor-pair-control":
            return run_motor_pair_control(args)
        if args.mode == "sensor-read":
            return run_sensor_read(args)
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
