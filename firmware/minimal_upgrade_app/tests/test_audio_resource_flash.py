from __future__ import annotations

import sys
import unittest
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))


class AudioResourceFlashTests(unittest.TestCase):
    def test_audio_resource_region_sits_before_program_slots(self) -> None:
        header = (ROOT / "include" / "est_audio_resource_store.h").read_text(
            encoding="utf-8"
        )
        source = (ROOT / "src" / "est_audio_resource_store.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("EST_AUDIO_RESOURCE_FLASH_SIZE 33554432U", header)
        self.assertIn("EST_AUDIO_RESOURCE_REGION_START 0x01B40000U", header)
        self.assertIn("EST_AUDIO_RESOURCE_REGION_SIZE 0x00400000U", header)
        self.assertIn("EST_AUDIO_RESOURCE_SLOT_SIZE 0x00008000U", header)
        self.assertIn("EST_AUDIO_RESOURCE_SLOT_COUNT", header)
        self.assertIn("EST_AUDIO_RESOURCE_DATA_MAX_BYTES", header)
        self.assertIn("<=\n\t0x01FD0000U", source)
        self.assertIn("EST_AUDIO_RESOURCE_DATA_MAX_BYTES <= 0xFFFFU", source)
        self.assertIn("AUDIO_RESOURCE_HEADER_MAGIC \"EAUD\"", source)
        self.assertIn("AUDIO_RESOURCE_COMMIT_MAGIC \"DONE\"", source)
        self.assertIn("slot < 32U && read_header(slot, &header)", source)

    def test_all_known_mp3_resources_fit_expanded_slots(self) -> None:
        header = (ROOT / "include" / "est_audio_resource_store.h").read_text(
            encoding="utf-8"
        )
        inventory = json.loads(
            (ROOT.parents[1] / "docs" / "audio_resources_inventory_2026-09-03.json")
            .read_text(encoding="utf-8")
        )
        sizes = [item["bytes"] for item in inventory["resources"]]
        self.assertEqual(len(sizes), 127)
        self.assertLessEqual(len(sizes), 128)
        self.assertLessEqual(max(sizes), 32768 - 128)
        self.assertIn("(EST_AUDIO_RESOURCE_REGION_SIZE / EST_AUDIO_RESOURCE_SLOT_SIZE)", header)

    def test_audio_resource_upload_commits_header_after_crc_validation(self) -> None:
        source = (ROOT / "src" / "est_audio_resource_store.c").read_text(
            encoding="utf-8"
        )
        write_header = source.index("static bool write_header")
        commit = source.index("est_audio_resource_commit")
        crc_check = source.index("crc_matches(slot_id, upload_session.resource_length", commit)
        header_write = source.index("write_header(&upload_session)", commit)
        self.assertLess(crc_check, header_write)
        self.assertGreater(
            source.index("AUDIO_RESOURCE_COMMIT_OFFSET", write_header),
            write_header,
        )
        self.assertIn(
            "received_length != upload_session.resource_length",
            source,
        )
        self.assertIn("resource_crc32", source)

    def test_protocol_exposes_external_audio_resource_command(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        protocol = (ROOT / "src" / "update_protocol.c").read_text(encoding="utf-8")
        constants = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "constants.py"
        ).read_text(encoding="utf-8")
        for token in (
            "FLASH_AUDIO_RESOURCE_COMMAND    0x27U",
            "DEVICE_PROTOCOL_MINOR           28U",
            "DEVICE_CAPABILITY_AUDIO_RESOURCE_FLASH (1UL << 27U)",
            "FLASH_AUDIO_RESOURCE_ACTION_STATUS 0x00U",
            "FLASH_AUDIO_RESOURCE_ACTION_BEGIN 0x01U",
            "FLASH_AUDIO_RESOURCE_ACTION_CHUNK 0x02U",
            "FLASH_AUDIO_RESOURCE_ACTION_COMMIT 0x03U",
            "FLASH_AUDIO_RESOURCE_ACTION_CLEAR 0x04U",
        ):
            self.assertIn(token, config)
        self.assertIn('#include "est_audio_resource_store.h"', protocol)
        self.assertIn("FLASH_AUDIO_RESOURCE_COMMAND", protocol)
        self.assertIn("AUDIO_RESOURCE_STATUS_PAYLOAD_LENGTH 88U", protocol)
        self.assertIn("handle_audio_resource", protocol)
        self.assertIn("board_audio_stop();", protocol)
        self.assertIn("est_audio_resource_tick(now_ms);", protocol)
        self.assertIn("DEVICE_CAPABILITY_AUDIO_RESOURCE_FLASH", protocol)
        self.assertIn("FLASH_AUDIO_RESOURCE_COMMAND = 0x27", constants)
        self.assertIn("DEVICE_PROTOCOL_MINOR = 28", constants)
        self.assertIn("DEVICE_CAPABILITY_AUDIO_RESOURCE_FLASH = 1 << 27", constants)

    def test_board_audio_can_resolve_flash_resources_without_removing_piano(self) -> None:
        audio = (ROOT / "src" / "board_audio.c").read_text(encoding="utf-8")
        self.assertIn('#include "est_audio_resource_store.h"', audio)
        self.assertIn("audio_resources[index].name", audio)
        self.assertIn("est_audio_resource_find(name, &flash_resource", audio)
        self.assertIn("return &flash_resource;", audio)
        self.assertIn("board_flash_read_4byte(resource->flash_address", audio)

    def test_resource_directory_is_cached_before_ui_starts(self) -> None:
        header = (ROOT / "include" / "est_audio_resource_store.h").read_text(
            encoding="utf-8"
        )
        source = (ROOT / "src" / "est_audio_resource_store.c").read_text(
            encoding="utf-8"
        )
        main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        self.assertIn("void est_audio_resource_init(void);", header)
        self.assertIn("resource_catalog[EST_AUDIO_RESOURCE_SLOT_COUNT]", source)
        self.assertIn("ensure_resource_catalog();", source)
        find = source.index("bool est_audio_resource_find")
        tick = source.index("void est_audio_resource_tick")
        find_body = source[find:tick]
        self.assertIn("resource_catalog_valid[slot]", find_body)
        self.assertNotIn("read_header(slot", find_body)
        self.assertLess(
            main.index("est_audio_resource_init();"),
            main.index("est_ui_init(est_system_millis());"),
        )


if __name__ == "__main__":
    unittest.main()
