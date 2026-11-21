#!/usr/bin/env python3
"""Build the Z6II C 1.70 native module host from the exact stock master."""
import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x10020000
APPEND = 0x125147B8
HOOK_SITE = 0x1045063C
STOCK_SHA = 'bb83613121f843d65101da936d942acdc513d86dbd3abc26259d0d870d03f5d4'
ADDRESS_MAP = {
    '0x1044e158': '0x10450784',
    '0x10361083': '0x1035d32b',
    '0x104d3c01': '0x104d67a5',
    '0x104d3c05': '0x104d67a9',
    '0x104d3d83': '0x104d6927',
    '0x104d3c91': '0x104d6835',
    '0x10509395': '0x1050c051',
    '0x10495c1d': '0x1049853d',
    '0x101045f1': '0x101045f1',
    '0x104e0695': '0x104e3239',
    '0x103d3dff': '0x103d655b',
    '0x103dc46b': '0x103debc7',
    '0x103606df': '0x1035c987',
    '0x1034fe31': '0x1034c1f1',
    '0x1034fe7f': '0x1034c23f',
    '0x103dc539': '0x103dec95',
    '0x1034fa29': '0x1034bde9',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tool(name, llvm=False):
    value = os.environ.get(name.upper())
    found = shutil.which(value or name)
    if found:
        return found
    candidate = Path('/opt/homebrew/opt/llvm/bin') / name
    if llvm and candidate.exists():
        return str(candidate)
    raise RuntimeError(f'Missing build tool: {name}')


def thumb_jump(site, length, target):
    padding = (-site) % 4
    code = struct.pack('<HH', 0xF8DF, 0xF000 | (4 if padding else 0))
    code += b'\x00\xbf' * (padding // 2) + struct.pack('<I', target | 1)
    if len(code) > length or (length-len(code)) % 2:
        raise ValueError('Hook does not fit instruction window')
    return code + b'\x00\xbf' * ((length-len(code)) // 2)


def build_host(stock):
    if sha(stock) != STOCK_SHA or BASE + len(stock) - 2 != APPEND or binascii.crc_hqx(stock, 0):
        raise ValueError('Requires the exact original Z6II C 1.70 master')
    original_hook = stock[HOOK_SITE-BASE:HOOK_SITE-BASE+10]
    if original_hook != bytes.fromhex('4d4851a110300cf772fe'):
        raise ValueError(f'Unexpected original init instructions: {original_hook.hex()}')
    with tempfile.TemporaryDirectory(prefix='z62-native-host-') as folder:
        obj, elf, binary = (Path(folder) / name for name in ('host.o', 'host.elf', 'host.bin'))
        entry = Path(folder) / 'entry.o'
        subprocess.run([tool('clang'), '--target=arm-none-eabi', '-mcpu=cortex-a9', '-mthumb',
                        '-c', str(ROOT / 'host/entry.S'), '-o', str(entry)], check=True)
        subprocess.run([tool('clang'), '--target=arm-none-eabi', '-mcpu=cortex-a9', '-mthumb',
                        '-mfloat-abi=soft', '-Os', '-ffreestanding', '-fno-builtin',
                        '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                        '-fno-stack-protector', '-Wall', '-Wextra', '-Werror',
                        '-I', str(ROOT / 'sdk'), '-c', str(ROOT / 'host/host.c'), '-o', str(obj)], check=True)
        subprocess.run([tool('arm-none-eabi-ld'), '-T', str(ROOT / 'host/host.ld'),
                        '--no-undefined', str(entry), str(obj), '-o', str(elf)], check=True)
        subprocess.run([tool('llvm-objcopy', True), '-O', 'binary', '--only-section=.extension',
                        str(elf), str(binary)], check=True)
        symbols = {name: int(value, 16) for value, name in re.findall(
            r'^([0-9a-fA-F]+) [tT] (\w+)$',
            subprocess.check_output([tool('llvm-nm', True), str(elf)], text=True), re.M)}
        code = binary.read_bytes()
        if not 0 < len(code) < 8192 or symbols['extension_init_hook'] != APPEND:
            raise ValueError('Unexpected host size or entry address')
    patched = bytearray(stock[:-2])
    patched[HOOK_SITE-BASE:HOOK_SITE-BASE+10] = thumb_jump(HOOK_SITE, 10, symbols['extension_init_hook'])
    if struct.unpack_from('<II', stock, 0x140C4) != (0xC18AEDC0, 0x1298A90):
        raise ValueError('Original BSS boundary mismatch')
    struct.pack_into('<II', patched, 0x140C4, 0xC18AEDB8, 0x1298A98)
    patched.extend(code)
    patched.extend(binascii.crc_hqx(patched, 0).to_bytes(2, 'big'))
    report = dict(stock_sha256=sha(stock), master_sha256=sha(patched), host_bytes=len(code),
                  hook=dict(address=hex(HOOK_SITE), original=original_hook.hex(),
                            patched=patched[HOOK_SITE-BASE:HOOK_SITE-BASE+10].hex()),
                  anchor='0xc18aedb8', append=hex(APPEND), symbols=symbols,
                  z7ii_to_z6ii=ADDRESS_MAP, camera_verified=False)
    return bytes(patched), report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-master', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    manifest = args.output.with_suffix(args.output.suffix + '.json')
    if not args.force and (args.output.exists() or manifest.exists()):
        raise ValueError('Output or manifest already exists; use --force to replace them')
    candidate, report = build_host(args.stock_master.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(candidate)
    manifest.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
