#!/usr/bin/env python3
"""Apply a reviewed address map to copied Z62 sources."""
import argparse
import json
from pathlib import Path
import re

SKIPPED = {0xb1853e64}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--overrides', type=Path, required=True)
    parser.add_argument('sources', nargs='+', type=Path)
    args = parser.parse_args()
    report = json.loads(args.map.read_text())
    mapping = {int(address, 16): int(row['mapped'], 16)
               for address, row in report.items() if 'mapped' in row}
    mapping.update({int(a, 16): int(v, 16) for a, v in json.loads(args.overrides.read_text()).items()})
    for address in SKIPPED:
        mapping.pop(address, None)
    for path in args.sources:
        text = path.read_text(encoding='utf-8')
        changed_count = 0
        def counted(match):
            nonlocal changed_count
            address = int(match.group().rstrip('u'), 16)
            if address not in mapping:
                return match.group()
            changed_count += 1
            return '0x' + format(mapping[address], '08x') + ('u' if match.group().endswith('u') else '')
        text = re.sub(r'0x[0-9a-fA-F]{8}u?', counted, text)
        path.write_text(text, encoding='utf-8')
        print(path, changed_count)


if __name__ == '__main__':
    main()
