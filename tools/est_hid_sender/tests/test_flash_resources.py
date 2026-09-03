import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.est_hid_sender import inspect_flash_resources as probe


def response(address, data, status=1):
    frame = bytearray(probe.build_frame(0x26, struct.pack("<BIH", status, address, len(data)) + data))
    frame[1] = 0x21
    frame[-2] = sum(frame[:-2]) & 255
    return bytes(frame)


class FlashResourceReadTests(unittest.TestCase):
    def test_read_is_bounded_and_only_sends_read_command(self):
        transport = Mock()
        transport.read_report.return_value = response(0x01F40000, b"ID3")
        self.assertEqual(probe.read_flash(transport, 0x01F40000, 3), b"ID3")
        sent = transport.write_report.call_args.args[0]
        self.assertEqual(sent[2], 0x26)
        self.assertEqual(sent[5:11], struct.pack("<IH", 0x01F40000, 3))
        self.assertEqual(len(sent), 64)

    def test_invalid_range_never_contacts_device(self):
        transport = Mock()
        for address, length in ((-1, 1), (0, 0), (0, 1001), (probe.FLASH_SIZE, 1),
                                (probe.FLASH_SIZE - 1, 2)):
            with self.subTest(address=address, length=length), self.assertRaises(ValueError):
                probe.read_flash(transport, address, length)
        transport.write_report.assert_not_called()

    def test_end_of_flash_is_readable(self):
        transport = Mock()
        transport.read_report.return_value = response(probe.FLASH_SIZE - 1, b"x")
        self.assertEqual(probe.read_flash(transport, probe.FLASH_SIZE - 1, 1), b"x")

    def test_rejects_corrupt_or_mismatched_response(self):
        valid = response(0, b"abc")
        corrupt = bytearray(valid)
        corrupt[-2] ^= 1
        for frame in (bytes(corrupt), valid[:-1], response(0, b"abcd"),
                      response(1, b"abc"), response(0, b"abc", status=0)):
            with self.subTest(frame=frame):
                transport = Mock()
                transport.read_report.return_value = frame
                with self.assertRaises((ValueError, OSError)):
                    probe.read_flash(transport, 0, 3)

    def test_timeout_is_bounded(self):
        transport = Mock()
        transport.read_report.return_value = b""
        with patch.object(probe.time, "monotonic", side_effect=(0, 0, 4)):
            with self.assertRaises(TimeoutError):
                probe.read_flash(transport, 0, 3)

    def test_reader_is_read_only_and_reuses_sectors(self):
        with tempfile.TemporaryDirectory() as temporary:
            reader = probe.FlashReader(Mock(), Path(temporary))
            self.assertFalse(reader.writable())
            with self.assertRaises(io.UnsupportedOperation):
                reader.write(b"data")
            with patch.object(probe, "read_flash", side_effect=lambda t, a, n: b"x" * n) as read:
                reader.seek(4094)
                self.assertEqual(reader.read(4), b"xxxx")
                self.assertEqual(read.call_count, 10)
                reader.seek(4094)
                self.assertEqual(reader.read(4), b"xxxx")
                self.assertEqual(read.call_count, 10)
            with self.assertRaises(ValueError):
                reader.seek(-1)
            with self.assertRaises(ValueError):
                reader.seek(0, 3)
            with self.assertRaises(ValueError):
                reader.read()


if __name__ == "__main__":
    unittest.main()
