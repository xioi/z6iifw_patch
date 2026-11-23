#!/usr/bin/env python3
"""Build the signed Z6II C 1.70 SD-loader update using only the stock package.

The embedded blobs contain the native host and the container transform tables.
No external key file, K8 input, NumPy installation or compiler is required.
The fixed output signature and package hash come from the verified native-host
build. This script never accepts an already modified package as input.
"""
import argparse
import base64
import binascii
import hashlib
import json
import os
from pathlib import Path
import struct
import zlib

STOCK_PACKAGE_SHA256 = '5a9640b4c07abe1ecbf5a52655cbec6b893bba045bb2a1a1d21055b9f8f9d06d'
STOCK_MASTER_SHA256 = 'bb83613121f843d65101da936d942acdc513d86dbd3abc26259d0d870d03f5d4'
PATCHED_MASTER_SHA256 = '632df40e82277403d75d726e75193515ebf4f2dd83058faf6a72eb9addcd384b'
OUTPUT_PACKAGE_SHA256 = 'aa5e2e8599f9126e92476c64e7de7c966d662d101e7d9ab19a358d1bb19ceacf'
OUTPUT_SIGNATURE = bytes.fromhex('6a504b565c7895747cd1fef409c867e5b1b44126')
STOCK_MASTER_BYTES = 38750138
HOST_BYTES = 1372
HOOK_OFFSET = 0x1045063C - 0x10020000
BSS_OFFSET = 0x140C4
ORIGINAL_HOOK = bytes.fromhex('4d4851a110300cf772fe')
PATCHED_HOOK = bytes.fromhex('dff800f0b947511200bf')
ORIGINAL_BSS = bytes.fromhex('c0ed8ac1908a2901')
PATCHED_BSS = bytes.fromhex('b8ed8ac1988a2901')

