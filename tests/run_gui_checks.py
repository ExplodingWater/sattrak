#!/usr/bin/env python3
"""Run real desktop/GPU/audio checks with isolated captures and readable failures.

Requires a working graphical desktop and audio output. Display-mode transitions
are excluded unless --include-display is supplied. No network checks are enabled.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

MODES = {
    'startup': ['--startup-smoke-test'],
    'scene': ['--smoke-test'],
    'tutorial': ['--tutorial-smoke-test'],
    'tutorial-compact': ['--tutorial-smoke-test', '--compact'],
    'orbit': ['--orbit-smoke-test'],
    'console': ['--console-smoke-test'],
    'multiview': ['--multiview-smoke-test'],
    'models': ['--models-smoke-test'],
    'iss-depth': ['--iss-depth-smoke-test'],
    'interface': ['--interface-smoke-test'],
    'accessibility': ['--accessibility-smoke-test'],
    'interaction': ['--interaction-smoke-test'],
    'catalog': ['--catalog-smoke-test'],
    'update-ui': ['--update-ui-smoke-test'],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--sky-renderer', type=Path)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', action='append', choices=MODES)
    parser.add_argument('--include-display', action='store_true')
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    source = args.source_root.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    modes = [(mode, MODES[mode]) for mode in (args.mode or list(MODES))]
    if args.include_display:
        modes.append(('display', ['--display-smoke-test']))
    results = []

    def check(name, command):
        folder = output / name
        folder.mkdir(exist_ok=True)
        for asset in ('assets', 'earth_texture.jpg', 'LICENSE', 'TERMS.md'):
            link = folder / asset
            if not link.exists():
                link.symlink_to(source / asset, target_is_directory=asset == 'assets')
        log = folder / 'result.log'
        with log.open('w') as stream:
            try:
                code = subprocess.run(command, cwd=folder, stdout=stream, stderr=subprocess.STDOUT, timeout=120).returncode
            except subprocess.TimeoutExpired:
                code = 124
                stream.write('\nGUI check timed out after 120 seconds\n')
        results.append({'mode': name, 'exit_code': code, 'log': str(log)})
        print(name + ': ' + ('PASS' if code == 0 else 'FAIL (' + str(code) + ')'), flush=True)
        if code:
            print(log.read_text()[-3000:], flush=True)

    for mode, flags in modes:
        check(mode, [str(binary), *flags])
    if args.sky_renderer:
        check('sky-renderer', [str(args.sky_renderer.resolve(strict=True)), str(source / 'assets/sky'), str(output / 'sky-preview.ppm')])
    (output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    return int(any(row['exit_code'] for row in results))


if __name__ == '__main__':
    sys.exit(main())
