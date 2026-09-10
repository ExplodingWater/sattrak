#!/usr/bin/env python3
"""Run tests and both default simulations, then regenerate benchmark documentation."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import platform
import subprocess
import tempfile
from plot_shock_validation import validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("docs/BENCHMARKS.md"))
    args = parser.parse_args()
    binaries = args.bin_dir.resolve()
    suffix = ".exe" if platform.system() == "Windows" else ""
    with tempfile.TemporaryDirectory(prefix="benchmark-") as directory:
        def run(name):
            result = subprocess.run([str(binaries / (name + suffix))], cwd=directory,
                                    capture_output=True, text=True, check=True, timeout=120)
            return result.stdout

        for name in ("test_adcs", "test_cfd", "test_regressions"):
            run(name)
        adcs = run("mekf_adcs")
        cfd = run("supersonic_cfd")
        if not validate(Path(directory) / "supersonic_wedge_shock.vtk"):
            raise RuntimeError("Measured CFD benchmark failed")
        adcs_lines = [line for line in adcs.splitlines() if line.startswith((
            "Final Attitude", "Final Spacecraft", "True Gyro", "Estimated Gyro",
            "Residual Bias", "Final 3-Sigma"))]
        cfd_lines = [line for line in cfd.splitlines() if line.startswith((
            ">>> Converged", "Post-Shock", "Downstream Mach", "Exact Oblique"))]
        if len(adcs_lines) < 6 or len(cfd_lines) < 5:
            raise RuntimeError("Simulation summary format changed; report not generated")
        content = ("# Reproduced benchmark results\n\n"
                   f"Generated {datetime.now(timezone.utc).date()} on {platform.system()} "
                   f"using `python scripts/generate_benchmarks.py --bin-dir {args.bin_dir.as_posix()}`.\n\n"
                   "All three test executables passed before these simulations ran. "
                   "The report uses newly generated output in an isolated temporary directory.\n\n"
                   "## ADCS default 60-second simulation\n\n```text\n" +
                   "\n".join(adcs_lines) + "\n```\n\n"
                   "## CFD default M=2.5, 15-degree wedge\n\n```text\n" +
                   "\n".join(cfd_lines) + "\n```\n\n"
                   "Pressure, density and Mach were independently checked from the exported VTK "
                   "at cell i=nx-10, j=1 against tolerances of 1%, 3% and 3%. "
                   "Shock angle is analytical only; it is not measured from the numerical field. "
                   "These are single-case regressions, not a mesh-convergence or filter-consistency study.\n")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
        print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
