#!/usr/bin/env python3
"""Check current firmware pin use against the verified V5 hardware map."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path


PIN_TABLE_RELATIVE = Path(
    "references/hardware/current/EST_MainControl_V5_pin_map.csv"
)

FIRMWARE_NETS = {
    "PB_CON": "board_power.c",
    "RED": "board_led.c",
    "BLUE": "board_led.c",
    "KEY0": "board_keys.c",
    "KEY1": "board_keys.c",
    "KEY2": "board_keys.c",
    "KEY3": "board_keys.c",
    "KEY4": "board_keys.c",
    "KEY5": "board_keys.c",
    "LCD_SCK": "board_lcd.c",
    "LCD_SDAI": "board_lcd.c",
    "LCD_RST": "board_lcd.c",
    "ULPI_D0": "usb_hid.c",
    "ULPI_D1": "usb_hid.c",
    "ULPI_D2": "usb_hid.c",
    "ULPI_D3": "usb_hid.c",
    "ULPI_D4": "usb_hid.c",
    "ULPI_D5": "usb_hid.c",
    "ULPI_D6": "usb_hid.c",
    "ULPI_D7": "usb_hid.c",
    "ULPI_STP": "usb_hid.c",
    "ULPI_NXT": "usb_hid.c",
    "ULPI_DIR": "usb_hid.c",
    "ULPI_CK": "usb_hid.c",
}


def load_pin_table(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))

    required_columns = {
        "package_pin",
        "mcu_pin",
        "current_net",
        "subsystem",
        "kind",
        "verification",
        "notes",
    }
    if not rows or set(rows[0]) != required_columns:
        raise ValueError("pin table columns do not match the required schema")
    return rows


def validate_table(rows: list[dict[str, str]]) -> dict[str, str]:
    package_pins = [int(row["package_pin"]) for row in rows]
    if len(rows) != 144 or set(package_pins) != set(range(1, 145)):
        raise ValueError("pin table must contain every package pin from 1 to 144")
    if len(package_pins) != len(set(package_pins)):
        raise ValueError("pin table contains a duplicate package pin")

    gpio_rows = [row for row in rows if row["kind"] == "gpio"]
    gpio_pins = [row["mcu_pin"] for row in gpio_rows]
    duplicates = sorted({pin for pin in gpio_pins if gpio_pins.count(pin) > 1})
    if duplicates:
        raise ValueError("GPIO pin conflict in table: " + ", ".join(duplicates))

    net_to_pin: dict[str, str] = {}
    for row in gpio_rows:
        net = row["current_net"]
        if net != "NC":
            if net in net_to_pin:
                raise ValueError(f"network {net} is assigned more than once")
            net_to_pin[net] = row["mcu_pin"]

    missing = sorted(set(FIRMWARE_NETS) - set(net_to_pin))
    if missing:
        raise ValueError("firmware networks missing from pin table: " + ", ".join(missing))
    return net_to_pin


def gpio_literal(pin: str) -> str:
    match = re.fullmatch(r"P([A-Z])(\d{1,2})", pin)
    if match is None:
        raise ValueError(f"invalid GPIO pin name: {pin}")
    return f"GPIO{match.group(1)}, GPIO{int(match.group(2))}"


def read_source(source_dir: Path, name: str) -> str:
    return (source_dir / name).read_text(encoding="utf-8")


def validate_simple_modules(source_dir: Path, net_to_pin: dict[str, str]) -> None:
    for net in ("PB_CON", "RED", "BLUE", "KEY0", "KEY1", "KEY2", "KEY3", "KEY4", "KEY5"):
        source_name = FIRMWARE_NETS[net]
        source = read_source(source_dir, source_name)
        expected = gpio_literal(net_to_pin[net])
        if expected not in source:
            raise ValueError(f"{source_name} does not use {net}={net_to_pin[net]}")


def read_lcd_pin(source: str, role: str) -> str:
    port_match = re.search(rf"#define LCD_{role}_PORT GPIO([A-Z])", source)
    pin_match = re.search(rf"#define LCD_{role}_PIN GPIO(\d+)", source)
    if port_match is None or pin_match is None:
        raise ValueError(f"board_lcd.c is missing the LCD_{role} pin macros")
    return f"P{port_match.group(1)}{int(pin_match.group(1))}"


def validate_lcd(source_dir: Path, net_to_pin: dict[str, str]) -> None:
    source = read_source(source_dir, "board_lcd.c")
    roles = {"CLOCK": "LCD_SCK", "DATA": "LCD_SDAI", "RESET": "LCD_RST"}
    for role, net in roles.items():
        actual_pin = read_lcd_pin(source, role)
        if actual_pin != net_to_pin[net]:
            raise ValueError(
                f"board_lcd.c uses {net}={actual_pin}, expected {net_to_pin[net]}"
            )


def read_mask_pins(source: str, variable: str, port: str) -> set[str]:
    match = re.search(rf"const uint16_t {variable} = (.*?);", source, re.DOTALL)
    if match is None:
        raise ValueError(f"usb_hid.c is missing {variable}")
    return {f"P{port}{int(number)}" for number in re.findall(r"GPIO(\d+)", match.group(1))}


def validate_usb(source_dir: Path, net_to_pin: dict[str, str]) -> None:
    source = read_source(source_dir, "usb_hid.c")
    actual = set()
    actual |= read_mask_pins(source, "gpioa_ulpi", "A")
    actual |= read_mask_pins(source, "gpiob_ulpi", "B")
    actual |= read_mask_pins(source, "gpioc_ulpi", "C")
    expected = {net_to_pin[net] for net in FIRMWARE_NETS if net.startswith("ULPI_")}
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ValueError(f"USB ULPI pin mismatch; missing={missing}, extra={extra}")


def validate_claim_conflicts(net_to_pin: dict[str, str]) -> None:
    claims: dict[str, list[str]] = {}
    for net in FIRMWARE_NETS:
        claims.setdefault(net_to_pin[net], []).append(net)
    conflicts = {pin: nets for pin, nets in claims.items() if len(nets) > 1}
    if conflicts:
        details = "; ".join(
            f"{pin}={'/'.join(nets)}" for pin, nets in sorted(conflicts.items())
        )
        raise ValueError("firmware GPIO conflict: " + details)


def validate_repository(repo_root: Path) -> tuple[int, int, int]:
    rows = load_pin_table(repo_root / PIN_TABLE_RELATIVE)
    net_to_pin = validate_table(rows)
    source_dir = repo_root / "firmware/minimal_upgrade_app/src"
    validate_simple_modules(source_dir, net_to_pin)
    validate_lcd(source_dir, net_to_pin)
    validate_usb(source_dir, net_to_pin)
    validate_claim_conflicts(net_to_pin)
    gpio_count = sum(row["kind"] == "gpio" for row in rows)
    return len(rows), gpio_count, len(FIRMWARE_NETS)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
    )
    args = parser.parse_args()
    package_count, gpio_count, claim_count = validate_repository(args.repo_root.resolve())
    print(f"pin table: {package_count} package pins, {gpio_count} GPIO pins")
    print(f"firmware claims: {claim_count}, conflicts: 0")
    print("hardware pin check passed")


if __name__ == "__main__":
    main()
