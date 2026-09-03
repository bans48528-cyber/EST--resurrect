"""Index verified device piano MP3s; optionally embed the audited Hello sample."""

import argparse
import hashlib
import math
from pathlib import Path

HELLO_SHA256 = "63c6356edf88ecc6e5726136fd44db6076cf1f0e3f21a019bd0a3027d9479449"
PIANO_LENGTHS = (
    6366, 6679, 7097, 6679, 6366, 6470, 6470, 6157, 6470, 6261, 6575, 6157,
    6157, 5948, 6052, 5843, 6261, 6575, 6261, 6993, 6784, 4381, 4590, 5112,
    5217, 4903, 4172, 3649, 6157, 6366, 3127, 2918, 3858, 3858, 5634, 5425, 2918,
)
PIANO_DURATIONS_MS = (
    1491, 1569, 1674, 1569, 1491, 1517, 1517, 1439, 1517, 1465, 1543, 1439,
    1439, 1387, 1413, 1360, 1465, 1543, 1465, 1648, 1596, 995, 1047, 1178,
    1204, 1125, 943, 812, 1439, 1491, 681, 629, 864, 864, 1308, 1256, 629,
)


def generate(source=None):
    lines = ['#include <stddef.h>', '#include "audio_resources.h"', '']
    if source is not None:
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != HELLO_SHA256:
            raise ValueError("Hello.mp3 differs from the audited source")
        lines.append('static const uint8_t hello_mp3[] = {')
        for offset in range(0, len(data), 16):
            lines.append('    ' + ', '.join(f'0x{byte:02x}' for byte in data[offset:offset + 16]) + ',')
        lines.append('};')
    lines.append('const struct audio_resource audio_resources[] = {')
    if source is not None:
        lines.append('    {"communication_hello", hello_mp3, sizeof(hello_mp3), 792U, 0U},')
    notes = ('C', 'Cs', 'D', 'Ds', 'E', 'F', 'Fs', 'G', 'Gs', 'A', 'As', 'B')
    for index, (length, duration) in enumerate(zip(PIANO_LENGTHS, PIANO_DURATIONS_MS)):
        name = notes[index % 12] + str(4 + index // 12)
        address = 0x01F40000 + index * 0x1C00
        lines.append(f'    {{"Piano/{name}", NULL, {length}U, {duration}U, 0x{address:08X}U}},')
    lines += ['};', 'const uint32_t audio_resource_count = sizeof(audio_resources) / sizeof(audio_resources[0]);',
              'const uint32_t audio_note_phase_steps[128] = {']
    for note in range(128):
        step = round(440 * 2 ** ((note - 69) / 12) * 2 ** 32 / 32000)
        lines.append(f'    {step}U,')
    lines += ['};', 'const int16_t audio_sine_samples[64] = {']
    lines += [f'    {round(12000 * math.sin(index * 2 * math.pi / 64))},'
              for index in range(64)]
    return '\n'.join(lines + ['};', ''])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = generate(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='ascii') != text:
        args.output.write_text(text, encoding='ascii')


if __name__ == '__main__':
    main()
