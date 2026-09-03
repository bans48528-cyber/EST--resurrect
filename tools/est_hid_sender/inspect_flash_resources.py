"""Read existing EST FAT resources over HID. No device write/erase operations."""

import argparse
import hashlib
import io
import json
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.est_hid_sender.protocol import build_frame
from tools.est_hid_sender.updater import FirmwareUpdater

FLASH_SIZE = 32 * 1024 * 1024
FLASH_READ_COMMAND = 0x26


def read_flash(transport, address, length):
    if not (0 < length <= 1000 and 0 <= address <= FLASH_SIZE - length):
        raise ValueError("invalid flash read range")
    transport.write_report(build_frame(FLASH_READ_COMMAND, struct.pack("<IH", address, length)).ljust(64, b"\x00"))
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        report = transport.read_report(250)
        if not report or report[:3] != b"\x68\x21\x26":
            continue
        payload_length = int.from_bytes(report[3:5], "little")
        if payload_length != length + 7:
            raise ValueError("unexpected flash response payload size")
        end = 5 + payload_length
        if len(report) < end + 2 or report[end + 1] != 0x16:
            raise ValueError("invalid flash response length")
        if (sum(report[:end]) & 255) != report[end]:
            raise ValueError("flash response checksum mismatch")
        status, actual_address, actual_length = struct.unpack("<BIH", report[5:12])
        if status != 1 or actual_address != address or actual_length != length:
            raise IOError("flash read rejected or response mismatch")
        return bytes(report[12:12 + length])
    raise TimeoutError("read-only Flash command timed out")


class FlashReader(io.RawIOBase):
    def __init__(self, transport, cache_directory):
        super().__init__()
        self.transport = transport
        self.position = 0
        self.cache = {}
        self.cache_directory = cache_directory
        cache_directory.mkdir(parents=True, exist_ok=True)

    def readable(self):
        return True

    def writable(self):
        return False

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        if whence not in (0, 1, 2):
            raise ValueError("invalid seek origin")
        position = offset + (self.position if whence == 1 else FLASH_SIZE if whence == 2 else 0)
        if not 0 <= position <= FLASH_SIZE:
            raise ValueError("seek outside physical Flash")
        self.position = position
        return position

    def write(self, data):
        raise io.UnsupportedOperation("device Flash is strictly read-only")

    def read(self, size=-1):
        if size < 0 or size > 2 * 1024 * 1024:
            raise ValueError("unbounded reads are not allowed")
        size = min(size, FLASH_SIZE - self.position)
        result = bytearray()
        while len(result) < size:
            base = self.position & ~4095
            offset = self.position - base
            if base not in self.cache:
                data = b"".join(read_flash(self.transport, base + part, min(1000, 4096 - part))
                                for part in range(0, 4096, 1000))
                self.cache[base] = data
                (self.cache_directory / f"{base:08x}.bin").write_bytes(data)
            count = min(size - len(result), 4096 - offset)
            result.extend(self.cache[base][offset:offset + count])
            self.position += count
        return bytes(result)


