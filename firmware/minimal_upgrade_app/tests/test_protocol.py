from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from package_firmware import (  # noqa: E402
    APP_FLASH_START,
    DEFAULT_PACKAGE_SIZE,
    build_packages,
    validate_app_image,
    write_outputs,
)
from protocol_reference import (  # noqa: E402
    FRAME_END,
    MAX_PAYLOAD,
    REPORT_SIZE,
    build_update_frame,
    checksum,
    parse_update_ack,
    split_hid_reports,
)
from verify_build import simulate_current_bootloader_copy, verify_version_pair  # noqa: E402

SOURCE_DIR = ROOT / "src"


class PackageTests(unittest.TestCase):
    @staticmethod
    def valid_image() -> bytes:
        return (0x20030000).to_bytes(4, "little") + (
            APP_FLASH_START + 0x101
        ).to_bytes(4, "little") + b"minimal-app"

    def test_package_has_header_padding_and_legacy_length(self) -> None:
        unpadded, padded = build_packages(self.valid_image(), DEFAULT_PACKAGE_SIZE)
        self.assertTrue(unpadded.startswith(b"APP="))
        self.assertEqual(len(padded), 256 * 1024)
        self.assertEqual(padded[4 : 4 + len(self.valid_image())], self.valid_image())
        self.assertEqual(set(padded[len(unpadded):]), {0xFF})

    def test_stack_top_at_end_of_sram_is_valid(self) -> None:
        msp, reset = validate_app_image(self.valid_image())
        self.assertEqual(msp, 0x20030000)
        self.assertEqual(reset & 1, 1)

    def test_manifest_matches_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            image_path = directory / "app.bin"
            image_path.write_bytes(self.valid_image())
            manifest = write_outputs(
                image_path, directory, "test", "M0.01A", DEFAULT_PACKAGE_SIZE
            )
            self.assertEqual(manifest["header_ascii"], "APP=")
            self.assertEqual(
                (directory / "test.upgrade.bin").stat().st_size,
                len(build_packages(self.valid_image(), DEFAULT_PACKAGE_SIZE)[1]),
            )

    def test_rejects_non_thumb_reset_handler(self) -> None:
        image = (0x20001000).to_bytes(4, "little") + APP_FLASH_START.to_bytes(
            4, "little"
        )
        with self.assertRaisesRegex(ValueError, "Reset_Handler"):
            validate_app_image(image)


class ProtocolTests(unittest.TestCase):
    def test_max_frame_fits_one_hs_report(self) -> None:
        payload = b"APP=" + bytes(MAX_PAYLOAD - 4)
        frame = build_update_frame(260, 0, payload)
        self.assertEqual(len(frame), 1021)
        reports = split_hid_reports(frame)
        self.assertEqual(len(reports), 1)
        self.assertTrue(all(len(report) == REPORT_SIZE for report in reports))
        rebuilt = b"".join(reports)[: len(frame)]
        self.assertEqual(rebuilt, frame)
        self.assertEqual(frame[-1], FRAME_END)
        self.assertEqual(checksum(frame[:-2]), frame[-2])

    def test_parse_zero_padded_ack(self) -> None:
        report = bytearray(REPORT_SIZE)
        report[:10] = bytes((0x68, 0x21, 0x05, 0x05, 0x00, 0x04, 0x01, 0x03, 0x00, 0x01))
        report[10] = checksum(report[:10])
        report[11] = 0x16
        ack = parse_update_ack(bytes(report))
        self.assertEqual(ack.total_frames, 260)
        self.assertEqual(ack.frame_index, 3)
        self.assertEqual(ack.flag, 1)


