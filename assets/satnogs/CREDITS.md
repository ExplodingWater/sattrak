# SatNOGS data

Satellite metadata, orbital-element records, transmitters and public ground-station
locations are supplied by SatNOGS / Libre Space Foundation and its contributors.

- https://db.satnogs.org/about/
- https://network.satnogs.org/about/
- License: [Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/)
- Endpoint URLs and the field selection are in `scripts/update_satnogs.py`.
- `downloaded.txt` records retrieval time, not an orbital measurement time.

The bundled snapshots have been reduced to fields used by sattrak. These data and
adapted datasets retain their CC BY-SA terms; they are not covered by the source
code's MIT license. Individual orbital records retain the upstream source named
by SatNOGS. Ground-station status and transmitter information describe the last
retrieved snapshot, not a live connection or a guarantee of reception.
