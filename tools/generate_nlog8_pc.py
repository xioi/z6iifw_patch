#!/usr/bin/env python3
"""Generate an experimental NLOG8 custom Picture Control from RawLinear.NCP."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

TEMPLATE_SHA256 = 'e941d9fbac71e289beee6a19113551cd9cdc05ace6f975f5aae9227f5d7c73db'
LUT_ENTRIES = 256
LUT_MAX = 32767
NLOG_GRAY = 0.18
NIKON_METERED_GRAY = 0.088


def nlog_code(reflectance):
    if reflectance < 0.328:
        return 650.0 * (reflectance + 0.0075) ** (1.0 / 3.0)
    return 150.0 * math.log(reflectance) + 619.0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template', type=Path, required=True,
                        help='Horshack_RawLinear.NCP from NikonPictureControlsDev')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--force', action='store_true',
                        help='replace an existing output and report')
    args = parser.parse_args()
    if not args.force and (args.output.exists() or (args.report and args.report.exists())):
        parser.error('output or report already exists; use --force to replace them')

    source = args.template.read_bytes()
    if hashlib.sha256(source).hexdigest() != TEMPLATE_SHA256:
        raise ValueError('Template does not match the reviewed Horshack_RawLinear.NCP')
    if len(source) != 638 or source[:4] != b'NCP\0' or source[-4:] != bytes(4):
        raise ValueError('Unexpected NCP container')
    lut_offset = source.find(bytes.fromhex('000c001800250031'))
    if lut_offset < 0 or lut_offset + 2 * LUT_ENTRIES != len(source) - 4:
        raise ValueError('Unexpected RawLinear LUT location')

    gain = NLOG_GRAY / NIKON_METERED_GRAY
    source_lut = struct.unpack_from('>256H', source, lut_offset)
    output_lut = []
    rows = []
    for index, raw_value in enumerate(source_lut):
        linear = raw_value / LUT_MAX
        reflectance = linear * gain
        code = min(1023.0, max(0.0, nlog_code(reflectance)))
        encoded = round(code / 1023.0 * LUT_MAX)
        output_lut.append(encoded)
        rows.append(dict(index=index, raw_linear=linear,
                         nlog_10bit_full=code / 1023.0 * 1023.0,
                         pc_8bit_full=round(code / 1023.0 * 255.0)))
    if any(b < a for a, b in zip(output_lut, output_lut[1:])):
        raise ValueError('Generated LUT is not monotonic')

    output = bytearray(source)
    name = b'NLOG8 v01'
    output[16:36] = name.ljust(20, b'\0')
    output[40] = 0x80
    struct.pack_into('>256H', output, lut_offset, *output_lut)
    if len(output) != len(source) or output[:16] != source[:16] or output[-4:] != bytes(4):
        raise ValueError('Generated container failed validation')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)

    report = dict(profile='NLOG8 v01', template_sha256=TEMPLATE_SHA256,
                  output_sha256=hashlib.sha256(output).hexdigest(),
                  output_bytes=len(output), lut_entries=LUT_ENTRIES,
                  gray_gain=gain, sharpening=0, saturation=0,
                  gray_reference=dict(reflectance=NLOG_GRAY,
                                      nlog_10bit_full=nlog_code(NLOG_GRAY),
                                      pc_8bit_full=round(nlog_code(NLOG_GRAY) / 1023.0 * 255.0)),
                  color_space='N-Log-like luminance only; not Rec.2020/N-Log certified',
                  samples=[rows[i] for i in (0, 32, 64, 96, 128, 160, 192, 224, 255)])
    text = json.dumps(report, indent=2) + '\n'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text, encoding='utf-8')
    print(text, end='')


if __name__ == '__main__':
    main()
