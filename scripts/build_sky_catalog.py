#!/usr/bin/env python3
"""Build compact offline J2000 sky assets from pinned d3-celestial GeoJSON.

Use --source-dir to read previously downloaded upstream files without networking.
No third-party Python packages are required. Run from any working directory.
"""
import argparse
import json
from pathlib import Path
import urllib.request
import unicodedata

COMMIT = 'd2e20e104b86429d90ac8227a5b021262b45d75a'
BASE = f'https://raw.githubusercontent.com/ofrohn/d3-celestial/{COMMIT}/'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets/sky')
    args = parser.parse_args()

    def read(name):
        if args.source_dir:
            return (args.source_dir / name).read_text(encoding='utf-8')
        with urllib.request.urlopen(BASE + ('LICENSE' if name == 'LICENSE' else 'data/' + name), timeout=30) as response:
            return response.read().decode('utf-8')

    stars = json.loads(read('stars.6.json'))['features']
    lines = json.loads(read('constellations.lines.json'))['features']
    labels = json.loads(read('constellations.json'))['features']
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / 'stars.txt').open('w') as out:
        out.write('# HIP_ID RA_deg Dec_deg V_magnitude B-V (J2000; missing B-V = 0.65)\n')
        for star in stars:
            ra, dec = star['geometry']['coordinates']
            mag = star['properties']['mag']
            bv = star['properties'].get('bv')
            if mag > 6:
                continue
            out.write(f"{star['id']} {ra % 360:.4f} {dec:.4f} {mag:.2f} {float(bv) if bv not in (None, '') else 0.65:.3f}\n")
    with (args.output / 'lines.txt').open('w') as out:
        out.write('# constellation RA1_deg Dec1_deg RA2_deg Dec2_deg (J2000)\n')
        for figure in lines:
            for path in figure['geometry']['coordinates']:
                for a, b in zip(path, path[1:]):
                    out.write(f"{figure['id']} {a[0] % 360:.4f} {a[1]:.4f} {b[0] % 360:.4f} {b[1]:.4f}\n")
    with (args.output / 'labels.txt').open('w') as out:
        out.write('# constellation RA_deg Dec_deg "English name" (J2000)\n')
        for label in labels:
            ra, dec = label['geometry']['coordinates']
            # The small vector font supports ASCII; transliterate diacritics.
            name = unicodedata.normalize('NFKD', label['properties']['name']).encode('ascii', 'ignore').decode('ascii')
            out.write(f"{label['id']} {ra % 360:.4f} {dec:.4f} {json.dumps(name, ensure_ascii=True)}\n")
    (args.output / 'LICENSE-d3-celestial.txt').write_text(read('LICENSE'), encoding='utf-8')
    print(f'Wrote sky assets to {args.output} from d3-celestial {COMMIT}')


if __name__ == '__main__':
    main()
