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

    def test_main_initializes_power_before_other_board_services(self) -> None:
        main = (SOURCE_DIR / "main.c").read_text(encoding="utf-8")
        power = main.index("board_power_init();")
        led = main.index("board_led_init();")
        clock = main.index("system_time_init();")
        usb = main.index("usb_hid_init();")
        self.assertLess(power, led)
        self.assertLess(led, clock)
        self.assertLess(clock, usb)

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
        lcd_init = main.index("board_lcd_init();")
        lcd_version = main.index("board_lcd_show_version(app_version_text);")
        self.assertLess(interrupts, lcd_init)
        self.assertLess(lcd_init, lcd_version)

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
        self.assertIn("board_motor_diagnostic_active()", protocol)
        self.assertIn("queue_motor_control_result", protocol)
        self.assertIn("write_i32_le(&report[9], snapshot.tacho_count)", protocol)


if __name__ == "__main__":
    unittest.main()