class BuildVerificationTests(unittest.TestCase):
    def test_version_object_is_rebuilt_when_build_version_changes(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("$(BUILD_DIR)/obj/app_version.o: FORCE", makefile)
        self.assertIn("verification-pair FORCE", makefile)

    def test_legacy_bootloader_overlap_reproduces_raw_app(self) -> None:
        image = PackageTests.valid_image()
        _, upgrade = build_packages(image, DEFAULT_PACKAGE_SIZE)
        copied_app = simulate_current_bootloader_copy(upgrade)
        self.assertEqual(copied_app[: len(image)], image)

    def test_version_pair_differs_only_in_version_text(self) -> None:
        first = b"prefix-M0.01A-suffix"
        second = b"prefix-M0.02A-suffix"
        verify_version_pair(first, "M0.01A", second, "M0.02A")

    def test_version_pair_rejects_an_unrelated_change(self) -> None:
        first = b"prefix-M0.01A-suffix"
        second = b"Prefix-M0.02A-suffix"
        with self.assertRaisesRegex(ValueError, "outside"):
            verify_version_pair(first, "M0.01A", second, "M0.02A")


class EstServiceApiTests(unittest.TestCase):
    def test_public_types_freeze_errors_ports_and_stop_modes(self) -> None:
        types = (ROOT / "include" / "est_types.h").read_text(encoding="utf-8")
        for token in (
            "EST_ERR_INVALID_ARGUMENT",
            "EST_ERR_INVALID_PORT",
            "EST_ERR_NOT_CONNECTED",
            "EST_ERR_TYPE_MISMATCH",
            "EST_ERR_NOT_SUPPORTED",
            "EST_ERR_BUSY",
            "EST_ERR_TIMEOUT",
            "EST_ERR_IO",
            "EST_ERR_STATE",
            "EST_MOTOR_PORT_A",
            "EST_MOTOR_PORT_D",
            "EST_SENSOR_PORT_1",
            "EST_SENSOR_PORT_4",
            "EST_STOP_COAST",
            "EST_STOP_BRAKE",
            "EST_STOP_HOLD",
        ):
            self.assertIn(token, types)

    def test_motor_contract_wraps_the_verified_board_driver(self) -> None:
        header = (ROOT / "include" / "est_motor.h").read_text(encoding="utf-8")
        source = (SOURCE_DIR / "est_motor.c").read_text(encoding="utf-8")
        for function in (
            "est_motor_get_type",
            "est_motor_set_power",
            "est_motor_run_speed",
            "est_motor_run_time",
            "est_motor_run_angle",
            "est_motor_stop",
            "est_motor_stop_all",
            "est_motor_reset_angle",
            "est_motor_get_status",
        ):
            self.assertIn(function, header)
            self.assertIn(function, source)
        self.assertIn("board_motor_start_speed(est_system_millis()", source)
        self.assertIn("board_motor_start_speed_for_time(est_system_millis()", source)
        self.assertIn("board_motor_start_position(est_system_millis()", source)
        self.assertIn("update_timed_speed_state(now_ms)", (
            SOURCE_DIR / "board_motor.c"
        ).read_text(encoding="utf-8"))
        self.assertIn("EST_MOTOR_TIMED", source)
        self.assertIn("return EST_ERR_NOT_SUPPORTED;", source)

    def test_sensor_contract_wraps_the_verified_board_driver(self) -> None:
        header = (ROOT / "include" / "est_sensor.h").read_text(encoding="utf-8")
        source = (SOURCE_DIR / "est_sensor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        for function in (
            "est_sensor_get_type",
            "est_sensor_set_mode",
            "est_sensor_restart",
            "est_sensor_get_status",
        ):
            self.assertIn(function, header)
            self.assertIn(function, source)
        self.assertIn("board_sensor_get_snapshot", source)
        self.assertIn("board_sensor_set_mode", source)
        self.assertIn("board_sensor_restart", source)
        self.assertIn('#include "est_sensor.h"', protocol)
        self.assertIn("est_sensor_set_mode", protocol)
        self.assertIn("est_sensor_restart", protocol)
        self.assertIn("est_sensor_get_status", protocol)
        self.assertNotIn("board_sensor_get_snapshot", protocol)
        self.assertNotIn("board_sensor_set_mode", protocol)
        self.assertNotIn("board_sensor_restart", protocol)

    def test_user_interface_contract_wraps_verified_board_drivers(self) -> None:
        buttons_header = (ROOT / "include" / "est_buttons.h").read_text(
            encoding="utf-8"
        )
        buttons_source = (SOURCE_DIR / "est_buttons.c").read_text(
            encoding="utf-8"
        )
        led_header = (ROOT / "include" / "est_led.h").read_text(encoding="utf-8")
        led_source = (SOURCE_DIR / "est_led.c").read_text(encoding="utf-8")
        backlight_header = (ROOT / "include" / "est_backlight.h").read_text(
            encoding="utf-8"
        )
        backlight_source = (SOURCE_DIR / "est_backlight.c").read_text(
            encoding="utf-8"
        )
        display_header = (ROOT / "include" / "est_display.h").read_text(
            encoding="utf-8"
        )
        display_source = (SOURCE_DIR / "est_display.c").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")

        for function in (
            "est_buttons_pressed_mask",
            "est_button_is_pressed",
            "est_buttons_take_pressed_events",
            "est_buttons_take_released_events",
            "est_buttons_take_long_press_events",
        ):
            self.assertIn(function, buttons_header)
            self.assertIn(function, buttons_source)
        self.assertIn("board_keys_pressed_mask", buttons_source)

        for function in ("est_led_set", "est_led_get"):
            self.assertIn(function, led_header)
            self.assertIn(function, led_source)
        self.assertIn("board_led_checkpoint", led_source)

        for function in (
            "est_backlight_set_percent",
            "est_backlight_get_percent",
        ):
            self.assertIn(function, backlight_header)
            self.assertIn(function, backlight_source)
        self.assertIn("board_backlight_set_percent", backlight_source)

        for function in (
            "est_display_clear",
            "est_display_pixel",
            "est_display_line",
            "est_display_rectangle",
            "est_display_text",
            "est_display_bitmap",
            "est_display_refresh",
        ):
            self.assertIn(function, display_header)
            self.assertIn(function, display_source)
        self.assertIn("board_lcd_draw_bitmap", display_source)
        self.assertIn('#include "est_buttons.h"', protocol)
        self.assertIn("est_buttons_pressed_mask()", protocol)
        self.assertNotIn("board_keys_pressed_mask()", protocol)
        self.assertIn("est_display_text(36U, 54U, app_version_text, 3U)", main)

    def test_battery_and_system_contracts_wrap_verified_board_services(self) -> None:
        battery_header = (ROOT / "include" / "est_battery.h").read_text(
            encoding="utf-8"
        )
        battery_source = (SOURCE_DIR / "est_battery.c").read_text(
            encoding="utf-8"
        )
        system_header = (ROOT / "include" / "est_system.h").read_text(
            encoding="utf-8"
        )
        system_source = (SOURCE_DIR / "est_system.c").read_text(
            encoding="utf-8"
        )
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        usb = (SOURCE_DIR / "usb_hid.c").read_text(encoding="utf-8")

        for function in (
            "est_battery_init",
            "est_battery_tick",
            "est_battery_get_status",
        ):
            self.assertIn(function, battery_header)
            self.assertIn(function, battery_source)
        self.assertIn("board_battery_snapshot", battery_source)
        self.assertIn("EST_BATTERY_LOW_LEVEL_MAX", battery_header)

        for function in (
            "est_system_init",
            "est_system_millis",
            "est_system_emergency_stop",
            "est_system_cleanup",
            "est_system_reboot",
            "est_system_power_off",
        ):
            self.assertIn(function, system_header)
            self.assertIn(function, system_source)
        self.assertIn("est_motor_stop_all(EST_STOP_COAST)", system_source)
        self.assertIn("board_sensor_stop();", system_source)
        self.assertIn("return EST_ERR_NOT_SUPPORTED;", system_source)
        self.assertNotIn("scb_reset_system", system_source)
        self.assertIn("est_system_power_off();", main)
        self.assertIn("est_system_emergency_stop();", protocol)
        self.assertIn("est_battery_get_status(&battery)", protocol)
        self.assertIn("est_system_millis()", usb)
        self.assertNotIn("board_battery_snapshot()", protocol)

    def test_drive_contract_is_frozen_before_sync_implementation(self) -> None:
        drive = (ROOT / "include" / "est_drive.h").read_text(encoding="utf-8")
        for function in (
            "est_drive_config",
            "est_motor_pair_run_angles",
            "est_motor_pair_run_speeds",
            "est_motor_pair_stop",
            "est_motor_pair_get_speed_status",
            "est_drive_straight",
            "est_drive_turn",
            "est_drive_arc",
            "est_drive_stop",
            "est_drive_get_status",
        ):
            self.assertIn(function, drive)

    def test_hid_motor_commands_enter_through_est_service_layer(self) -> None:
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        self.assertIn('#include "est_motor.h"', protocol)
        self.assertIn("est_motor_set_power", protocol)
        self.assertIn("est_motor_run_speed", protocol)
        self.assertIn("est_motor_run_angle", protocol)
        self.assertIn("est_motor_stop", protocol)
        self.assertIn("est_motor_reset_angle", protocol)
        self.assertNotIn("board_motor_start_speed(now_ms", protocol)
        self.assertNotIn("board_motor_start_position(now_ms", protocol)


class BoardModuleLayoutTests(unittest.TestCase):
    def test_platform_no_longer_owns_board_gpio_or_time(self) -> None:
        platform = (SOURCE_DIR / "platform.c").read_text(encoding="utf-8")
        self.assertNotIn("gpio_", platform)
        self.assertNotIn("iwdg_reset", platform)
        self.assertNotIn("sys_tick_handler", platform)

    def test_board_modules_keep_existing_power_and_led_pins(self) -> None:
        power = (SOURCE_DIR / "board_power.c").read_text(encoding="utf-8")
        led = (SOURCE_DIR / "board_led.c").read_text(encoding="utf-8")
        self.assertIn("GPIOE, GPIO2", power)
        self.assertIn("GPIOF, GPIO2", led)
        self.assertIn("GPIOC, GPIO13", led)

    def test_battery_driver_uses_v5_adc_pin_and_original_level_thresholds(self) -> None:
        battery = (SOURCE_DIR / "board_battery.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        runtime = (SOURCE_DIR / "est_runtime.c").read_text(encoding="utf-8")
        self.assertIn("#define BATTERY_ADC_PORT GPIOF", battery)
        self.assertIn("#define BATTERY_ADC_PIN GPIO3", battery)
        self.assertIn("#define BATTERY_ADC ADC3", battery)
        self.assertIn("#define BATTERY_ADC_CHANNEL ADC_CHANNEL9", battery)
        for threshold in ("1661U", "1591U", "1521U", "1451U"):
            self.assertIn(threshold, battery)
        self.assertIn("est_battery_init(est_system_millis());", main)
        self.assertIn("est_battery_tick(now_ms);", runtime)

    def test_main_initializes_power_before_other_board_services(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        power = main.index("est_system_init();")
        led = main.index("est_led_init();")
        usb = main.index("usb_hid_init();")
        self.assertLess(power, led)
        self.assertLess(led, usb)

    def test_key_module_keeps_all_six_documented_pins(self) -> None:
        keys = (SOURCE_DIR / "board_keys.c").read_text(encoding="utf-8")
        for pin in (
            "GPIOC, GPIO14",
            "GPIOC, GPIO15",
            "GPIOF, GPIO0",
            "GPIOE, GPIO3",
            "GPIOF, GPIO1",
            "GPIOE, GPIO4",
        ):
            self.assertIn(pin, keys)
        self.assertIn("KEY_DEBOUNCE_MS 25U", keys)

    def test_lcd_module_uses_official_app_pins_without_usb_conflict(self) -> None:
        lcd = (SOURCE_DIR / "board_lcd.c").read_text(encoding="utf-8")
        for pin in (
            "#define LCD_CLOCK_PORT GPIOD",
            "#define LCD_CLOCK_PIN GPIO14",
            "#define LCD_DATA_PORT GPIOG",
            "#define LCD_DATA_PIN GPIO2",
            "#define LCD_RESET_PORT GPIOD",
            "#define LCD_RESET_PIN GPIO15",
        ):
            self.assertIn(pin, lcd)
        for usb_conflicting_pin in (
            "#define LCD_CLOCK_PORT GPIOB",
            "#define LCD_CLOCK_PIN GPIO13",
        ):
            self.assertNotIn(usb_conflicting_pin, lcd)

    def test_lcd_starts_only_after_interrupts_are_enabled(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        interrupts = main.index("platform_enable_interrupts();")
        lcd_init = main.index("est_display_init();")
        lcd_version = main.index("est_display_text(36U, 54U, app_version_text, 3U)")
        self.assertLess(interrupts, lcd_init)
        self.assertLess(lcd_init, lcd_version)

    def test_lcd_uses_conservative_cold_start_reset_timing(self) -> None:
        lcd = (SOURCE_DIR / "board_lcd.c").read_text(encoding="utf-8")
        for timing in (
            "LCD_RESET_IDLE_MS 50U",
            "LCD_RESET_ASSERT_MS 30U",
            "LCD_RESET_RELEASE_MS 250U",
            "LCD_CONTROLLER_SETTLE_MS 100U",
        ):
            self.assertIn(timing, lcd)
        self.assertIn("lcd_delay_ms(LCD_RESET_IDLE_MS);", lcd)
        self.assertIn("lcd_delay_ms(LCD_RESET_ASSERT_MS);", lcd)
        self.assertIn("lcd_delay_ms(LCD_RESET_RELEASE_MS);", lcd)
        self.assertIn("lcd_delay_ms(LCD_CONTROLLER_SETTLE_MS);", lcd)

    def test_external_flash_write_test_is_guarded_and_recoverable(self) -> None:
        header = (ROOT / "include" / "board_flash.h").read_text(encoding="utf-8")
        source = (SOURCE_DIR / "board_flash.c").read_text(encoding="utf-8")
        self.assertIn("READ_JEDEC_ID_COMMAND 0x9FU", source)
        self.assertIn("board_flash_read_identity", header)
        self.assertIn("READ_DATA_4BYTE_COMMAND 0x13U", source)
        self.assertIn("READ_DATA_COMMAND 0x03U", source)
        self.assertIn("PAGE_PROGRAM_COMMAND 0x02U", source)
        self.assertIn("SECTOR_ERASE_COMMAND 0x20U", source)
        self.assertIn("page_program_in_4byte_mode", source)
        self.assertIn("erase_sector_in_4byte_mode", source)
        self.assertNotIn("PAGE_PROGRAM_4BYTE_COMMAND 0x12U", source)
        self.assertNotIn("SECTOR_ERASE_4BYTE_COMMAND 0x21U", source)
        self.assertIn("board_flash_sector_is_erased_4byte(address)", source)
        self.assertIn("memcmp(alias_before, alias_after", source)
        self.assertIn("erase_sector_in_4byte_mode(address)", source)
        self.assertIn("READ_STATUS_2_COMMAND 0x35U", source)
        self.assertIn("READ_STATUS_3_COMMAND 0x15U", source)
        self.assertIn("board_flash_read_status", header)
        self.assertIn("WRITE_DISABLE_COMMAND 0x04U", source)
        self.assertIn("ENTER_4BYTE_MODE_COMMAND 0xB7U", source)
        self.assertIn("EXIT_4BYTE_MODE_COMMAND 0xE9U", source)
        self.assertIn("board_flash_probe_modes", header)
        self.assertIn("BOARD_FLASH_TEST_WRITE_ENABLE_FAILED", header)
        self.assertIn("BOARD_FLASH_TEST_ENTER_4BYTE_FAILED", header)
        self.assertIn("BOARD_FLASH_TEST_RESTORE_3BYTE_FAILED", header)

    def test_external_flash_is_initialized_before_usb(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertLess(main.index("board_flash_init();"), main.index("usb_hid_init();"))

    def test_motor_a_uses_verified_legacy_pins_and_starts_stopped(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("#define MOTOR_A_PWM_PORT GPIOB", motor)
        self.assertIn("#define MOTOR_A_PWM_PIN GPIO9", motor)
        self.assertIn("#define MOTOR_A_DIRECTION_PORT GPIOG", motor)
        self.assertIn("#define MOTOR_A_DIRECTION_0_PIN GPIO10", motor)
        self.assertIn("#define MOTOR_A_DIRECTION_1_PIN GPIO11", motor)
        self.assertIn("#define MOTOR_A_TACHO_PORT GPIOE", motor)
        self.assertIn("#define MOTOR_A_TACHO_PHASE_PIN GPIO5", motor)
        self.assertIn("#define MOTOR_A_TACHO_DIRECTION_PIN GPIO6", motor)
        self.assertIn("TIM_OC4", motor)
        self.assertIn("MOTOR_PWM_OFF_COMPARE 100U", motor)
        self.assertIn("test_state = BOARD_MOTOR_TEST_IDLE", motor)
        self.assertLess(main.index("board_motor_init();"), main.index("usb_hid_init();"))

    def test_motor_b_uses_verified_legacy_pins_and_separate_timer_channel(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        self.assertIn("#define MOTOR_B_PWM_PORT GPIOB", motor)
        self.assertIn("#define MOTOR_B_PWM_PIN GPIO8", motor)
        self.assertIn("#define MOTOR_B_DIRECTION_PORT GPIOD", motor)
        self.assertIn("#define MOTOR_B_DIRECTION_0_PIN GPIO0", motor)
        self.assertIn("#define MOTOR_B_DIRECTION_1_PIN GPIO1", motor)
        self.assertIn("#define MOTOR_B_TACHO_PORT GPIOE", motor)
        self.assertIn("#define MOTOR_B_TACHO_PHASE_PIN GPIO13", motor)
        self.assertIn("#define MOTOR_B_TACHO_DIRECTION_PIN GPIO14", motor)
        self.assertIn("MOTOR_B_PWM_PORT, MOTOR_B_PWM_PIN, TIM_OC3", motor)
        self.assertIn("exti_select_source(EXTI13, MOTOR_B_TACHO_PORT)", motor)
        self.assertIn("void exti15_10_isr(void)", motor)
        self.assertIn("motor_output_off_all()", motor)

    def test_motor_c_uses_current_v5_schematic_pins(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        self.assertIn("BOARD_MOTOR_PORT_C = 2", header)
        self.assertIn("#define MOTOR_C_PWM_PORT GPIOD", motor)
        self.assertIn("#define MOTOR_C_PWM_PIN GPIO13", motor)
        self.assertIn("#define MOTOR_C_DIRECTION_PORT GPIOG", motor)
        self.assertIn("#define MOTOR_C_DIRECTION_0_PIN GPIO12", motor)
        self.assertIn("#define MOTOR_C_DIRECTION_1_PIN GPIO13", motor)
        self.assertIn("#define MOTOR_C_TACHO_PORT GPIOC", motor)
        self.assertIn("#define MOTOR_C_TACHO_PHASE_PIN GPIO7", motor)
        self.assertIn("#define MOTOR_C_TACHO_DIRECTION_PIN GPIO6", motor)
        self.assertIn("MOTOR_C_PWM_PORT, MOTOR_C_PWM_PIN, TIM_OC2", motor)
        self.assertIn("exti_select_source(EXTI7, MOTOR_C_TACHO_PORT)", motor)
        self.assertIn("count_tacho_edge(BOARD_MOTOR_PORT_C)", motor)

    def test_motor_d_uses_current_v5_schematic_pins(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        self.assertIn("BOARD_MOTOR_PORT_D = 3", header)
        self.assertIn("#define MOTOR_D_PWM_PORT GPIOD", motor)
        self.assertIn("#define MOTOR_D_PWM_PIN GPIO12", motor)
        self.assertIn("#define MOTOR_D_DIRECTION_PORT GPIOA", motor)
        self.assertIn("#define MOTOR_D_DIRECTION_0_PIN GPIO10", motor)
        self.assertIn("#define MOTOR_D_DIRECTION_1_PIN GPIO9", motor)
        self.assertIn("#define MOTOR_D_TACHO_PORT GPIOC", motor)
        self.assertIn("#define MOTOR_D_TACHO_PHASE_PIN GPIO9", motor)
        self.assertIn("#define MOTOR_D_TACHO_DIRECTION_PIN GPIO8", motor)
        self.assertIn("MOTOR_D_PWM_PORT, MOTOR_D_PWM_PIN, TIM_OC1", motor)
        self.assertIn("exti_select_source(EXTI9, MOTOR_D_TACHO_PORT)", motor)
        self.assertIn("count_tacho_edge(BOARD_MOTOR_PORT_D)", motor)

    def test_motor_types_use_official_id_voltage_windows(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("BOARD_MOTOR_TYPE_LARGE = 4", header)
        self.assertIn("BOARD_MOTOR_TYPE_MEDIUM = 5", header)
        self.assertIn("millivolts > 300U && millivolts < 380U", motor)
        self.assertIn("millivolts > 2400U && millivolts < 3000U", motor)
        self.assertIn("millivolts > 400U && millivolts < 480U", motor)
        self.assertIn("millivolts > 1550U && millivolts < 2150U", motor)
        self.assertIn("MOTOR_ID_ADC_AVERAGE_SAMPLES 10U", motor)
        self.assertIn("MOTOR_ID_AUTO_SCAN_INTERVAL_MS 100U", motor)
        self.assertIn("sample <= MOTOR_ID_ADC_AVERAGE_SAMPLES", motor)
        self.assertIn("automatic_identification_next_port", motor)
        for pin in ("GPIOC", "GPIO4", "GPIO5", "GPIOF", "GPIO8", "GPIO9"):
            self.assertIn(pin, motor)
        self.assertIn("MOTOR_TYPE_COMMAND              0x1AU", config)
        self.assertIn("queue_motor_type_result", protocol)
        self.assertIn("MOTOR_TYPE_ACTION_REFRESH", config)
        self.assertIn("board_motor_refresh_identification", motor)
        self.assertIn("MOTOR_ID_REFRESH_PHASE_MS 20U", motor)
        self.assertIn("MOTOR_IDENTIFICATION_REFRESH_FLOAT", motor)
        self.assertIn("MOTOR_IDENTIFICATION_REFRESH_DRIVE_LOW", motor)
        self.assertIn("MOTOR_IDENTIFICATION_REFRESH_PIN5_PULLUP", motor)
        self.assertIn("motor_tacho_direction_float(port)", motor)
        self.assertIn("motor_tacho_direction_drive_low(identification_refresh_port)", motor)
        self.assertIn("motor_tacho_phase_pullup(identification_refresh_port, true)", motor)
        self.assertIn("MOTOR_ID_PULLUP_LARGE_LOW_MV 250U", motor)
        self.assertIn("MOTOR_ID_PULLUP_LARGE_HIGH_MV 360U", motor)
        self.assertIn("MOTOR_ID_PULLUP_MEDIUM_LOW_MV 380U", motor)
        self.assertIn("MOTOR_ID_PULLUP_MEDIUM_HIGH_MV 500U", motor)
        self.assertIn("MOTOR_ID_PULLUP_NONE_LOW_MV 520U", motor)
        self.assertIn("MOTOR_ID_PULLUP_NONE_HIGH_MV 1000U", motor)
        self.assertIn("motor_type_from_probe(identification_refresh_float_mv", motor)
        self.assertIn("motor.id_pin5_pullup_mv", protocol)
        self.assertIn("motor.id_pin5_pullup_high", protocol)
        self.assertIn("report[3] = 57U", protocol)
        self.assertIn("motor_tacho_direction_float(identification_refresh_port)", motor)
        self.assertIn("motor bridge stays off", motor)
        self.assertIn("apply_identified_motor_type", motor)

    def test_motor_hotplug_hides_empty_data_and_resets_reconnected_count(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("automatic_identification_allowed", motor)
        self.assertIn("start_identification_refresh(now_ms", motor)
        self.assertIn("if (motor_type[port_index] != identified_type)", motor)
        self.assertIn("reset_motor_measurements(port, now_ms)", motor)
        self.assertIn("tacho_count[port_index] = 0", motor)
        self.assertIn("measured_speed_percent[port_index] = 0", motor)
        self.assertIn("speed_sample_count[port_index] = 0", motor)
        self.assertIn("snapshot->tacho_count = identification_refresh_tacho_count", motor)
        self.assertIn("snapshot->type == BOARD_MOTOR_TYPE_NONE", main)
        self.assertIn("output[output_index++] = '-'", main)

    def test_motor_test_is_command_only_and_update_forces_stop(self) -> None:
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("MOTOR_TEST_COMMAND", protocol)
        self.assertIn("board_motor_start_test(now_ms)", protocol)
        self.assertIn("board_motor_stop();", protocol)
        self.assertNotIn("board_motor_start_test", main)

    def test_motor_a_tacho_uses_both_edges_and_quadrature_direction(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        self.assertIn("exti_select_source(EXTI5, MOTOR_A_TACHO_PORT)", motor)
        self.assertIn("exti_set_trigger(EXTI5, EXTI_TRIGGER_BOTH)", motor)
        self.assertIn("void exti9_5_isr(void)", motor)
        self.assertIn("if (phase == direction)", motor)
        self.assertIn("tacho_count[(uint8_t)port]--", motor)
        self.assertIn("tacho_count[(uint8_t)port]++", motor)
        self.assertIn("MOTOR_TACHO_TEST_COMMAND", protocol)
        self.assertIn("snapshot.forward_count", protocol)
        self.assertIn("snapshot.reverse_count", protocol)

    def test_high_power_motor_test_is_short_and_has_longer_stop_pause(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        self.assertIn("MOTOR_HIGH_POWER_THRESHOLD_PERCENT 80U", motor)
        self.assertIn("MOTOR_HIGH_POWER_RUN_MS 500U", motor)
        self.assertIn("MOTOR_HIGH_POWER_PAUSE_MS 800U", motor)
        self.assertIn("board_motor_start_test_with_power", motor)
        self.assertIn(
            "data_length == 1U || data_length == 2U || data_length == 3U",
            protocol,
        )
        self.assertIn("power_percent", protocol)
        self.assertIn("board_motor_start_port_test_with_power", protocol)

    def test_motor_stop_comparison_uses_both_legacy_stop_states(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        self.assertIn("BOARD_MOTOR_STOP_LOW_OPEN_DRAIN", header)
        self.assertIn("BOARD_MOTOR_STOP_HIGH_PUSH_PULL", header)
        self.assertIn("motor_output_high_push_pull_stop", motor)
        self.assertIn("MOTOR_STOP_TEST_RUN_MS 600U", motor)
        self.assertIn("MOTOR_STOP_TEST_MEASURE_MS 1500U", motor)
        self.assertIn("motor_apply_stop_mode(test_port, stop_test_mode)", motor)
        self.assertIn("MOTOR_STOP_TEST_COMMAND", protocol)
        self.assertIn("snapshot.stopped_count", protocol)
        self.assertIn(
            "data_length == 1U || data_length == 3U || data_length == 4U",
            protocol,
        )

    def test_dual_motor_test_drives_and_brakes_a_b_together(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        self.assertIn("BOARD_MOTOR_DUAL_TEST_FORWARD", header)
        self.assertIn("BOARD_MOTOR_DUAL_TEST_FINAL_BRAKE", header)
        self.assertIn("board_motor_start_dual_test", motor)
        self.assertIn("motor_output_forward(BOARD_MOTOR_PORT_A, test_compare", motor)
        self.assertIn("motor_output_forward(BOARD_MOTOR_PORT_B, test_compare", motor)
        self.assertIn("motor_output_reverse(BOARD_MOTOR_PORT_A, test_compare", motor)
        self.assertIn("motor_output_reverse(BOARD_MOTOR_PORT_B, test_compare", motor)
        self.assertIn("dual_a_forward_count", motor)
        self.assertIn("dual_b_reverse_count", motor)
        self.assertIn("MOTOR_DUAL_TEST_COMMAND", protocol)
        self.assertIn("snapshot.a_forward_count", protocol)
        self.assertIn("snapshot.b_reverse_count", protocol)

    def test_general_motor_control_keeps_old_commands_and_exposes_all_ports(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("MOTOR_CONTROL_COMMAND           0x17U", config)
        self.assertIn("BOARD_MOTOR_OUTPUT_COAST", header)
        self.assertIn("BOARD_MOTOR_OUTPUT_DRIVE", header)
        self.assertIn("BOARD_MOTOR_OUTPUT_BRAKE", header)
        self.assertIn("board_motor_set_power", motor)
        self.assertIn("board_motor_coast", motor)
        self.assertIn("board_motor_brake", motor)
        self.assertIn("board_motor_reset_tacho", motor)
        self.assertIn("power_percent < -100", motor)
        self.assertNotIn("board_motor_diagnostic_active()", protocol)
        self.assertIn("port_control_active(port)", motor)
        self.assertIn("queue_motor_control_result", protocol)
        self.assertIn("write_i32_le(&report[9], snapshot.tacho_count)", protocol)

    def test_motor_position_uses_official_large_and_medium_profiles(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("MOTOR_POSITION_COMMAND          0x1BU", config)
        self.assertIn("DEVICE_PROTOCOL_MINOR           15U", config)
        self.assertIn("MOTOR_LARGE_COUNTS_PER_SPEED 12800U", motor)
        self.assertIn("MOTOR_MEDIUM_COUNTS_PER_SPEED 8100U", motor)
        self.assertIn("medium_samples[4] = {2U, 4U, 8U, 16U}", motor)
        self.assertIn("large_samples[4] = {4U, 16U, 32U, 64U}", motor)
        self.assertIn("speed_error * 8", motor)
        self.assertIn("MOTOR_SPEED_CONTROL_INTERVAL_MS 10U", motor)
        self.assertIn("motor_position_slowdown_degrees", motor)
        self.assertIn("MOTOR_POSITION_MAX_DEGREES 3600", motor)
        self.assertIn("BOARD_MOTOR_POSITION_TIMEOUT", header)
        self.assertIn("board_motor_start_position", motor)
        self.assertIn("handle_motor_position", protocol)
        self.assertIn("snapshot.target_count - snapshot.current_count", protocol)

    def test_motor_speed_reuses_type_aware_closed_loop_and_safe_stops(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("MOTOR_SPEED_COMMAND             0x1CU", config)
        self.assertIn("MOTOR_SPEED_ACTION_COAST", config)
        self.assertIn("MOTOR_SPEED_ACTION_BRAKE", config)
        self.assertIn("BOARD_MOTOR_SPEED_RUNNING", header)
        self.assertIn("board_motor_start_speed", motor)
        self.assertIn("apply_closed_loop_speed", motor)
        self.assertIn("speed_error * 8", motor)
        self.assertIn("MOTOR_SPEED_MIN_PERCENT 10", motor)
        self.assertIn("board_motor_stop_speed", motor)
        self.assertIn("EST_STOP_COAST", protocol)
        self.assertIn("EST_STOP_BRAKE", protocol)
        self.assertIn("queue_motor_speed_result", protocol)

    def test_motor_closed_loop_state_is_independent_for_all_four_ports(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        start_position = motor.split("bool board_motor_start_position", 1)[1].split(
            "bool board_motor_stop_position", 1
        )[0]
        start_speed = motor.split("bool board_motor_start_speed", 1)[1].split(
            "bool board_motor_stop_speed", 1
        )[0]

        self.assertIn("position_controls[BOARD_MOTOR_PORT_COUNT]", motor)
        self.assertIn("speed_controls[BOARD_MOTOR_PORT_COUNT]", motor)
        self.assertIn("update_position_control_port(now_ms", motor)
        self.assertIn("apply_closed_loop_speed(port,", motor)
        self.assertIn(
            "pair_adjust_continuous_speed(port, control->requested_speed)", motor
        )
        self.assertNotIn("motor_output_off_all()", start_position)
        self.assertNotIn("motor_output_off_all()", start_speed)
        self.assertIn("board_motor_stop_position", header)
        self.assertIn("board_motor_position_snapshot_for_port", header)
        self.assertIn("board_motor_speed_snapshot_for_port", header)
        self.assertIn("queue_motor_position_result(result, port)", protocol)
        self.assertIn("queue_motor_speed_result(result, port)", protocol)

    def test_motor_pair_position_unwinds_overspeed_without_limiting_torque(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("MOTOR_PAIR_POSITION_COMMAND     0x1DU", config)
        self.assertIn("DEVICE_CAPABILITY_MOTOR_PAIR_POSITION", config)
        self.assertIn("MOTOR_PAIR_SYNC_COUNTS_PER_PERCENT 4", motor)
        self.assertIn("MOTOR_PAIR_SYNC_MAX_CORRECTION_PERCENT 10", motor)
        self.assertIn("MOTOR_PAIR_LEADER_DEADBAND_COUNTS 4", motor)
        self.assertIn("MOTOR_PAIR_SPEED_FOLLOW_TOLERANCE_PERCENT 1", motor)
        self.assertIn("MOTOR_PAIR_OVERSPEED_TOLERANCE_PERCENT 10", motor)
        self.assertIn("MOTOR_PAIR_OVERSPEED_CONTROL_GAIN 32", motor)
        self.assertIn("speed_measurement_valid[BOARD_MOTOR_PORT_COUNT]", motor)
        self.assertIn("pair_adjust_position_speed", motor)
        self.assertIn("pair_position_scaled_progress", motor)
        self.assertIn("pair_position_equivalent_speed", motor)
        self.assertIn("pair_position_scaled_max_speed", motor)
        self.assertIn("magnitude -= correction", motor)
        self.assertIn("pair_position_equivalent_speed(peer_port, port)", motor)
        self.assertIn("magnitude > speed_limit", motor)
        self.assertIn("(MOTOR_PAIR_OVERSPEED_CONTROL_GAIN - 8)", motor)
        self.assertNotIn("MOTOR_PAIR_LAG_RECOVERY_MARGIN_PERCENT", motor)
        self.assertNotIn("*pwm_x100 = pwm_limit", motor)
        self.assertIn("leader_port = BOARD_MOTOR_PORT_COUNT", motor)
        self.assertIn("if (target_speed == 0)", motor)
        pair_position_start = motor[
            motor.index("bool board_motor_start_pair_position") :
            motor.index("bool board_motor_stop_pair_position")
        ]
        self.assertNotIn("left_magnitude != right_magnitude", pair_position_start)
        self.assertIn("left_speed_percent", pair_position_start)
        self.assertIn("right_speed_percent", pair_position_start)
        self.assertIn("control->timeout_ms != 0U", motor)
        self.assertIn("position_controls[(uint8_t)left_port].timeout_ms = 0U", motor)
        self.assertIn("position_controls[(uint8_t)right_port].timeout_ms = 0U", motor)
        pair_state = motor[motor.index("static void update_pair_position_state") :
                           motor.index("void board_motor_init")]
        self.assertNotIn("BOARD_MOTOR_POSITION_TIMEOUT", pair_state)
        self.assertNotIn("motor_output_off", pair_state)
        self.assertIn("board_motor_start_pair_position", drive)
        self.assertIn("return EST_ERR_NOT_SUPPORTED", drive)
        self.assertIn("handle_motor_pair_position", protocol)
        self.assertIn("maximum_synchronization_error_degrees", protocol)

    def test_motor_pair_speed_runs_until_explicit_stop(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("MOTOR_PAIR_SPEED_COMMAND        0x1EU", config)
        self.assertIn("DEVICE_CAPABILITY_MOTOR_PAIR_SPEED", config)
        self.assertIn("BOARD_MOTOR_PAIR_SPEED_RUNNING", header)
        self.assertIn("board_motor_start_pair_speed", header)
        self.assertIn("board_motor_stop_pair_speed", header)
        self.assertIn("pair_adjust_continuous_speed", motor)
        self.assertIn("update_pair_speed_correction", motor)
        self.assertIn("pair_speed_scaled_progress", motor)
        self.assertIn("pair_speed_equivalent_speed", motor)
        self.assertIn("pair_speed_scaled_correction", motor)
        self.assertIn("est_motor_pair_run_speeds", drive)
        self.assertIn("est_motor_pair_stop", drive)
        self.assertIn("handle_motor_pair_speed", protocol)
        self.assertIn("MOTOR_PAIR_SPEED_ACTION_BRAKE", protocol)
        pair_speed_start = motor[motor.index("bool board_motor_start_pair_speed") :
                                 motor.index("bool board_motor_stop_pair_speed")]
        self.assertNotIn("timeout", pair_speed_start)
        self.assertNotIn("left_magnitude != right_magnitude", pair_speed_start)

    def test_drive_straight_converts_millimeters_in_firmware(self) -> None:
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "est_drive.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("DRIVE_STRAIGHT_COMMAND          0x1FU", config)
        self.assertIn("DEVICE_CAPABILITY_DRIVE_STRAIGHT", config)
        self.assertIn("EST_DRIVE_PI_NUMERATOR 355LL", drive)
        self.assertIn("EST_DRIVE_PI_DENOMINATOR 113LL", drive)
        self.assertIn("distance_to_wheel_degrees", drive)
        self.assertIn("wheel_degrees_to_distance", drive)
        self.assertIn("est_motor_pair_run_angles(drive_config.left_port", drive)
        self.assertIn("target_distance_mm", header)
        self.assertIn("actual_distance_mm", header)
        self.assertIn("handle_drive_straight", protocol)
        self.assertIn("queue_drive_straight_result", protocol)
        self.assertIn("data_length == 13U", protocol)

    def test_drive_run_supports_degrees_and_firmware_timed_seconds(self) -> None:
        motor = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        motor_header = (ROOT / "include" / "board_motor.h").read_text(
            encoding="utf-8"
        )
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        drive_header = (ROOT / "include" / "est_drive.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("DRIVE_RUN_COMMAND               0x20U", config)
        self.assertIn("DEVICE_CAPABILITY_DRIVE_RUN", config)
        self.assertIn("est_drive_run_degrees", drive_header)
        self.assertIn("est_drive_run_time", drive_header)
        self.assertIn("est_drive_get_motion_status", drive_header)
        self.assertIn("board_motor_start_pair_speed_for_time", motor_header)
        self.assertIn("MOTOR_PAIR_TIMED_MAX_DURATION_MS 600000U", motor)
        self.assertIn("update_pair_speed_state(now_ms)", motor)
        self.assertIn("BOARD_MOTOR_PAIR_SPEED_COMPLETE", motor)
        self.assertIn("est_motor_pair_run_angles(left_port, degrees", drive)
        self.assertIn("board_motor_start_pair_speed_for_time", drive)
        self.assertIn("handle_drive_run", protocol)
        self.assertIn("DRIVE_RUN_MODE_DEGREES", protocol)
        self.assertIn("DRIVE_RUN_MODE_TIME_MS", protocol)

    def test_drive_steer_uses_ev3_classroom_speed_mix(self) -> None:
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        drive_header = (ROOT / "include" / "est_drive.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("DRIVE_STEER_COMMAND             0x21U", config)
        self.assertIn("DEVICE_CAPABILITY_DRIVE_STEER", config)
        self.assertIn("est_drive_mix_steering", drive_header)
        self.assertIn("est_drive_start_steer", drive_header)
        self.assertIn("steering == 100 || steering == -100", drive)
        self.assertIn("left_raw = clamp_percent(100 + steering)", drive)
        self.assertIn("right_raw = clamp_percent(100 - steering)", drive)
        self.assertIn("(int64_t)left_raw * speed_percent, 100LL", drive)
        self.assertIn("closed_loop_speed_supported", drive)
        self.assertIn("est_motor_pair_run_speeds(left_port, left_speed", drive)
        self.assertIn("handle_drive_steer", protocol)
        self.assertIn("queue_motor_pair_speed_result(DRIVE_STEER_COMMAND", protocol)
        self.assertIn("data_length == 5U", protocol)

    def test_drive_steer_for_reuses_mix_for_degrees_and_firmware_time(self) -> None:
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        drive_header = (ROOT / "include" / "est_drive.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

        self.assertIn("DRIVE_STEER_FOR_COMMAND         0x22U", config)
        self.assertIn("DEVICE_CAPABILITY_DRIVE_STEER_FOR", config)
        self.assertIn("est_drive_steer_for", drive_header)
        self.assertIn("est_drive_get_steer_for_status", drive_header)
        self.assertIn("est_drive_mix_steering(steering, speed_percent", drive)
        self.assertIn("scale_steering_degrees", drive)
        self.assertIn("est_motor_pair_run_angles(left_port, left_degrees", drive)
        self.assertIn("board_motor_start_pair_speed_for_time", drive)
        self.assertIn("handle_drive_steer_for", protocol)
        self.assertIn("queue_drive_steer_for_result", protocol)
        self.assertIn("data_length == 11U", protocol)

    def test_input_port_one_uses_verified_v5_sensor_pins(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        expected_macros = (
            "#define SENSOR_A_ADC0_PORT GPIOF",
            "#define SENSOR_A_ADC0_PIN GPIO4",
            "#define SENSOR_A_ADC1_PORT GPIOF",
            "#define SENSOR_A_ADC1_PIN GPIO5",
            "#define SENSOR_A_POWER_PORT GPIOF",
            "#define SENSOR_A_POWER_PIN GPIO6",
            "#define SENSOR_A_DIGITAL0_PORT GPIOB",
            "#define SENSOR_A_DIGITAL0_PIN GPIO3",
            "#define SENSOR_A_DIGITAL1_PORT GPIOF",
            "#define SENSOR_A_DIGITAL1_PIN GPIO7",
            "#define SENSOR_A_LEGACY_DETECT_PORT GPIOB",
            "#define SENSOR_A_LEGACY_DETECT_PIN GPIO4",
            "#define SENSOR_A_UART_PORT GPIOA",
            "#define SENSOR_A_UART_TX_PIN GPIO0",
            "#define SENSOR_A_UART_RX_PIN GPIO1",
            "#define SENSOR_A_UART_ENABLE_PORT GPIOG",
            "#define SENSOR_A_UART_ENABLE_PIN GPIO15",
        )
        for macro in expected_macros:
            self.assertIn(macro, sensor)

    def test_color_sensor_driver_uses_ev3_uart_handshake_and_modes(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        runtime = (SOURCE_DIR / "est_runtime.c").read_text(encoding="utf-8")
        self.assertIn("SENSOR_SYNC_BAUD 2400U", sensor)
        self.assertIn("SENSOR_DEFAULT_DATA_BAUD 57600U", sensor)
        self.assertIn("BOARD_SENSOR_TYPE_EV3_COLOR 0x1DU", header)
        self.assertIn("SENSOR_SYSTEM_ACK 0x04U", sensor)
        self.assertIn("SENSOR_SYSTEM_NACK 0x02U", sensor)
        self.assertIn("SENSOR_SELECT_MODE 0x43U", sensor)
        self.assertIn("BOARD_SENSOR_MODE_REFLECTED", header)
        self.assertIn("BOARD_SENSOR_MODE_AMBIENT", header)
        self.assertIn("BOARD_SENSOR_MODE_COLOR", header)
        self.assertLess(main.index("board_sensor_init"), main.index("usb_hid_init"))
        self.assertIn("board_sensor_tick(now_ms);", runtime)

    def test_touch_sensor_preserves_legacy_adc_thresholds_and_shows_state(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_TOUCH 0x10U", header)
        self.assertIn("SENSOR_TOUCH_ID_MIN_RAW 82U", sensor)
        self.assertIn("SENSOR_TOUCH_ID_MAX_RAW 656U", sensor)
        self.assertIn("SENSOR_TOUCH_PRESS_MAX_RAW 655U", sensor)
        self.assertIn("SENSOR_TOUCH_RELEASE_MIN_RAW 1229U", sensor)
        self.assertIn("update_touch_detection(port, now_ms);", sensor)
        self.assertIn('return snapshot->value != 0U ? "DOWN" : "UP";', main)

    def test_ultrasonic_sensor_uses_existing_uart_modes_and_formats_distance(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_ULTRASONIC 0x1EU", header)
        self.assertIn(
            "runtime->rx_message[1] == BOARD_SENSOR_TYPE_ULTRASONIC", sensor
        )
        self.assertIn("BOARD_SENSOR_MODE_DISTANCE_CM", header)
        self.assertIn("BOARD_SENSOR_MODE_DISTANCE_INCH", header)
        self.assertIn("BOARD_SENSOR_MODE_PRESENCE", header)
        self.assertIn(
            'format_tenths((int16_t)snapshot->value, "CM", formatted);', main
        )
        self.assertIn(
            'format_tenths((int16_t)snapshot->value, "IN", formatted);', main
        )
        self.assertIn('return snapshot->value != 0U ? "YES" : "NO";', main)
        for label in ("DIST CM", "DIST IN", "PRESENCE"):
            self.assertIn(f'"{label}"', main)

    def test_temperature_sensor_uses_original_i2c_protocol_and_signed_units(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_TEMPERATURE 0x06U", header)
        self.assertIn("SENSOR_I2C_ADDRESS 0x4CU", sensor)
        self.assertIn("SENSOR_I2C_IDENTIFY_REGISTER 0x01U", sensor)
        self.assertIn("SENSOR_I2C_IDENTIFY_VALUE 0x60U", sensor)
        self.assertIn("SENSOR_I2C_TEMPERATURE_REGISTER 0x00U", sensor)
        self.assertIn("SENSOR_I2C_DELAY_CYCLES 1000U", sensor)
        self.assertIn("SENSOR_I2C_SWITCH_SETTLE_MS 20U", sensor)
        self.assertIn("SENSOR_I2C_DETECT_ADC0_MIN_RAW 656U", sensor)
        self.assertIn("SENSOR_I2C_DETECT_ADC1_MIN_RAW 984U", sensor)
        self.assertIn("temperature_candidate(&runtime->snapshot)", sensor)
        self.assertIn("gpio_set(hardware->enable_port, hardware->enable_pin);", sensor)
        self.assertIn("temperature_probe(port)", sensor)
        self.assertIn("((uint16_t)response[0] << 4U)", sensor)
        self.assertIn("raw_value | 0xF000U", sensor)
        self.assertIn("* 10) / 16", sensor)
        self.assertIn("* 18) / 16 + 320", sensor)
        self.assertIn("BOARD_SENSOR_MODE_CELSIUS", header)
        self.assertIn("BOARD_SENSOR_MODE_FAHRENHEIT", header)
        self.assertIn('"TEMP C"', main)
        self.assertIn('"TEMP F"', main)
        self.assertIn('? "F" : "C"', main)
        for pin in (
            "SENSOR_A_DIGITAL0_PIN GPIO3",
            "SENSOR_A_DIGITAL1_PIN GPIO7",
            "SENSOR_B_DIGITAL0_PIN GPIO13",
            "SENSOR_B_DIGITAL1_PIN GPIO11",
            "SENSOR_C_DIGITAL0_PIN GPIO1",
            "SENSOR_C_DIGITAL1_PIN GPIO0",
            "SENSOR_D_DIGITAL0_PIN GPIO11",
            "SENSOR_D_DIGITAL1_PIN GPIO12",
        ):
            self.assertIn(pin, sensor)

    def test_gyro_sensor_uses_type_0x20_and_signed_angle_rate_values(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_GYRO 0x20U", header)
        self.assertIn("BOARD_SENSOR_TYPE_GYRO", sensor)
        self.assertIn("mode > BOARD_SENSOR_MODE_GYRO_RATE", sensor)
        self.assertIn("format_i32((int32_t)(int16_t)snapshot->value", main)

    def test_sound_sensor_uses_legacy_detect_and_original_adc_scale(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_SOUND 0x03U", header)
        self.assertIn("(snapshot->digital_mask & 0x04U) == 0U", sensor)
        self.assertIn("(uint32_t)raw * 5000U / 4096U", sensor)
        self.assertIn("100U - millivolts / 14U", sensor)
        self.assertLess(
            sensor.index("update_sound_detection(port, now_ms);"),
            sensor.index("update_touch_detection(port, now_ms);"),
        )
        self.assertIn('return "SOUND DB";', main)

    def test_infrared_sensor_uses_type_0x21_and_three_uart_modes(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("BOARD_SENSOR_TYPE_INFRARED 0x21U", header)
        self.assertIn("BOARD_SENSOR_MODE_IR_PROXIMITY", header)
        self.assertIn("BOARD_SENSOR_MODE_IR_BEACON", header)
        self.assertIn("BOARD_SENSOR_MODE_IR_REMOTE", header)
        self.assertIn("BOARD_SENSOR_TYPE_INFRARED", sensor)
        self.assertIn('"IR PROX", "IR BEACON", "IR REMOTE"', main)
        self.assertIn('return "NO BEACON";', main)

    def test_sensor_command_is_additive_and_update_stops_sensor(self) -> None:
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("UPDATE_COMMAND                  0x05U", config)
        self.assertIn("MOTOR_CONTROL_COMMAND           0x17U", config)
        self.assertIn("INPUT_SENSOR_COMMAND            0x18U", config)
        self.assertIn("handle_input_sensor", protocol)
        self.assertIn("queue_input_sensor_result", protocol)
        self.assertIn("est_sensor_set_mode", protocol)
        self.assertIn("est_sensor_restart", protocol)
        self.assertIn("est_system_emergency_stop();", protocol)

    def test_device_status_is_additive_and_returns_all_runtime_subsystems(self) -> None:
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        self.assertIn("DEVICE_STATUS_COMMAND           0x19U", config)
        self.assertIn("DEVICE_PROTOCOL_MAJOR           1U", config)
        self.assertIn("DEVICE_STATUS_PAYLOAD_LENGTH 72U", protocol)
        self.assertIn("est_battery_get_status(&battery)", protocol)
        self.assertIn("est_buttons_pressed_mask()", protocol)
        self.assertIn("board_motor_control_snapshot", protocol)
        self.assertIn("est_sensor_get_status", protocol)
        self.assertIn("queue_device_status(now_ms);", protocol)
        self.assertIn("UPDATE_COMMAND                  0x05U", config)

    def test_sensor_screen_shows_live_modes_and_cycles_with_a_key(self) -> None:
        lcd = (SOURCE_DIR / "board_lcd.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_lcd.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertIn("board_lcd_show_sensor", header)
        self.assertIn("board_lcd_show_sensor_ports", header)
        self.assertIn("board_lcd_show_io_ports", header)
        self.assertIn("board_lcd_show_io_ports", main)
        self.assertIn("format_motor_line", main)
        self.assertIn('"MTR S% DEG REV"', lcd)
        self.assertIn("LCD_MOTOR_COLUMN_X 72U", lcd)
        self.assertIn("BOARD_LCD_MOTOR_LINE_CHARACTERS 18U", header)
        self.assertIn("motor display column exceeds the LCD width", lcd)
        self.assertIn("snapshot->speed_percent", main)
        self.assertIn("rotation_tenths", main)
        self.assertIn(
            "MOTOR_DISPLAY_LINE_SIZE (BOARD_LCD_MOTOR_LINE_CHARACTERS + 1U)",
            main,
        )
        self.assertIn("format_status_line", main)
        self.assertIn('append_status_text(output, &output_index, "BAT:");', main)
        self.assertIn('append_status_text(output, &output_index, " SND:");', main)
        self.assertIn("glyph_percent", lcd)
        self.assertIn("draw_text_centered(116U, status, 1U);", lcd)
        self.assertIn("BOARD_SENSOR_PORT_COUNT", main)
        self.assertIn("board_sensor_set_all_modes", main)
        self.assertIn('draw_text_centered(112U, "ANY KEY", 1U);', lcd)
        for label in ("REFLECT", "AMBIENT", "COLOR"):
            self.assertIn(f'"{label}"', main)
        for color in ("NONE", "BLACK", "BLUE", "GREEN", "YELLOW", "RED", "WHITE", "BROWN"):
            self.assertIn(f'"{color}"', main)
        self.assertIn("key_mask != 0U && last_key_mask == 0U", main)
        self.assertIn("SENSOR_DISPLAY_INTERVAL_MS 100U", main)

    def test_startup_exercises_backlight_and_audio_without_blocking_main_loop(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        runtime = (SOURCE_DIR / "est_runtime.c").read_text(encoding="utf-8")
        backlight = (SOURCE_DIR / "board_backlight.c").read_text(encoding="utf-8")
        audio = (SOURCE_DIR / "board_audio.c").read_text(encoding="utf-8")
        self.assertIn("est_backlight_set_percent(20U);", main)
        self.assertIn("est_backlight_set_percent(0U);", main)
        self.assertIn("est_backlight_set_percent(100U);", main)
        self.assertIn("board_audio_start_test(now_ms)", main)
        self.assertIn("board_audio_tick(now_ms);", runtime)
        self.assertIn("BACKLIGHT_PORT GPIOA", backlight)
        self.assertIn("BACKLIGHT_PIN GPIO8", backlight)
        self.assertIn("timer_set_oc_value(TIM1, TIM_OC1", backlight)
        self.assertIn("timer_enable_break_main_output(TIM1);", backlight)
        for pin in (
            "AUDIO_COMMAND_SELECT_PIN GPIO4",
            "AUDIO_DREQ_PIN GPIO5",
            "AUDIO_RESET_PIN GPIO6",
            "AUDIO_DATA_SELECT_PIN GPIO7",
        ):
            self.assertIn(pin, audio)
        self.assertIn("AUDIO_TEST_STREAMING", audio)
        self.assertIn("AUDIO_TEST_DRAINING", audio)
        self.assertIn("AUDIO_VOLUME_TEST 0x0000U", audio)
        self.assertIn("AUDIO_PCM_SAMPLE_RATE 8000U", audio)
        self.assertIn("AUDIO_PCM_SAMPLE_COUNT 40000U", audio)
        self.assertIn("'R', 'I', 'F', 'F'", audio)
        self.assertIn("'W', 'A', 'V', 'E'", audio)
        self.assertIn("send_stream_chunk();", audio)
        self.assertIn("AUDIO_STREAM_CHUNK_SIZE 32U", audio)
        self.assertIn("spi_set_clock_polarity_1", audio)
        self.assertIn("restore_flash_bus();", audio)

    def test_sensor_uart_uses_rx_interrupt_buffer_during_lcd_refresh(self) -> None:
        sensor = (SOURCE_DIR / "board_sensor.c").read_text(encoding="utf-8")
        self.assertIn("SENSOR_RX_RING_SIZE 256U", sensor)
        self.assertIn("usart_enable_rx_interrupt(hardware->uart);", sensor)
        for irq in (
            "NVIC_UART4_IRQ",
            "NVIC_USART2_IRQ",
            "NVIC_USART1_IRQ",
            "NVIC_USART3_IRQ",
        ):
            self.assertIn(irq, sensor)
        for handler in (
            "void uart4_isr(void)",
            "void usart2_isr(void)",
            "void usart1_isr(void)",
            "void usart3_isr(void)",
        ):
            self.assertIn(handler, sensor)
        self.assertIn("while (runtime->rx_tail != runtime->rx_head)", sensor)


class MicroPythonIntegrationTests(unittest.TestCase):
    def test_micropython_is_pinned_and_built_as_a_separate_library(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        port_makefile = (ROOT / "micropython_port" / "Makefile").read_text(
            encoding="utf-8"
        )
        gitmodules = (ROOT.parents[1] / ".gitmodules").read_text(encoding="utf-8")

        self.assertIn("third_party/micropython", gitmodules)
        self.assertIn("https://github.com/micropython/micropython.git", gitmodules)
        self.assertIn("micropython-lib", makefile)
        self.assertIn("libest_micropython.a", makefile)
        self.assertIn("py/mkenv.mk", port_makefile)
        self.assertIn("-mfloat-abi=hard", port_makefile)

    def test_startup_self_test_is_bounded_and_never_starts_a_motor(self) -> None:
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("EST_MICROPYTHON_HEAP_SIZE (48U * 1024U)", runtime)
        self.assertIn("for i in range(96)", runtime)
        self.assertIn("assert est.drive_mix(50, 40) == (40, 20)", runtime)
        self.assertIn("assert est.force_gc() >= 0", runtime)
        self.assertNotIn("est_motor_run", runtime)
        self.assertNotIn("est_drive_start", runtime)

    def test_vm_exception_cleanup_and_gc_metrics_are_recorded(self) -> None:
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("nlr_push(&nlr)", runtime)
        self.assertIn("EST_MICROPYTHON_EXCEPTION", runtime)
        self.assertIn("(void)est_system_cleanup();", runtime)
        self.assertIn("gc_helper_collect_regs_and_stack();", runtime)
        self.assertIn("micropython_status.maximum_gc_pause_us", runtime)

    def test_est_module_uses_public_service_apis(self) -> None:
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        for service_call in (
            "est_system_millis()",
            "est_motor_get_type",
            "est_motor_stop_all",
            "est_drive_mix_steering",
            "est_sensor_get_status",
        ):
            self.assertIn(service_call, module)
        self.assertIn("MP_REGISTER_MODULE(MP_QSTR_est, mp_module_est)", module)
        self.assertNotIn("board_motor_", module)
        self.assertNotIn("board_sensor_", module)

    def test_vm_starts_after_interrupts_and_deinitializes_before_poweroff(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        self.assertLess(
            main.index("platform_enable_interrupts();"),
            main.index("est_micropython_init();"),
        )
        self.assertLess(
            main.index("est_micropython_deinit();"),
            main.index("est_system_power_off();"),
        )
        self.assertIn('append_status_text(output, &output_index, " PY:");', main)

    def test_protocol_exposes_micropython_health_status(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        self.assertIn("MICROPYTHON_STATUS_COMMAND      0x23U", config)
        self.assertIn("DEVICE_PROTOCOL_MINOR           15U", config)
        self.assertIn("DEVICE_CAPABILITY_MICROPYTHON", config)
        self.assertIn("MICROPYTHON_STATUS_PAYLOAD_LENGTH 28U", protocol)
        self.assertIn("queue_micropython_status", protocol)
        self.assertIn("status.maximum_gc_pause_us", protocol)
        self.assertIn("status.self_test_value", protocol)

    def test_ram_python_program_is_crc_checked_bounded_and_interruptible(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        header = (ROOT / "include" / "est_micropython.h").read_text(
            encoding="utf-8"
        )
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        port = (ROOT / "micropython_port" / "mpconfigport.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")

        self.assertIn("PYTHON_PROGRAM_COMMAND          0x24U", config)
        self.assertIn("DEVICE_CAPABILITY_PYTHON_PROGRAM", config)
        self.assertIn("EST_MICROPYTHON_PROGRAM_MAX_SIZE 8192U", header)
        self.assertIn("crc32_bytes", runtime)
        self.assertIn("program_deadline_ms", runtime)
        self.assertIn("program_stop_requested", runtime)
        self.assertIn("mp_raise_msg(&mp_type_RuntimeError", runtime)
        self.assertIn("usb_hid_poll();", runtime)
        self.assertIn("est_micropython_program_is_executing", protocol)
        self.assertIn("logical_frame[2] != PYTHON_PROGRAM_COMMAND", protocol)
        self.assertIn("MICROPY_VM_HOOK_LOOP est_micropython_vm_hook();", port)
        self.assertIn("PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH 32U", protocol)
        self.assertIn("est_micropython_program_write", protocol)
        self.assertIn("est_micropython_tick();", main)
        self.assertLess(
            main.index("est_runtime_tick(now_ms);"),
            main.index("est_micropython_tick();"),
        )

    def test_runtime_service_tick_is_shared_by_main_and_vm(self) -> None:
        service = (SOURCE_DIR / "est_runtime.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "est_runtime.h").read_text(encoding="utf-8")
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )

        for service_call in (
            "est_backlight_tick(now_ms);",
            "board_audio_tick(now_ms);",
            "est_buttons_tick(now_ms);",
            "board_motor_tick(now_ms);",
            "board_sensor_tick(now_ms);",
            "est_battery_tick(now_ms);",
            "update_protocol_tick(now_ms);",
        ):
            self.assertIn(service_call, service)
        self.assertIn("now_ms == runtime_status.last_tick_ms", service)
        self.assertIn("runtime_status.maximum_gap_ms", service)
        self.assertIn("est_runtime_status_t", header)
        self.assertIn("est_runtime_init(est_system_millis());", main)
        self.assertIn("est_runtime_tick(now_ms);", main)
        self.assertIn("est_runtime_tick(now_ms);", runtime)
        self.assertIn("MP_QSTR__runtime_ticks", module)

    def test_formal_read_only_python_api_wraps_public_services(self) -> None:
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "read_services.py"
        ).read_text(encoding="utf-8")

        for public_api in (
            "est_sensor_get_status",
            "est_buttons_pressed_mask",
            "est_button_is_pressed",
            "est_battery_get_status",
        ):
            self.assertIn(public_api, module)
        for python_name in (
            "MP_QSTR_Sensor",
            "MP_QSTR_buttons",
            "MP_QSTR_battery",
            "MP_QSTR_pressed",
            "MP_QSTR_percent",
        ):
            self.assertIn(python_name, module)
        self.assertIn("est.Sensor(port)", script)
        self.assertIn("est.buttons.pressed(button)", script)
        self.assertIn("est.battery.percent()", script)
        self.assertNotIn("board_sensor_", module)
        self.assertNotIn("board_keys_", module)
        self.assertNotIn("board_battery_", module)

    def test_typed_sensor_python_api_validates_modes_and_waits_for_data(self) -> None:
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        sensor_source = (SOURCE_DIR / "est_sensor.c").read_text(
            encoding="utf-8"
        )
        script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "test_typed_sensors.py"
        ).read_text(encoding="utf-8")

        for class_name in (
            "SoundSensor",
            "TemperatureSensor",
            "TouchSensor",
            "ColorSensor",
            "UltrasonicSensor",
            "GyroSensor",
            "InfraredSensor",
        ):
            self.assertIn(f"MP_QSTR_{class_name}", module)
            self.assertIn(f"est.{class_name}", script)
        for public_call in (
            "est_sensor_get_status",
            "est_sensor_set_mode",
            "est_sensor_restart",
            "est_micropython_vm_hook",
        ):
            self.assertIn(public_call, module)
        for method in (
            "MP_QSTR_set_mode",
            "MP_QSTR_read_mode",
            "MP_QSTR_restart",
            "MP_QSTR_reflection",
            "MP_QSTR_pressed",
            "MP_QSTR_distance_mm",
            "MP_QSTR_angle",
            "MP_QSTR_reset_angle",
            "MP_QSTR_proximity",
            "MP_QSTR_beacon",
            "MP_QSTR_remote",
        ):
            self.assertIn(method, module)
        self.assertIn("est_sensor_mode_supported", sensor_source)
        self.assertIn("case EST_SENSOR_TYPE_SOUND:", sensor_source)
        self.assertIn("case EST_SENSOR_TYPE_GYRO:", sensor_source)
        self.assertIn("return EST_ERR_NOT_SUPPORTED;", sensor_source)
        self.assertIn("sound.set_mode(1)", script)
        self.assertIn("gyro.reset_angle()", script)
        self.assertIn("est.ColorSensor(1)", script)
        self.assertNotIn("board_sensor_", module)

    def test_motor_python_api_wraps_single_motor_services(self) -> None:
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        read_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "read_motors.py"
        ).read_text(encoding="utf-8")
        run_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "run_single_motor.py"
        ).read_text(encoding="utf-8")
        exception_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "motor_exception.py"
        ).read_text(encoding="utf-8")
        stop_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "motor_until_stopped.py"
        ).read_text(encoding="utf-8")
        medium_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "run_medium_motor.py"
        ).read_text(encoding="utf-8")
        timed_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "run_timed_motor.py"
        ).read_text(encoding="utf-8")
        timed_medium_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "run_timed_medium_motor.py"
        ).read_text(encoding="utf-8")
        timed_cancel_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "cancel_timed_motor.py"
        ).read_text(encoding="utf-8")
        timed_exception_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "timed_motor_exception.py"
        ).read_text(encoding="utf-8")

        self.assertIn("MP_QSTR_Motor", module)
        self.assertIn("est_motor_get_status", module)
        self.assertIn("est_motor_stop(self->port, stop_mode)", module)
        for method in (
            "MP_QSTR_port",
            "MP_QSTR_type",
            "MP_QSTR_state",
            "MP_QSTR_power",
            "MP_QSTR_target_speed",
            "MP_QSTR_speed",
            "MP_QSTR_angle",
            "MP_QSTR_status",
            "MP_QSTR_stop",
        ):
            self.assertIn(method, module)
        for public_call in (
            "est_motor_set_power",
            "est_motor_run_speed",
            "est_motor_run_time",
            "est_motor_run_angle",
            "est_motor_reset_angle",
        ):
            self.assertIn(public_call, module)
        for python_method in (
            "MP_QSTR_run_power",
            "MP_QSTR_run_speed",
            "MP_QSTR_run_time",
            "MP_QSTR_run_angle",
            "MP_QSTR_reset_angle",
        ):
            self.assertIn(python_method, module)
        self.assertIn('est.Motor("A")', read_script)
        self.assertIn("motor.stop()", read_script)
        self.assertIn("motor.stop(motor.STOP_BRAKE)", read_script)
        self.assertIn("motor.run_power(20)", run_script)
        self.assertIn("motor.run_speed(20)", run_script)
        self.assertIn("motor.run_angle(degrees=90, speed=30)", run_script)
        self.assertIn("est.force_gc()", run_script)
        self.assertIn("raise RuntimeError", exception_script)
        self.assertIn("while True:", stop_script)
        self.assertIn('est.Motor("D")', medium_script)
        self.assertIn("degrees=-90", medium_script)
        self.assertIn("duration_ms=1200", timed_script)
        self.assertIn("motor.STATE_TIMED", timed_script)
        self.assertIn("motor.STOP_BRAKE", timed_script)
        self.assertIn("est.force_gc()", timed_script)
        self.assertIn('est.Motor("D")', timed_medium_script)
        self.assertIn("motor.run_time(800", timed_medium_script)
        self.assertIn("motor.run_time(5000", timed_cancel_script)
        self.assertIn("motor.stop()", timed_cancel_script)
        self.assertIn("raise RuntimeError", timed_exception_script)
        self.assertIn('assert est.Motor(\\"A\\").port() == \\"A\\"', runtime)
        self.assertNotIn("board_motor_", module)

    def test_motor_pair_and_drive_base_python_api_use_precise_c_services(self) -> None:
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        drive_header = (ROOT / "include" / "est_drive.h").read_text(
            encoding="utf-8"
        )
        drive_source = (SOURCE_DIR / "est_drive.c").read_text(
            encoding="utf-8"
        )
        script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "test_motor_pair_drive.py"
        ).read_text(encoding="utf-8")
        stop_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "stop_motor_pair.py"
        ).read_text(encoding="utf-8")
        exception_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "motor_pair_exception.py"
        ).read_text(encoding="utf-8")
        host_stop_script = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "examples" /
            "motor_pair_until_stopped.py"
        ).read_text(encoding="utf-8")

        self.assertIn("est_motor_pair_run_speeds_for_time", drive_header)
        self.assertIn("left_type != right_type", drive_source)
        self.assertIn("require_matching_tacho_motors", drive_source)
        self.assertIn("position_stopped = board_motor_stop_pair_position", drive_source)
        self.assertIn("speed_stopped = board_motor_stop_pair_speed", drive_source)
        for public_call in (
            "est_motor_pair_run_angles",
            "est_motor_pair_run_speeds",
            "est_motor_pair_run_speeds_for_time",
            "est_motor_pair_stop",
            "est_drive_run_degrees",
            "est_drive_run_time",
            "est_drive_start_steer",
            "est_drive_steer_for",
            "est_micropython_vm_hook",
        ):
            self.assertIn(public_call, module)
        for python_name in (
            "MP_QSTR_MotorPair",
            "MP_QSTR_DriveBase",
            "MP_QSTR_run_speed",
            "MP_QSTR_run_time",
            "MP_QSTR_run_angle",
            "MP_QSTR_straight_angle",
            "MP_QSTR_straight_time",
            "MP_QSTR_steer",
            "MP_QSTR_steer_angle",
            "MP_QSTR_steer_time",
            "MP_QSTR_wait",
        ):
            self.assertIn(python_name, module)
        self.assertIn('est.MotorPair("A", "C")', script)
        self.assertIn('est.DriveBase("A", "C")', script)
        self.assertIn('est.MotorPair("A", "D")', script)
        self.assertIn("left_speed=80", script)
        self.assertIn("est.force_gc()", script)
        self.assertIn("wait=True", script)
        self.assertIn("pair.stop(pair.STOP_BRAKE)", stop_script)
        self.assertIn("raise RuntimeError", exception_script)
        self.assertIn("while True:", host_stop_script)
        self.assertNotIn("board_motor_", module)

    def test_ram_program_exit_always_stops_motors_and_failures_cleanup(self) -> None:
        runtime = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        tick = runtime.split("void est_micropython_tick(void)", 1)[1].split(
            "bool est_micropython_get_status", 1
        )[0]
        self.assertGreaterEqual(tick.count("est_motor_stop_all(EST_STOP_COAST)"), 2)
        self.assertIn("program_cleanup_after_failure();", tick)
        self.assertIn("(void)est_system_cleanup();", runtime)
        self.assertIn("restart_sensors();", runtime)


if __name__ == "__main__":
    unittest.main()
