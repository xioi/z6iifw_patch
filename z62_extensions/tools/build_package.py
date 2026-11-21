#!/usr/bin/env python3
"""Build a signed Z6II C 1.70 update containing only the native SD host."""
import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
sys.path.insert(0, str(REPO))
import decrypt_firmware as fw
import repack_firmware as rf
from build_host import build_host, sha


def members(body):
    mods = fw.parse_table(body)
    return [(name, bytes(body[start:start+length])) for name, start, length in mods]


def pack_members(items):
    offset = 48 + 32*len(items)
    directory = bytearray(struct.pack('>4I', len(items), offset, 0, 0))
    for name, value in items:
        encoded = name.encode('ascii')
        if len(encoded) > 16:
            raise ValueError('Module name is too long')
        directory.extend(struct.pack('>16s4I', encoded, offset, len(value), 0, 0))
        offset += len(value)
    body = b' '*32 + directory + b''.join(value for _, value in items)
    body += binascii.crc_hqx(body, 0).to_bytes(2, 'big') + bytes(14)
    return body


def build_package(stock_package, host_output, key_path, sig_path):
    key = fw.load_key(str(key_path))
    k8 = rf.load_sig(str(sig_path))
    raw = stock_package.read_bytes()
    fw.check_scheme(raw, str(stock_package))
    plain = fw.decrypt(raw, *key)
    original = members(memoryview(plain)[fw.HDR:])
    if rf.sign(plain[fw.HDR:], k8) != plain[:20]:
        raise ValueError('The supplied K8 does not verify the stock package')
    targets = [i for i, (name, _) in enumerate(original) if name.startswith('eg1850_mas_')]
    if len(targets) != 1:
        raise ValueError('Could not uniquely identify the Z6II master')
    index = targets[0]
    candidate, report = build_host(original[index][1])
    host_output.write_bytes(candidate)
    updated = list(original)
    updated[index] = (updated[index][0], candidate)
    body = pack_members(updated)
    digest = rf.sign(body, k8)
    out = digest + plain[20:fw.HDR] + body
    cipher = fw.decrypt(out, *key)
    check = fw.decrypt(cipher, *key)
    if check != out or rf.sign(check[fw.HDR:], k8) != check[:20]:
        raise ValueError('Output cipher or signature verification failed')
    rebuilt = members(memoryview(check)[fw.HDR:])
    fw.validate_firmware(memoryview(check)[fw.HDR:])
    if rebuilt != updated or [x for i, x in enumerate(rebuilt) if i != index] != [x for i, x in enumerate(original) if i != index]:
        raise ValueError('Package roundtrip changed an unexpected member')
    report.update(stock_package_sha256=sha(raw), package_sha256=sha(cipher),
                  package_bytes=len(cipher), only_master_changed=True,
                  roundtrip_verified=True, camera_verified=False)
    return cipher, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-package', type=Path, required=True)
    parser.add_argument('--key', type=Path, required=True)
    parser.add_argument('--sig', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    manifest = args.output.with_suffix(args.output.suffix + '.json')
    host_output = args.output.parent / 'eg1850_mas_01700_loader.bin'
    paths = [args.output, manifest, host_output]
    if not args.force and any(p.exists() for p in paths):
        raise ValueError('An output already exists; use --force to replace outputs')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    cipher, report = build_package(args.stock_package, host_output, args.key, args.sig)
    temporary = args.output.with_suffix(args.output.suffix + '.tmp')
    try:
        temporary.write_bytes(cipher)
        os.replace(temporary, args.output)
    finally:
        if temporary.exists():
            temporary.unlink()
    manifest.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
