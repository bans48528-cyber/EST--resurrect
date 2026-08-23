import copy
import importlib.util
import unittest
from pathlib import Path


APP_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = APP_DIR.parents[1]
CHECKER_PATH = APP_DIR / "tools" / "check_hardware_pins.py"
SPEC = importlib.util.spec_from_file_location("check_hardware_pins", CHECKER_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("unable to load hardware pin checker")
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class HardwarePinCheckTests(unittest.TestCase):
    def test_current_repository_matches_verified_pin_table(self) -> None:
        package_count, gpio_count, claim_count = CHECKER.validate_repository(REPO_ROOT)
        self.assertEqual(package_count, 144)
        self.assertEqual(gpio_count, 112)
        self.assertEqual(claim_count, 50)

    def test_duplicate_gpio_pin_is_rejected(self) -> None:
        rows = CHECKER.load_pin_table(REPO_ROOT / CHECKER.PIN_TABLE_RELATIVE)
        changed = copy.deepcopy(rows)
        changed[1]["mcu_pin"] = changed[0]["mcu_pin"]
        with self.assertRaisesRegex(ValueError, "GPIO pin conflict"):
            CHECKER.validate_table(changed)

    def test_firmware_claim_conflict_is_rejected(self) -> None:
        rows = CHECKER.load_pin_table(REPO_ROOT / CHECKER.PIN_TABLE_RELATIVE)
        net_to_pin = CHECKER.validate_table(rows)
        net_to_pin["LCD_SCK"] = net_to_pin["ULPI_D6"]
        with self.assertRaisesRegex(ValueError, "firmware GPIO conflict"):
            CHECKER.validate_claim_conflicts(net_to_pin)


if __name__ == "__main__":
    unittest.main()