CRYPT_TABLES_B85 = 'c-jE~1ONQ7rysf`@&(}h3Z0&7l*P2oJ{JNJV+-$XuGdS+!P{j}axw9Jj7h}9fbb>>5`@P)MKe8HNP`nMY~=aqj)&R=ya$zn^+CIQFka<g&P}rGDsR)k;YWA2%Hy}JoFXmMxSAFi8#TZCUpCxxc0yrM_8nPjI>>w8Dd`&QKe?u9^wyZIv!JgCkBw(tD`X8JnJ0gv4*`%2k^fJ6-R_bcC4nP;bdy?UPEroyVyO^Dh~L@Fc`~2o^B{{XmiLJUs5q*kmvB5*XmzA7RTUIDq0uz`wZh9D_`P_g*#4VW`WaNx0MBl|t1d)wQ`G}Z##ri#=j}t#uu5Epg(yG(c{Q)Cs_j2GG{NbbBX4IeRUg#O9-dw2g=|8bd~pB74_v;O)x)NXVUM^1PcgH%EAR;%-IIgA;0J1dQ>FTPbjLmlFi3t4cOo6nIlG4^CNI*_8cD$G$~x#^LD#kzl&nisy^$^MCFDTn+DB4XuoXkXDro^cl6z(QvF`}?N)}#*$@E$d3@PRE2Au5pq*0c+8)oVtab%28;r5l67yJX?EU0KGI}muxJytf}`6LUekl7i?isIHxMKfX)cJbK7{i3{ypK|`XH=&@-<3w(b1QShP+cIsmu4~h)5u--XnOTJOTXoB0Sf>Rc+>P^s5`D6PoldoYbBM+O3MrJ#tzY4()Grm+kXhR7*|M=e#nZF$J!!mPiGoMFmkrKMbl^xQ$Y&!_Ql5qpH0mWjg_WKER*LlLMU4BbdZ@H>2!vP}ZB7E4Vl}W!p!#eo3tcU4IzmBc2}(R>y5wHKKm(G}gC6k(aR!Z(2k3d{Ia<p$-H{l-#}vI9Wc~Q}A%3Kv!(*0q0ZDtB{>m{Rb?f59VN+Y7)vnD@wTEyWrne{-?-wvG&!RHixp<Ex!X`JP+aeD33|Efs(GwqBa&NYoLsUEE?a8IafmQk55{nxI^-QarfTx&hxbqJ<5M_NV9Ps=rM!w(H<9}<=uXotyPego(s=+gV=6;X'
HOST_CODE_B85 = 'c-mD|eQXnD7=PaO+FpCZ-Xb3*4tzUyShhjh!B8P&?X7)pWTWi{n1LvxeidUvO(4;T+7g2WLTF+jU5v>HSzu`6mW70m%(iM`G+=t{AC_gY`UhjHlDiu#=M0(eNznMm-Fwf+`#itr_j}*tuUI-P+pV2h36y2}*gf9=i_R+m%;%}%C$<pZG~UX0T0=Y=4Acdyzx#_?H)f@Tq+Bt7y+dbCr-$3M7jaPwLsAM?ukOC7om}RLyG9PiJN(ESDs^e9tC_|(vEYWa1*tsB5Wqnl_aQImS&%K5XfJmcGz3L$8%j&yMwi?7?c9ZYb?X*nnpw@EoW2?&4<H(@{;-kucbhW^pVk>bm+2leCRqu`k%yb?s(xBqZg1`@YON^Vhe>{J|CzL8F0*PFVUm{E;a7C#lromQa)=xo4pgIWQp9`0)w3OdQA6ax@*_93)N(q(%2~2RWjI=$y)^^ieGhq_lf6gHo(9?bk;&thgH>IOl0Af?sRzZYGsNU!sn^=`pI4yX2_4{tkqMpQ1AgQ)$*4aVOT|phjBILn9Tz0Dtt3y)z#EwONF!N%xO4HzS9md~sk@3A&kRV9rVX~&&ww*G|NMvVkVA78W{{j9R?1{<2-XDjHGnocnigcD4IasTRn&G+T5HKAsc97^l=u3qlyn!gErGNcr~O8Kk!t?1S%@gW7BnXV>$uO}k}s$tIxoI<yR$kI1vga;U_KRdk&FbMNQvuD5}NP0HDf;rn9n(>2Ingdj%N4NWNysE$4kz7@>Hb{xQfH>AtyqS2mHIVis@Ct=F#S-&SfNuE>SZ?(LoD=`QqW+CFhp|l2O4*Jmg7!MoMCe)qh-M^3H5;=xLd)IL!P|t&W;*sE9MuRhPN`pJ<P3;~+3)ld$4fMzEGG!W2bk%w|kwbo-7N-9XKt+@<ut9JumUVcW<J6`Y{_mF&{m+`VM3JgL_~ySDd$G2!>smTrnWGfDNE)Oort9Y~9lx~njyH<ugbMBF$LG52u1pe1V4qFYwtiO_2K8m;+EsjqIhVuuyuE0s31RQ_>3iB5+2J9P>|OXQm&?l#+>WK*K;4x6=ClHm6q4<Q)@DBA=(eo4nX^1?b--X`##hcc}9D;JXA(7LNi9UAsUl*LxSKUc4IW5sM@*Wkvb`Idc9e{9xbO3n6I_RoHMn9c8I(bh&%m610Qnw9UhLm9B|?X>7ujwUJJ-HwW#rCcLcZ39GA2KOz!xnL;$ehJhz#Hs!ox6;NK^Vl+M=>VxGorEgDp{*wR{!^aParwFtEin~4f_g+%xvnC|f^1-2QL3Q~45<u_z}l!@@BPBIOFkv`+;NPIijj%Cy(H9idu<T4+>M?KttNlSRE5S~s@5h)Nn$E242YHqH?iH{`YiUP#D#HsQ==f{=OSlp_iOhXYkuy~8G2WFYuq1C&RPL^UHJ`!>&IkMZ1Lg6M~;5B50Bcit+A5ipr?Zs45htB5Bi_EsMawaGDybmfm0cKd<Gsz7(tn)cEdk5y?wUqfMgf$PTRAb!1Vn#4S)ckhCVS!AaTpSy_;JC(dY(odl&-$v;GF`)F>w'


def decode_blob(encoded):
    return zlib.decompress(base64.b85decode(encoded.encode('ascii')))


CRYPT_TABLES = decode_blob(CRYPT_TABLES_B85)
HOST_CODE = decode_blob(HOST_CODE_B85)
if len(CRYPT_TABLES) != 768 or len(HOST_CODE) != HOST_BYTES:
    raise RuntimeError('Embedded loader data is corrupt')
T1 = CRYPT_TABLES[:256]
T2 = CRYPT_TABLES[256:512]
T3 = CRYPT_TABLES[512:]
CIPHER_PERIOD = 1 << 24
CIPHER_MASK = b''.join(
    bytes(T1[i & 0xFF] ^ T2[(i >> 8) & 0xFF] ^ T3[high] for i in range(0x10000))
    for high in range(0x100)
)


