#!/usr/bin/env python3
"""Convert the experimental Z12 packed 12-bit Bayer stream to uncompressed DNG files."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

Z12_MAGIC = 0x5236325A
FRAME_MAGIC = 0x46323152
TYPE_SIZES = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 10: 8}


def pack_values(kind, values):
    if isinstance(values, (bytes, bytearray)):
        return bytes(values)
    if isinstance(values, str):
        return values.encode('ascii') + b'\0'
    if isinstance(values, (int, np.integer)):
        values = [values]
    values = list(values)
    if kind == 1:
        return bytes(values)
    if kind == 2:
        raise ValueError('ASCII value must be str')
    if kind in (3, 4):
        return struct.pack('<' + ('H' if kind == 3 else 'I') * len(values), *values)
    if kind in (5, 10):
        fmt = '<' + ('I' if kind == 5 else 'i') * (2 * len(values))
        return struct.pack(fmt, *(x for pair in values for x in pair))
    raise ValueError(f'Unsupported TIFF field type {kind}')


def write_dng(path, raw, force=False):
    if path.exists() and not force:
        raise FileExistsError(f'{path} exists; use --force to replace it')
    height, width = raw.shape
    if width % 2 or height % 2:
        raise ValueError('Bayer dimensions must be even')
    pixels = raw.astype('<u2', copy=False).tobytes()
    entries = [
        (254, 4, 1, 0),
        (256, 4, 1, width),
        (257, 4, 1, height),
        (258, 3, 1, 16),
        (259, 3, 1, 1),
        (262, 3, 1, 32803),
        (271, 2, 0, 'NIKON CORPORATION'),
        (272, 2, 0, 'NIKON Z 6_2 (RAW12 probe)'),
        (273, 4, 1, 0),
        (274, 3, 1, 1),
        (277, 3, 1, 1),
        (278, 4, 1, height),
        (279, 4, 1, len(pixels)),
        (284, 3, 1, 1),
        (33421, 3, 2, (2, 2)),
        (33422, 1, 4, bytes((0, 1, 1, 2))),
        (50706, 1, 4, bytes((1, 4, 0, 0))),
        (50707, 1, 4, bytes((1, 1, 0, 0))),
        (50708, 2, 0, 'NIKON Z 6_2 RAW12'),
        (50710, 1, 3, bytes((0, 1, 2))),
        (50711, 3, 1, 1),
        (50717, 4, 1, 4095),
        (50721, 10, 9, ((1, 1), (0, 1), (0, 1), (0, 1), (1, 1), (0, 1), (0, 1), (0, 1), (1, 1))),
        (50778, 3, 1, 21),
        (50829, 4, 4, (0, 0, height, width)),
        (50830, 4, 2, (4, 4)),
        (50831, 4, 2, (width - 8, height - 8)),
    ]
    prepared = []
    external = bytearray()
    extra_base = 8 + 2 + len(entries) * 12 + 4
    for tag, kind, count, value in entries:
        if kind == 2:
            count = len(value) + 1
        payload = pack_values(kind, value)
        if len(payload) != count * TYPE_SIZES[kind]:
            raise ValueError(f'Bad TIFF field size for tag {tag}')
        if len(payload) <= 4:
            stored = payload + bytes(4 - len(payload))
        else:
            offset = extra_base + len(external)
            external += payload
            external += bytes(-len(external) % 4)
            stored = struct.pack('<I', offset)
        prepared.append([tag, kind, count, stored])
    prefix = bytearray(struct.pack('<2sHI', b'II', 42, 8))
    prefix += struct.pack('<H', len(prepared))
    for tag, kind, count, stored in prepared:
        prefix += struct.pack('<HHI', tag, kind, count) + stored
    prefix += struct.pack('<I', 0)
    prefix += external
    pixel_offset = (len(prefix) + 63) & ~63
    prefix += bytes(pixel_offset - len(prefix))
    for index, entry in enumerate(prepared):
        if entry[0] == 273:
            offset = 8 + 2 + index * 12 + 8
            struct.pack_into('<I', prefix, offset, pixel_offset)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(prefix + pixels)


def read_z12(data):
    if len(data) < 80:
        raise ValueError('Z12 is too short')
    header = struct.unpack_from('<16I', data, 0)
    if header[0] != Z12_MAGIC or header[1] != 1 or header[2] != 64 or header[15] != (~Z12_MAGIC & 0xFFFFFFFF):
        raise ValueError('Invalid Z12 header')
    width, height, bits, row_bytes, frame_bytes = header[3], header[4], header[5], header[6], header[7]
    if bits != 12 or width % 2 or height % 2 or row_bytes < (width * 12 + 7) // 8:
        raise ValueError('Unsupported Z12 geometry')
    offset = 64
    frames = []
    sequences = []
    while offset < len(data):
        if offset + 16 > len(data):
            raise ValueError('Truncated Z12 frame header')
        magic, sequence, size, source = struct.unpack_from('<4I', data, offset)
        offset += 16
        if magic != FRAME_MAGIC or size != frame_bytes or offset + size > len(data):
            raise ValueError('Invalid Z12 frame record')
        payload = data[offset:offset + size]
        offset += size
        rows = np.frombuffer(payload, dtype=np.uint8).reshape(height, row_bytes)
        used = (width * 12 + 7) // 8
        packed = rows[:, :used].reshape(height, width // 2, 3).astype(np.uint16)
        raw = np.empty((height, width), dtype=np.uint16)
        raw[:, 0::2] = packed[:, :, 0] | ((packed[:, :, 1] & 15) << 8)
        raw[:, 1::2] = (packed[:, :, 1] >> 4) | (packed[:, :, 2] << 4)
        frames.append(raw)
        sequences.append(sequence)
    return header, frames, sequences


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    data = args.input.read_bytes()
    header, frames, sequences = read_z12(data)
    output = args.output or args.input.with_suffix('.dng')
    outputs = []
    for index, raw in enumerate(frames):
        path = output if len(frames) == 1 else output.with_name(f'{output.stem}-{index:04d}{output.suffix}')
        write_dng(path, raw, args.force)
        outputs.append(path)
    report = {
        'input': str(args.input), 'input_sha256': hashlib.sha256(data).hexdigest(),
        'frames': len(frames), 'sequences': sequences,
        'width': header[3], 'height': header[4], 'source_bits': 12, 'dng_bits': 16,
        'fps': [header[8], header[9]], 'cfa': 'RGGB',
        'color_matrix': 'identity (not calibrated)',
        'outputs': [str(path) for path in outputs],
        'output_sha256': [hashlib.sha256(path.read_bytes()).hexdigest() for path in outputs],
    }
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
