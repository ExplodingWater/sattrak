# Research-related satellite catalog

Public orbital elements downloaded from CelesTrak: science (Space & Earth Science), resource (Earth Resources) and weather groups. All returned objects are retained, with duplicates removed by NORAD catalog ID. These groups are a practical research-related selection, not an exhaustive inventory of every research satellite or a guarantee of operational status. See manifest.json for retrieval date, URLs and counts.

Source: https://celestrak.org/NORAD/elements/
Data format: https://celestrak.org/NORAD/documentation/gp-data-formats.php

Propagation: Daniel Warner's libsgp4, Apache-2.0. Sources and license in external/sgp4; license also accompanies these runtime assets. Positions are predictions from the bundled element sets, not a live service. No automatic network access occurs in ag.exe. Refresh manually using scripts/update_research_catalog.py, then rebuild to copy updated assets.

The catalog starts at epoch.txt UTC and advances with simulation time. SGP4 TEME positions are transformed to Earth-fixed coordinates using GMST and then aligned with the displayed Earth rotation. This does not include high-precision Earth orientation corrections. Old elements and future propagation lose accuracy.
