"""Decode the original ZIP's MP3 files and produce a reproducible inventory."""

import argparse
import hashlib
import io
import json
import zipfile
from pathlib import Path

import av


def audit(archive):
    resources = []
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            if not entry.filename.lower().endswith(".mp3"):
                continue
            name = "/".join(entry.filename.replace("\\", "/").split("/")[-2:])
            data = source.read(entry)
            with av.open(io.BytesIO(data), format="mp3") as container:
                stream = container.streams.audio[0]
                samples = 0
                nonzero = False
                for frame in container.decode(stream):
                    samples += frame.samples
                    nonzero |= any(any(bytes(plane)) for plane in frame.planes)
                if not samples or not nonzero:
                    raise ValueError("empty/silent resource: " + name)
                resources.append({
                    "name": name,
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "codec": stream.codec_context.name,
                    "sample_rate": stream.codec_context.sample_rate,
                    "channels": stream.codec_context.channels,
                    "samples": samples,
                    "duration_ms": round(samples * 1000 / stream.codec_context.sample_rate),
                    "decoded": True,
                })
    return resources


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--extract-hello", type=Path)
    args = parser.parse_args()
    resources = audit(args.archive)
    report = {"archive": args.archive.name, "count": len(resources),
              "total_bytes": sum(item["bytes"] for item in resources),
              "resources": resources}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.extract_hello:
        with zipfile.ZipFile(args.archive) as source:
            entry = next(item for item in source.infolist()
                         if item.filename.endswith("/Communication/Hello.mp3"))
            args.extract_hello.parent.mkdir(parents=True, exist_ok=True)
            args.extract_hello.write_bytes(source.read(entry))
    print(json.dumps({"count": report["count"], "bytes": report["total_bytes"],
                      "hello": next(item for item in resources
                                    if item["name"] == "Communication/Hello.mp3")}))


if __name__ == "__main__":
    main()