def crypt(image):
    if len(image) < 64:
        raise ValueError('Firmware is too short')
    body = image[32:]
    mask = (CIPHER_MASK * ((len(body) + CIPHER_PERIOD - 1) // CIPHER_PERIOD))[:len(body)]
    transformed = (int.from_bytes(body, 'big') ^ int.from_bytes(mask, 'big')).to_bytes(len(body), 'big')
    return image[:32] + image[32:64] + transformed[32:]


def parse_members(body, check_crc=True):
    if len(body) < 112 or body[:32] != b' ' * 32 or body[-14:] != bytes(14):
        raise ValueError('Invalid firmware body or checksum trailer')
    count, directory_size = struct.unpack_from('>II', body, 0x20)
    if not 0 < count <= 64 or directory_size != 48 + 32 * count:
        raise ValueError('Invalid firmware module directory')
    expected = directory_size
    members = []
    for index in range(count):
        position = 0x30 + index * 32
        name = bytes(body[position:position + 16]).rstrip(bytes(1))
        start, length = struct.unpack_from('>II', body, position + 16)
        if not name or len(name) > 16 or start != expected or length < 2:
            raise ValueError('Invalid module name or extent')
        expected += length
        if expected > len(body) - 16:
            raise ValueError('Module extends beyond the firmware body')
        payload = bytes(body[start:start + length])
        if check_crc and binascii.crc_hqx(payload, 0):
            raise ValueError(f'Module CRC mismatch: {name.decode("ascii", "replace")}')
        members.append((name, payload))
    if expected != len(body) - 16:
        raise ValueError('Module extents do not end at the checksum trailer')
    if check_crc:
        stored = struct.unpack_from('>H', body, len(body) - 16)[0]
        if binascii.crc_hqx(body[:-16], 0) != stored:
            raise ValueError('Package CRC mismatch')
    return members


def pack_members(members):
    offset = 48 + 32 * len(members)
    directory = bytearray(struct.pack('>4I', len(members), offset, 0, 0))
    for name, payload in members:
        directory.extend(struct.pack('>16s4I', name, offset, len(payload), 0, 0))
        offset += len(payload)
    body = b' ' * 32 + directory + b''.join(payload for _, payload in members)
    return body + binascii.crc_hqx(body, 0).to_bytes(2, 'big') + bytes(14)


def build_loader(stock_package):
    if hashlib.sha256(stock_package).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError('Input is not the exact Nikon Z6II C 1.70 stock package; expected SHA-256 ' + STOCK_PACKAGE_SHA256)
    plain = crypt(stock_package)
    original = parse_members(plain[32:])
    targets = [index for index, (name, _) in enumerate(original) if name.startswith(b'eg1850_mas_')]
    if len(targets) != 1:
        raise ValueError('Could not uniquely identify the Z6II master')
    index = targets[0]
    master = original[index][1]
    if len(master) != STOCK_MASTER_BYTES or hashlib.sha256(master).hexdigest() != STOCK_MASTER_SHA256:
        raise ValueError('Stock master hash or size mismatch')
    if master[HOOK_OFFSET:HOOK_OFFSET + 10] != ORIGINAL_HOOK or master[BSS_OFFSET:BSS_OFFSET + 8] != ORIGINAL_BSS:
        raise ValueError('Stock host hook or BSS boundary mismatch')

    patched = bytearray(master[:-2])
    patched[HOOK_OFFSET:HOOK_OFFSET + 10] = PATCHED_HOOK
    patched[BSS_OFFSET:BSS_OFFSET + 8] = PATCHED_BSS
    patched.extend(HOST_CODE)
    patched.extend(binascii.crc_hqx(patched, 0).to_bytes(2, 'big'))
    if hashlib.sha256(patched).hexdigest() != PATCHED_MASTER_SHA256:
        raise ValueError('Patched master verification failed')

    updated = list(original)
    updated[index] = (updated[index][0], bytes(patched))
    body = pack_members(updated)
    candidate = OUTPUT_SIGNATURE + plain[20:32] + body
    encrypted = crypt(candidate)
    if crypt(encrypted) != candidate:
        raise ValueError('Firmware transform roundtrip failed')
    rebuilt = parse_members(crypt(encrypted)[32:])
    if rebuilt != updated:
        raise ValueError('Package roundtrip changed a module')
    if candidate[:20] != OUTPUT_SIGNATURE or hashlib.sha256(encrypted).hexdigest() != OUTPUT_PACKAGE_SHA256:
        raise ValueError('Signed package verification failed')
    return encrypted, {
        'stock_package_sha256': STOCK_PACKAGE_SHA256,
        'master_sha256': PATCHED_MASTER_SHA256,
        'host_bytes': HOST_BYTES,
        'package_bytes': len(encrypted),
        'package_sha256': OUTPUT_PACKAGE_SHA256,
        'roundtrip_verified': True,
        'signature_verified': True,
        'only_master_changed': True,
        'camera_verified': False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stock_package', type=Path, help='unmodified Nikon Z6_2_0170.bin')
    parser.add_argument('--output', type=Path, required=True, help='signed loader update to create')
    parser.add_argument('--force', action='store_true', help='replace an existing output')
    args = parser.parse_args()
    if args.stock_package.resolve() == args.output.resolve():
        parser.error('output must differ from the stock input')
    if args.output.exists() and not args.force:
        parser.error('output already exists; use --force to replace it')
    try:
        encrypted, report = build_loader(args.stock_package.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix(args.output.suffix + '.tmp')
        try:
            temporary.write_bytes(encrypted)
            os.replace(temporary, args.output)
        finally:
            if temporary.exists():
                temporary.unlink()
    except (OSError, ValueError) as exc:
        parser.exit(1, f'error: {exc}\n')
    report['output'] = str(args.output)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
