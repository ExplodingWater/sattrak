"""Create small, synthetic circular-orbit fixtures for replay, not observed tracking data."""
from pathlib import Path
from datetime import datetime, timedelta, timezone
import math

folder = Path(__file__).resolve().parents[1] / "assets" / "orbits"
folder.mkdir(parents=True, exist_ok=True)
epoch = datetime(2026, 9, 13, tzinfo=timezone.utc)
for name, shift in (("demo", 0), ("demo-comparison", 0.002)):
    radius, inc = 7028.137, math.radians(51.6)
    n = math.sqrt(398600.4418 / radius**3)
    rows = []
    for seconds in range(0, 6001, 10):
        a = n * seconds + shift
        state = (radius*math.cos(a), radius*math.sin(a)*math.cos(inc), radius*math.sin(a)*math.sin(inc),
                 -radius*n*math.sin(a), radius*n*math.cos(a)*math.cos(inc), radius*n*math.cos(a)*math.sin(inc))
        utc = (epoch+timedelta(seconds=seconds)).strftime("%Y-%m-%dT%H:%M:%S")
        rows.append((utc, *(f"{v:.10f}" for v in state)))
    header = """CCSDS_OEM_VERS = 2.0
CREATION_DATE = 2026-09-13T00:00:00
ORIGINATOR = SATTRAK SYNTHETIC EXAMPLE
META_START
OBJECT_NAME = SYNTHETIC DEMO
OBJECT_ID = DEMO
CENTER_NAME = EARTH
REF_FRAME = EME2000
TIME_SYSTEM = UTC
START_TIME = 2026-09-13T00:00:00
STOP_TIME = 2026-09-13T01:40:00
INTERPOLATION = HERMITE
INTERPOLATION_DEGREE = 3
META_STOP
"""
    (folder / (name+".oem")).write_text(header + "\n".join(" ".join(row) for row in rows)+"\n", encoding="utf-8")
    if name == "demo":
        (folder / "demo.csv").write_text("# frame=EME2000\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n"+"\n".join(",".join(row) for row in rows)+"\n", encoding="utf-8")
print("Created two OEM examples and equivalent CSV in", folder)
