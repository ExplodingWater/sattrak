# Reproduced benchmark results

Generated 2026-09-09 on Windows using `python scripts/generate_benchmarks.py --bin-dir build/Release`.

All three test executables passed before these simulations ran. The report uses newly generated output in an isolated temporary directory.

## ADCS default 60-second simulation

```text
True Gyro Bias: [0.025, -0.018, 0.012] rad/s
Final Attitude Pointing Error : 0.0852 deg
Final Spacecraft Angular Rate : 0.1173 deg/s
True Gyro Bias [x,y,z]        : [0.0250, -0.0180, 0.0122] rad/s
Estimated Gyro Bias [x,y,z]   : [0.0249, -0.0177, 0.0119] rad/s
Residual Bias Estimation Error: 0.0005 rad/s
Final 3-Sigma Attitude Bound  : 0.1133 deg
```

## CFD default M=2.5, 15-degree wedge

```text
>>> Converged to tolerance 2.0000e-05 at iteration 1500 <<<
Exact Oblique Shock Angle (beta) : 36.9449 deg
Post-Shock Pressure [Pa]      250019.4505       250043.5755       0.01 %
Post-Shock Density [kg/m3]         2.2865            2.2406       2.01 %
Downstream Mach Number M2          1.8735            1.8355       2.03 %
```

Pressure, density and Mach were independently checked from the exported VTK at cell i=nx-10, j=1 against tolerances of 1%, 3% and 3%. Shock angle is analytical only; it is not measured from the numerical field. These are single-case regressions, not a mesh-convergence or filter-consistency study.
