from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parents[2]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from est_hid_sender.hid_transport import HidTransport
from est_hid_sender.updater import FirmwareUpdater


PORTS = {"A": 0, "B": 1, "C": 2, "D": 3}
POSITION_STATES = {0: "IDLE", 1: "RUNNING", 2: "COMPLETE", 3: "TIMEOUT"}
OUTPUT_STATES = {0: "OFF", 1: "DRIVE", 2: "BRAKE"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture an opt-in motor-position endpoint trace over existing HID diagnostics."
    )
    parser.add_argument("--port", choices=PORTS, required=True)
    parser.add_argument("--speed", type=int, required=True)
    parser.add_argument("--degrees", type=int, required=True)
    parser.add_argument("--sample-ms", type=int, default=20)
    parser.add_argument("--post-complete-ms", type=int, default=500)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def output_direction(power_percent: int) -> str:
    if power_percent > 0:
        return "FORWARD"
    if power_percent < 0:
        return "REVERSE"
    return "NONE"


def settling_entry(motor_type: int, speed_percent: int) -> int:
    speed = abs(speed_percent)
    if speed <= 20:
        return 20 if motor_type == 5 else 10
    if speed <= 30:
        return 60
    if speed <= 40:
        return 70
    if speed <= 50:
        return 100 if motor_type == 5 else 90
    if speed <= 60:
        return 110
    if speed <= 70:
        return 120 if motor_type == 5 else 130
    if speed <= 80:
        return 130 if motor_type == 5 else 140
    return 180 if motor_type == 5 else 160


def control_phase(
    firmware: str,
    motor_type: int,
    state: int,
    position_error: int,
    requested_speed: int,
    previous_phase: str,
) -> str:
    if state != 1:
        return POSITION_STATES.get(state, "UNKNOWN")
    if firmware == "M1.14A":
        if previous_phase == "SETTLING" or abs(position_error) <= 2 * settling_entry(
            motor_type, requested_speed
        ):
            return "SETTLING"
    return "TRACKING"


def summarize(
    rows: list[dict[str, int | str]], command_sign: int, motor_type: int
) -> dict[str, int | None]:
    target = int(rows[0]["target_count"])
    completion_ms = next(
        (int(row["elapsed_ms"]) for row in rows if row["completion"] == "yes"), None
    )
    signed_overshoots = [
        command_sign * (int(row["current_count"]) - target) for row in rows
    ]
    maximum_overshoot = max(0, max(signed_overshoots))

    reversals = 0
    previous_sign = 0
    for row in rows:
        power = int(row["pwm_percent"])
        if abs(power) < 5:
            continue
        power_sign = 1 if power > 0 else -1 if power < 0 else 0
        if power_sign != 0:
            if previous_sign != 0 and power_sign != previous_sign:
                reversals += 1
            previous_sign = power_sign

    position_tolerance = 5 if motor_type == 5 else 2
    stable_since: int | None = None
    for index, row in enumerate(rows):
        if (
            abs(int(row["position_error"])) > position_tolerance
            or abs(int(row["measured_speed"])) > 2
        ):
            continue
        start_ms = int(row["elapsed_ms"])
        stable = True
        for later in rows[index:]:
            if int(later["elapsed_ms"]) - start_ms >= 80:
                break
            if (
                abs(int(later["position_error"])) > position_tolerance
                or abs(int(later["measured_speed"])) > 2
            ):
                stable = False
                break
        if stable and int(rows[-1]["elapsed_ms"]) - start_ms >= 80:
            stable_since = start_ms
            break

    return {
        "completion_ms": completion_ms,
        "maximum_overshoot_counts": maximum_overshoot,
        "settled_ms": stable_since,
        "obvious_direction_reversals": reversals,
        "maximum_reverse_pwm_percent": max(
            0,
            max(-command_sign * int(row["pwm_percent"]) for row in rows),
        ),
        "final_error_counts": int(rows[-1]["position_error"]),
    }


def main() -> int:
    args = parse_args()
    if not 0 <= args.speed <= 100:
        raise ValueError("--speed must be between 0 and 100")
    if args.degrees == 0 or not -3600 <= args.degrees <= 3600:
        raise ValueError("--degrees must be between -3600 and 3600 and non-zero")
    if args.sample_ms < 10:
        raise ValueError("--sample-ms must be at least 10")

    rows: list[dict[str, int | str]] = []
    port = PORTS[args.port]
    started = False
    failure: str | None = None
    with HidTransport.open() as transport:
        updater = FirmwareUpdater(transport)
        version = updater.ping()
        started_at = time.monotonic()
        completed_at: float | None = None
        phase = "TRACKING"
        try:
            position = updater.start_motor_position(port, args.speed, args.degrees)
            if position.result != 1:
                raise RuntimeError(f"motor-position start failed: result={position.result}")
            started = True
            deadline = started_at + args.timeout
            while time.monotonic() < deadline:
                speed = updater.read_motor_speed_status(port)
                now = time.monotonic()
                elapsed_ms = round((now - started_at) * 1000)
                phase = control_phase(
                    version,
                    position.motor_type,
                    position.state,
                    position.error_count,
                    position.requested_speed_percent,
                    phase,
                )
                rows.append(
                    {
                        "elapsed_ms": elapsed_ms,
                        "target_count": position.target_count,
                        "current_count": position.current_count,
                        "position_error": position.error_count,
                        "measured_speed": position.measured_speed_percent,
                        "pwm_percent": speed.power_percent,
                        "output_direction": output_direction(speed.power_percent),
                        "output_state": OUTPUT_STATES.get(speed.output_state, "UNKNOWN"),
                        "control_phase": phase,
                        "completion": "yes" if position.state == 2 else "no",
                    }
                )
                if position.state == 3:
                    failure = "motor-position timed out"
                    break
                if position.state == 2:
                    if completed_at is None:
                        completed_at = now
                    elif (now - completed_at) * 1000 >= args.post_complete_ms:
                        break
                time.sleep(args.sample_ms / 1000)
                position = updater.read_motor_position_status(port)
            else:
                failure = "motor-position trace timed out"
        except Exception as error:
            failure = str(error)
        finally:
            if started:
                updater.stop_motor_position(port)

    if not rows:
        raise RuntimeError(failure or "motor-position produced no trace rows")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "firmware": version,
        "port": args.port,
        "speed_percent": args.speed,
        "degrees": args.degrees,
        "trace": str(args.output),
        "failure": failure,
        **summarize(
            rows,
            1 if args.degrees > 0 else -1,
            position.motor_type,
        ),
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 1 if failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
