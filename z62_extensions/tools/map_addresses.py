#!/usr/bin/env python3
"""Map Z7II addresses to Z6II using code and literal-pool anchors."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

BASE = 0x10020000
THUMB = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
CODE_END = 0x12600000


def words(data, value):
    needle = struct.pack('<I', value)
    result = []
    start = 0
    while True:
        found = data.find(needle, start)
        if found < 0:
            return result
        result.append(BASE + found)
        start = found + 1


def code_matches(source, target, address):
    address &= ~1
    block = source[address-BASE:address-BASE+96]
    for size in (64, 48, 32, 24, 16, 12, 8):
        hits = []
        start = 0
        while True:
            found = target.find(block[:size], start)
            if found < 0:
                break
            hits.append(BASE + found)
            start = found + 1
        if hits:
            common = []
            for hit in hits:
                length = 0
                while length < 96 and block[length] == target[hit-BASE+length]:
                    length += 1
                common.append(length)
            return size, hits, common
    return 0, [], []


def normalized_matches(source, target, address):
    address &= ~1
    code = source[address-BASE:address-BASE+80]
    instructions = list(THUMB.disasm(code, address))
    if not instructions or instructions[0].address != address:
        return []
    consumed = 0
    masks = []
    for instruction in instructions:
        if consumed >= 64:
            break
        size = instruction.size
        variable = instruction.mnemonic in ('bl', 'b.w', 'adr') or \
            (instruction.mnemonic == 'blx' and instruction.op_str.startswith('#')) or \
            ('[pc' in instruction.op_str)
        masks.extend((not variable,)*size)
        consumed += size
    block = bytearray(code[:consumed])
    for index, keep in enumerate(masks):
        if not keep:
            block[index] = 0
    runs = []
    start = None
    for index, keep in enumerate(masks + [False]):
        if keep and start is None:
            start = index
        elif not keep and start is not None:
            if index-start >= 6:
                runs.append((index-start, start, index))
            start = None
    for length, start, end in sorted(runs, reverse=True):
        needle = bytes(block[start:end])
        found = target.find(needle)
        hits = []
        while found >= 0:
            candidate = found-start
            if candidate >= 0 and candidate % 2 == 0 and candidate+consumed <= len(target):
                if all(target[candidate+i] == block[i] for i, keep in enumerate(masks) if keep):
                    hits.append(BASE+candidate)
            found = target.find(needle, found+1)
        if hits:
            return hits
    return []


def position_candidates(source, target, position):
    results = []
    for distance in list(range(16, 256, 2)) + list(range(256, 2049, 16)):
        for direction in (-1, 1):
            anchor = position + direction*distance
            offset = anchor-BASE
            if offset < 0 or offset+16 > len(source):
                continue
            block = source[offset:offset+16]
            found = target.find(block)
            if found < 0:
                continue
            if target.find(block, found+1) >= 0:
                continue
            mapped = BASE + found - direction*distance
            if mapped >= BASE and mapped+4 <= BASE+len(target):
                results.append((distance, mapped, struct.unpack_from('<I', target, mapped-BASE)[0]))
        if len(results) >= 8:
            break
    return results


def literal_candidates(source, target, address):
    refs = words(source, address)
    votes = Counter()
    evidence = {}
    for ref in refs:
        for distance, mapped, value in position_candidates(source, target, ref):
            if 0xc0000000 <= value < 0xd0000000 or BASE <= value < CODE_END:
                key = hex(value)
                votes[key] += 1
                evidence.setdefault(key, []).append({'source_literal': hex(ref),
                    'target_literal': hex(mapped), 'distance': distance})
    return refs, votes, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--z7', type=Path, required=True)
    parser.add_argument('--z6', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('sources', nargs='+', type=Path)
    args = parser.parse_args()
    source, target = args.z7.read_bytes(), args.z6.read_bytes()
    occurrences = {}
    for path in args.sources:
        text = path.read_text(encoding='utf-8')
        for match in re.finditer(r'0x[0-9a-fA-F]{8}', text):
            address = int(match.group(), 16)
            line = text.count('\n', 0, match.start()) + 1
            occurrences.setdefault(address, []).append(f'{path.name}:{line}')
    report = {}
    for address, uses in sorted(occurrences.items()):
        row = {'uses': uses}
        if BASE <= (address & ~1) < CODE_END:
            size, hits, common = code_matches(source, target, address)
            row.update(kind='code', prefix=size, candidates=[hex(x) for x in hits], common=common)
            if not hits:
                hits = normalized_matches(source, target, address)
                row['normalized_candidates'] = [hex(x) for x in hits]
            if len(hits) == 1:
                row['mapped'] = hex(hits[0] | (address & 1))
            if 'mapped' not in row and address >= 0x10800000:
                refs, votes, evidence = literal_candidates(source, target, address)
                row['references'] = [hex(x) for x in refs]
                if votes:
                    row['literal_votes'] = dict(votes)
                    row['evidence'] = evidence
                    row['mapped'] = votes.most_common(1)[0][0]
        elif 0xb0000000 <= address < 0xc0000000:
            row['kind'] = 'ram'
            refs, votes, evidence = literal_candidates(source, target, address)
            row['references'] = [hex(x) for x in refs]
            if votes:
                row['votes'] = dict(votes)
                row['evidence'] = evidence
                row['mapped'] = votes.most_common(1)[0][0]
        else:
            row['kind'] = 'constant'
        report[hex(address)] = row
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    counts = Counter(row['kind'] + ('-mapped' if 'mapped' in row else '-unresolved') for row in report.values())
    print(json.dumps({'addresses': len(report), 'counts': counts, 'output': str(args.output)}, indent=2))
    for address, row in report.items():
        if row['kind'] != 'constant' and 'mapped' not in row:
            print('UNRESOLVED', address, row['kind'], ','.join(row['uses']))


if __name__ == '__main__':
    main()
