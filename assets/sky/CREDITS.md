# Offline sky catalog

Data converted from [d3-celestial](https://github.com/ofrohn/d3-celestial),
by Olaf Frohn, pinned at commit `d2e20e104b86429d90ac8227a5b021262b45d75a`.
The upstream BSD 3-Clause notice is reproduced in `LICENSE-d3-celestial.txt`.

- `stars.txt`: 5,044 entries from `data/stars.6.json`. Hipparcos IDs, J2000
  right ascension/declination in degrees, apparent visual magnitude and B-V.
  Upstream source: **XHIP: An Extended Hipparcos Compilation**, Anderson &
  Francis (2012), [VizieR V/137D](https://cdsarc.cds.unistra.fr/viz-bin/cat/V/137D).
- `lines.txt`: 743 segments converted from `data/constellations.lines.json`.
- `labels.txt`: 89 label positions from `data/constellations.json`, covering
  all 88 constellation identifiers; Serpens has Caput and Cauda labels.
  Upstream constellation sources: IAU constellation charts, with label
  positions and line modifications by Olaf Frohn. Connecting lines are
  illustrative Western figures, not official constellation boundaries.

Conversion: normalized signed right ascension to [0,360), flattened line
paths into endpoint pairs, kept English names (ASCII transliteration for the
vector font), removed unused attributes.
Missing B-V would use 0.65 (solar-like); positions are not synthesized.
Regenerate with `python3 scripts/build_sky_catalog.py`. This downloads only
at regeneration time. The application and ordinary build work offline.

Constellation text uses Sean Barrett's **stb_easy_font v1.1**, bundled in
`external/stb/stb_easy_font.h` at stb commit
`f58f558c120e9b32c217290b80bad1a0729fbb2c`; its MIT/public-domain terms
are included in the header.
