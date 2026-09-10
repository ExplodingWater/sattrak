#!/usr/bin/env python3
"""Validate the default M=2.5, 15-degree wedge against actual VTK cell data.

The historical filename is retained. Reports measured scalar errors;
it does not claim to measure the shock angle from the flowfield.
"""
import argparse
import math
from pathlib import Path


def solve_analytical_beta(mach, theta, gamma=1.4):
    if not (math.isfinite(mach) and mach > 1 and math.isfinite(theta)
            and 0 <= theta < math.pi / 2 and math.isfinite(gamma) and gamma > 1):
        raise ValueError("Invalid shock parameters")
    mu = math.asin(1 / mach)
    if theta == 0:
        return mu

    def residual(beta):
        return (2 / math.tan(beta) * (mach**2 * math.sin(beta)**2 - 1)
                / (mach**2 * (gamma + math.cos(2 * beta)) + 2) - math.tan(theta))

    low = mu
    for k in range(1, 4097):
        high = mu + (math.pi / 2 - mu) * k / 4096
        if residual(high) >= 0:
            break
        low = high
    else:
        raise ValueError("No attached weak shock for this deflection")
    for _ in range(60):
        mid = (low + high) / 2
        if residual(mid) >= 0:
            high = mid
        else:
            low = mid
    return (low + high) / 2


def parse_vtk_metrics(path):
    tokens = Path(path).read_text().split()
    i = tokens.index("DIMENSIONS")
    nx, ny, nz = map(int, tokens[i + 1:i + 4])
    if nz != 1 or nx < 12 or ny < 3:
        raise ValueError("Expected a 2D wedge grid with at least 11 x 2 cells")
    nx, ny = nx - 1, ny - 1
    count = int(tokens[tokens.index("CELL_DATA") + 1])
    if count != nx * ny:
        raise ValueError("Cell count does not match grid dimensions")
    fields = {}
    for name in ("Pressure_Pa", "Density_kg_m3", "Mach_Number"):
        i = tokens.index(name)
        start = tokens.index("LOOKUP_TABLE", i) + 2
        values = [float(v) for v in tokens[start:start + count]]
        if len(values) != count or any(not math.isfinite(v) or v <= 0 for v in values):
            raise ValueError(f"Invalid {name} data")
        fields[name] = values
    return nx, ny, fields


def validate(path):
    nx, _, fields = parse_vtk_metrics(path)
    beta = solve_analytical_beta(2.5, math.radians(15))
    mn_sq = (2.5 * math.sin(beta))**2
    pressure = 101325 * (1 + 2 * 1.4 / 2.4 * (mn_sq - 1))
    density = 101325 / (287.058 * 288.15) * 2.4 * mn_sq / (0.4 * mn_sq + 2)
    mach = math.sqrt((1 + 0.2 * mn_sq) / (1.4 * mn_sq - 0.2)) / math.sin(beta - math.radians(15))
    sample = nx + nx - 10  # i=nx-10, j=1, matching the C++ executable.
    print(f"Dataset: {path}\nDefault case: M=2.5, theta=15 deg, gamma=1.4")
    print(f"Analytical beta: {math.degrees(beta):.6f} deg; numerical angle: not measured")
    passed = True
    for name, reference, tolerance in (("Pressure_Pa", pressure, 0.01),
                                       ("Density_kg_m3", density, 0.03),
                                       ("Mach_Number", mach, 0.03)):
        measured = fields[name][sample]
        error = abs(measured / reference - 1)
        ok = error <= tolerance
        passed &= ok
        print(f"{name}: measured={measured:.6f}, reference={reference:.6f}, "
              f"error={error:.3%}, tolerance={tolerance:.1%}: {'PASS' if ok else 'FAIL'}")
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vtk", nargs="?", default="supersonic_wedge_shock.vtk")
    args = parser.parse_args()
    try:
        return 0 if validate(args.vtk) else 1
    except (OSError, ValueError) as error:
        print(f"Validation failed: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
