#!/usr/bin/env python3
"""Build one Z6II SD module with a selectable feature configuration."""
import argparse
import hashlib
import json
from pathlib import Path

from module import build as build_native

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'plugins/extensions.c'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--portrait', action='store_true',
                        help='rotate the LCD shooting and supported menu UI')
    parser.add_argument('--evf', action='store_true',
                        help='enable the experimental EVF portrait path; implies --portrait')
    parser.add_argument('--movie', action='store_true',
                        help='enable the experimental normal-movie duration extension')
    parser.add_argument('--video-rotation', action='store_true',
                        help='write the latched camera orientation into MOV/MP4 track metadata')
    parser.add_argument('--raw12', action='store_true',
                        help='QEMU-only prototype; known to crash a real camera during recording')
    parser.add_argument('--raw12-hook-only', action='store_true',
                        help='diagnose only the MIPU hook; no RAW reads, buffers, menus or files')
    parser.add_argument('--raw12-single-frame', action='store_true',
                        help='copy one frame to memory per recording; no files, menus or retries')
    parser.add_argument('--raw12-single-export', action='store_true',
                        help='export one captured frame only after normal recording has stopped')
    parser.add_argument('--raw12-dual-frame', action='store_true',
                        help='capture two consecutive frames in memory and export both after stopping')
    parser.add_argument('--raw12-rawout-single', action='store_true',
                        help='research one MIPU RAW-output DMA frame without a CPU frame copy')
    parser.add_argument('--raw12-dma-single', action='store_true',
                        help='research one frame split between XDMAC channel 3 and the CPU')
    parser.add_argument('--raw12-dma-dual', action='store_true',
                        help='capture two consecutive frames with XDMAC and export both after stopping')
    parser.add_argument('--raw12-dma-burst4', action='store_true',
                        help='capture four consecutive frames in memory and export after stopping')
    parser.add_argument('--focus-probe', action='store_true',
                        help='enable focus diagnostics; implies --portrait')
    parser.add_argument('--output', type=Path, default=Path('artifacts/Z62APP.BIN'),
                        help='module output path (default: %(default)s)')
    parser.add_argument('--force', action='store_true',
                        help='replace the previous module and configuration manifest')
    args = parser.parse_args()

    if not (args.portrait or args.evf or args.movie or args.video_rotation or args.raw12 or
            args.raw12_hook_only or args.raw12_single_frame or args.raw12_single_export or args.raw12_dual_frame or
            args.raw12_rawout_single or args.raw12_dma_single or args.raw12_dma_dual or args.raw12_dma_burst4 or args.focus_probe):
        parser.error('select at least one feature, including the explicitly selected RAW12 research modes')
    if (args.raw12+args.raw12_hook_only+args.raw12_single_frame+args.raw12_single_export+args.raw12_dual_frame+
        args.raw12_rawout_single+args.raw12_dma_single+args.raw12_dma_dual+args.raw12_dma_burst4)>1:
        parser.error('the nine RAW12 research modes are mutually exclusive')

    portrait = args.portrait or args.evf or args.focus_probe
    configuration = dict(portrait=portrait, evf=args.evf, movie=args.movie,
                         video_rotation=args.video_rotation, raw12=args.raw12,
                         raw12_hook_only=args.raw12_hook_only, raw12_single_frame=args.raw12_single_frame,
                         raw12_single_export=args.raw12_single_export, raw12_dual_frame=args.raw12_dual_frame,
                         raw12_rawout_single=args.raw12_rawout_single, raw12_dma_single=args.raw12_dma_single,
                         raw12_dma_dual=args.raw12_dma_dual, raw12_dma_burst4=args.raw12_dma_burst4,
                         focus_probe=args.focus_probe, histogram=False)
    raw12_hook = args.raw12_hook_only or args.raw12_single_frame or args.raw12_single_export or args.raw12_dual_frame or args.raw12_rawout_single or args.raw12_dma_single or args.raw12_dma_dual or args.raw12_dma_burst4
    raw12_frame = args.raw12_single_frame or args.raw12_single_export or args.raw12_dual_frame or args.raw12_rawout_single or args.raw12_dma_single or args.raw12_dma_dual or args.raw12_dma_burst4
    raw12_export = args.raw12_single_export or args.raw12_dual_frame or args.raw12_rawout_single or args.raw12_dma_single or args.raw12_dma_dual or args.raw12_dma_burst4
    defines = [f'Z62_WITH_PORTRAIT={int(portrait)}',
               f'Z62_WITH_MOVIE={int(args.movie)}',
               f'Z62_WITH_MOVIE_ROTATION={int(args.video_rotation)}',
               f'Z62_WITH_RAW12={int(args.raw12)}',
               f'Z62_WITH_RAW12_HOOK_ONLY={int(raw12_hook)}',
               f'Z62_RAW12_SINGLE_FRAME={int(raw12_frame)}',
               f'Z62_RAW12_SINGLE_EXPORT={int(raw12_export)}',
               f'Z62_RAW12_DUAL_FRAME={int(args.raw12_dual_frame)}',
               f'Z62_RAW12_RAWOUT_SINGLE={int(args.raw12_rawout_single)}',
               f'Z62_RAW12_DMA_SINGLE={int(args.raw12_dma_single)}',
               f'Z62_RAW12_DMA_DUAL={int(args.raw12_dma_dual)}',
               f'Z62_RAW12_DMA_BURST4={int(args.raw12_dma_burst4)}',
               f'Z62_EVF_PORTRAIT={int(args.evf)}']
    if args.focus_probe:
        defines.append('Z62_FOCUS_PROBE=1')

    manifest = args.output.with_suffix(args.output.suffix + '.json')
    if not args.force and (args.output.exists() or manifest.exists()):
        parser.error('output or manifest already exists; use --force to replace them')

    image, report = build_native(SOURCE, defines)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    digest = hashlib.sha256(image).hexdigest()
    report.update(source='plugins/extensions.c', configuration=configuration,
                  defines=defines, module_bytes=len(image), module_sha256=digest,
                  camera_verified=False)
    if args.raw12:
        report.update(camera_failed=True,
                      camera_failure='Real Z6II 1080p30: recording freeze, display corruption and watchdog reboot; no MOV/Z12 produced')
    manifest.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(output=str(args.output), configuration=configuration,
                          module_bytes=len(image), module_sha256=digest,
                          manifest=str(manifest)), indent=2))


if __name__ == '__main__':
    main()
