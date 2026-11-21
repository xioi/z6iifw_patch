#!/usr/bin/env python3
"""Decode FOCUSPROP.BIN produced by the Z62 focus-property diagnostic module."""
import argparse
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    parser.add_argument('--all', action='store_true', help='show unchanged properties too')
    args = parser.parse_args()
    data = args.file.read_bytes()
    if len(data) not in (1056, 5152):
        raise SystemExit(f'Expected 1056 or 5152 bytes, got {len(data)}')
    header = struct.unpack_from('<8I', data)
    if header[0] != 0x31525046 or header[1] != 64:
        raise SystemExit('Not a FPR1 focus-property log')
    values = [struct.unpack_from('<64I', data, 32+i*256) for i in range(4)]
    initial, current, minimum, maximum = values
    print(f'log sequence: {header[2]}, changed mask: {header[4]:08x}:{header[5]:08x}')
    print('property    initial     current     minimum     maximum')
    found = False
    for i in range(64):
        changed = initial[i] != current[i] or minimum[i] != maximum[i]
        if args.all or changed:
            print(f'0x{0x2000+i:04x}  0x{initial[i]:08x}  0x{current[i]:08x}  '
                  f'0x{minimum[i]:08x}  0x{maximum[i]:08x}')
            found = True
    if not found:
        print('No property changed during the logged window; use --all to inspect the snapshot.')
    if len(data) == 5152:
        total, next_position = header[6], header[7]
        count = min(total, 128)
        print(f'\ndraw records: total={total}, showing={count}')
        print('command      x       y       w       h       extra       handled  outputs')
        start = next_position if total >= 128 else 0
        for i in range(count):
            entry = struct.unpack_from('<8I', data, 1056+((start+i)%128)*32)
            command, x, y, w, h, extra, handled, outputs = entry
            signed = lambda value: value-(1 << 32) if value & 0x80000000 else value
            print(f'0x{command:08x}  {signed(x):7d}  {signed(y):7d}  {w:7d}  {h:7d}  '
                  f'0x{extra:08x}  {handled:7d}  {outputs:7d}')


if __name__ == '__main__':
    main()
