# Additional research-satellite data sources

Candidates researched September 15, 2026; these are suggestions, not additional integrated providers.

- [CelesTrak SupGP](https://www.celestrak.org/NORAD/documentation/sup-gp-queries.php): supplemental orbital elements derived from sources such as operator ephemerides. Coverage depends on the operator, so this can complement the existing GP catalogs rather than replace them for every research mission.
- [Space-Track](https://www.space-track.org/documentation): another route to catalog and orbital-element data, including history. Integration would need account/authentication handling, rate-limit compliance and source attribution. Basic SSA redistribution has conditions documented by Space-Track; do not bundle credentials.
- [ESA DISCOSweb](https://discosweb.esoc.esa.int/): spacecraft/launch metadata, useful for richer information panels and identifying objects. It is not a direct substitute for time-dependent orbital elements. Use the current DISCOSweb interface/API documentation; the separate legacy DISCOSweb Operations API is discontinued.

For the current application, expanding CelesTrak coverage and adopting OMM-format
elements would be a practical next step. Additional sources must be deduplicated
by catalog identifier and retain epoch/source information. Refreshed orbital
elements still produce predictions, not continuous measured positions.