def inspect(transport, output, offset=0):
    from pyfatfs.PyFat import PyFat
    reader = FlashReader(transport, output / "sectors")
    reader.seek(offset)
    boot = reader.read(512)
    print("boot_prefix=" + boot[:64].hex(), flush=True)
    filesystem = PyFat(encoding="gbk", offset=offset, lazy_load=True)
    filesystem.set_fp(reader)
    entries = []
    visited = set()

    def visit(directory):
        if len(visited) >= 256 or len(entries) >= 4096:
            raise ValueError("directory inspection limit exceeded")
        identity = directory.get_cluster()
        if identity in visited:
            raise ValueError("directory cycle")
        visited.add(identity)
        directories, files, _ = directory.get_entries()
        for item in files:
            entry = {"path": item.get_full_path(), "bytes": item.get_size(),
                     "cluster": item.get_cluster()}
            if item.get_size() and item.get_full_path().lower().endswith(".mp3"):
                if item.get_size() > 2 * 1024 * 1024:
                    raise ValueError("audio file exceeds inspection limit")
                clusters = []
                seen = set()
                for cluster in filesystem.get_cluster_chain(item.get_cluster()):
                    if cluster in seen or len(clusters) >= 512:
                        raise ValueError("invalid or excessive cluster chain")
                    seen.add(cluster)
                    clusters.append(cluster)
                data = b"".join(filesystem.read_cluster_contents(cluster) for cluster in clusters)
                data = data[:item.get_size()]
                entry["sha256"] = hashlib.sha256(data).hexdigest()
                entry["clusters"] = clusters
                entry["first_address"] = offset + filesystem.get_data_cluster_address(clusters[0])
                target = output / "sounds" / item.get_full_path().lstrip("/\\")
                if not target.resolve().is_relative_to((output / "sounds").resolve()):
                    raise ValueError("unsafe resource path")
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                print(json.dumps(entry, ensure_ascii=True), flush=True)
            entries.append(entry)
        for directory in directories:
            visit(directory)

    visit(filesystem.root_dir)
    result = {"flash_size": FLASH_SIZE, "partition_offset": offset, "fat_type": filesystem.fat_type,
              "bpb": dict(filesystem.bpb_header), "entries": entries}
    # BPB labels are byte strings; retain their hex representation in the report.
    (output / "inventory.json").write_text(json.dumps(result, indent=2, default=lambda v: v.hex()) + "\n",
                                           encoding="utf-8")
    filesystem.close()
    print("files=" + str(len(entries)), flush=True)


def inspect_piano(transport, output):
    import av
    reader = FlashReader(transport, output / "sectors")
    entries = []
    notes = ("C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B")
    for index in range(37):
        address = 0x01F40000 + index * 0x1C00
        reader.seek(address)
        slot = reader.read(0x1C00)
        data = slot.rstrip(b"\xff")
        name = notes[index % 12] + str(4 + index // 12)
        target = output / "piano" / (name + ".mp3")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        entry = {"name": "Piano/" + name, "address": address,
                 "address_hex": f"0x{address:08X}", "bytes": len(data),
                 "sha256": hashlib.sha256(data).hexdigest(), "decoded": False}
        try:
            with av.open(io.BytesIO(data), format="mp3") as container:
                stream = container.streams.audio[0]
                samples = sum(frame.samples for frame in container.decode(stream))
                entry.update(decoded=samples > 0, samples=samples,
                             sample_rate=stream.codec_context.sample_rate,
                             channels=stream.codec_context.channels,
                             duration_ms=round(samples * 1000 / stream.codec_context.sample_rate))
        except Exception as error:
            entry["error"] = str(error)
        entries.append(entry)
        print(json.dumps(entry), flush=True)
    (output / "piano_inventory.json").write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")


def main():
    from tools.est_hid_sender.hid_transport import HidTransport
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--wait-seconds", type=int, default=60)
    parser.add_argument("--scan", action="store_true")
    parser.add_argument("--piano", action="store_true")
    parser.add_argument("--partition-offset", type=lambda v: int(v, 0), default=0)
    args = parser.parse_args()
    deadline = time.monotonic() + args.wait_seconds
    while True:
        try:
            transport = HidTransport.open()
            break
        except OSError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(2)
    with transport:
        updater = FirmwareUpdater(transport)
        version = updater.ping()
        print("firmware=" + version, flush=True)
        if version != "M1.22Z" and not version.startswith("M1.23"):
            raise RuntimeError("read-only probe firmware required")
        if args.scan:
            args.output.mkdir(parents=True, exist_ok=True)
            entries = []
            for address in range(0, FLASH_SIZE, 4096):
                data = read_flash(transport, address, 96)
                if data != b"\xff" * len(data):
                    entries.append({"address": address, "hex": data.hex(),
                                    "text": ''.join(chr(v) if 32 <= v < 127 else '.' for v in data)})
                if address % 1048576 == 0:
                    print(f"scanned={address // 1048576}/32MiB nonblank={len(entries)}", flush=True)
            (args.output / "sector_header_scan.json").write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
            print("nonblank_headers=" + str(len(entries)), flush=True)
        elif args.piano:
            inspect_piano(transport, args.output)
        else:
            inspect(transport, args.output, args.partition_offset)


if __name__ == "__main__":
    main()
