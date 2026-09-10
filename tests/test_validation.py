"""Regression tests for the independent VTK validation/reporting path."""
import contextlib
import io
import math
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from plot_shock_validation import parse_vtk_metrics, solve_analytical_beta, validate


class ValidationTests(unittest.TestCase):
    def test_weak_and_detached_branches(self):
        self.assertAlmostEqual(math.degrees(solve_analytical_beta(2.5, math.radians(15))), 36.9449, places=4)
        with self.assertRaises(ValueError):
            solve_analytical_beta(1.2, math.radians(25))

    def test_actual_data_and_failure_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.vtk"

            def write(pressure="2.50019450454e5"):
                content = "DIMENSIONS 12 3 1\nCELL_DATA 22\n"
                for name, value in (("Pressure_Pa", pressure), ("Density_kg_m3", "2.286481"),
                                    ("Mach_Number", "1.873526")):
                    content += f"SCALARS {name} double 1\nLOOKUP_TABLE default\n" + (value + "\n") * 22
                path.write_text(content)

            write()
            nx, ny, _ = parse_vtk_metrics(path)
            self.assertEqual((nx, ny), (11, 2))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertTrue(validate(path))
                write("1.01325e5")
                self.assertFalse(validate(path))
            for invalid in ("nan", "inf", "-1"):
                write(invalid)
                with self.assertRaises(ValueError):
                    parse_vtk_metrics(path)
            path.write_text("DIMENSIONS 12 3 1\nCELL_DATA 21\n")
            with self.assertRaises(ValueError):
                parse_vtk_metrics(path)


if __name__ == "__main__":
    unittest.main()
