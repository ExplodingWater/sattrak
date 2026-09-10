#include "cfd/cfd_solver.hpp"
#include "cfd/oblique_shock.hpp"
#include "cfd/vtk_exporter.hpp"
#include "common/file_utils.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

int main(int argc, char* argv[]) try {
    (void)argc;
    (void)argv;
    std::cout << "====================================================================\n";
    std::cout << "        2D Compressible Aerodynamics CFD Solver                     \n";
    std::cout << "     Finite Volume Method with Roe Approximate Riemann Solver       \n";
    std::cout << "====================================================================\n\n";

    // 1. Aerodynamic Flow Conditions
    const double M_inf = 2.5;                     // Freestream Mach number
    const double p_inf = 101325.0;                 // Freestream pressure [Pa]
    const double T_inf = 288.15;                   // Freestream temperature [K]
    const double rho_inf = p_inf / (aero::constants::R_SPECIFIC_AIR * T_inf); // ~1.225 kg/m^3
    const double a_inf = std::sqrt(aero::constants::GAMMA_AIR * p_inf / rho_inf);
    const double u_inf = M_inf * a_inf;            // Freestream velocity ~851 m/s

    aero::cfd::PrimitiveState freestream(rho_inf, u_inf, 0.0, p_inf);

    // 2. Geometry & Mesh: 15-degree Compression Ramp (Oblique Shock Generator)
    const double thetaDeg = 15.0;
    const double thetaRad = thetaDeg * aero::constants::PI / 180.0;
    const double rampX = 0.4; // Ramp begins at x = 0.4 m
    const size_t nx = 90;
    const size_t ny = 45;

    std::cout << "Generating 2D Curvilinear Body-Fitted Mesh...\n";
    std::cout << "  Domain: [0.0, 1.8] m x [0.0, 0.9] m\n";
    std::cout << "  Wedge Deflection Angle: " << thetaDeg << " degrees starting at x = " << rampX << " m\n";
    std::cout << "  Resolution: " << nx << " x " << ny << " = " << nx * ny << " cells\n\n";

    auto grid = aero::cfd::StructuredGrid::createCompressionRamp(
        nx, ny, 0.0, 1.8, 0.9, rampX, thetaDeg);

    // 3. Solver Setup
    aero::cfd::SolverConfig solverConfig;
    solverConfig.cfl = 0.55;
    solverConfig.limiter = aero::cfd::LimiterType::Minmod;
    solverConfig.useRoeSolver = true; // High-resolution Roe scheme with entropy fix
    solverConfig.convergenceTol = 2e-5;
    solverConfig.maxIterations = 4000;

    aero::cfd::CFDSolver solver(std::move(grid), freestream, solverConfig);

    // 4. Run Time-Marching Simulation
    if (!solver.solve(true)) {
        std::cerr << "CFD did not converge; benchmark not verified.\n";
        return 1;
    }

    // 5. Analytical Verification & Ground-Truth Comparison
    const auto shock = aero::cfd::weakObliqueShock(M_inf, thetaRad);
    if (!shock) throw std::runtime_error("No attached weak shock for this configuration");
    const double betaAnalyticalDeg = shock->betaRad * 180.0 / aero::constants::PI;
    const double p2Exact = p_inf * shock->pressureRatio;
    const double rho2Exact = rho_inf * shock->densityRatio;
    const double M2Exact = shock->mach2;

    // Sample CFD post-shock state along the wedge surface (near outflow, j=1)
    const size_t sampleI = nx - 10;
    const auto postShockPrim = solver.cellState(sampleI, 1).toPrimitive();

    const double pressureErrorPct = std::abs(postShockPrim.p - p2Exact) / p2Exact * 100.0;
    const double densityErrorPct  = std::abs(postShockPrim.rho - rho2Exact) / rho2Exact * 100.0;
    const double machErrorPct     = std::abs(postShockPrim.mach() - M2Exact) / M2Exact * 100.0;

    std::cout << "\n====================================================================\n";
    std::cout << "           CFD vs Exact Rankine-Hugoniot Analytical Benchmark       \n";
    std::cout << "====================================================================\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Upstream Mach Number M1          : " << M_inf << "\n";
    std::cout << "Wedge Deflection Angle (theta)   : " << thetaDeg << " deg\n";
    std::cout << "Exact Oblique Shock Angle (beta) : " << betaAnalyticalDeg << " deg\n\n";

    std::cout << "Metric                    Analytical Exact      CFD Numerical       Error (%)\n";
    std::cout << "--------------------------------------------------------------------\n";
    std::cout << "Post-Shock Pressure [Pa]  " << std::setw(15) << p2Exact
              << "   " << std::setw(15) << postShockPrim.p
              << "   " << std::setw(8) << std::setprecision(2) << pressureErrorPct << " %\n";
    std::cout << std::setprecision(4);
    std::cout << "Post-Shock Density [kg/m3]" << std::setw(15) << rho2Exact
              << "   " << std::setw(15) << postShockPrim.rho
              << "   " << std::setw(8) << std::setprecision(2) << densityErrorPct << " %\n";
    std::cout << std::setprecision(4);
    std::cout << "Downstream Mach Number M2 " << std::setw(15) << M2Exact
              << "   " << std::setw(15) << postShockPrim.mach()
              << "   " << std::setw(8) << std::setprecision(2) << machErrorPct << " %\n";
    std::cout << "--------------------------------------------------------------------\n";

    // 6. Export Results to Project Directory (data/cfd) with Date in Filename
    const std::string cfdDir = "data/cfd";
    aero::utils::ensureDirectoryExists(cfdDir);
    const std::string dateStr = aero::utils::getDateString();

    const std::string datedCsvFile = cfdDir + "/cfd_wedge_shock_" + dateStr + ".csv";
    if (!aero::cfd::VTKExporter::exportCSV(solver, datedCsvFile)) return 1;

    const std::string datedVtkFile = cfdDir + "/supersonic_wedge_shock_" + dateStr + ".vtk";
    if (!aero::cfd::VTKExporter::exportStructuredGrid(solver, datedVtkFile)) return 1;

    // Also export root copies for backward compatibility
    if (!aero::cfd::VTKExporter::exportStructuredGrid(solver, "supersonic_wedge_shock.vtk")) return 1;

    std::cout << "\nVisual Inspection Files:\n";
    std::cout << "  - Flowfield CSV: " << datedCsvFile << "\n";
    std::cout << "  - ParaView VTK : " << datedVtkFile << "\n";
    std::cout << "====================================================================\n";

    return 0;
} catch (const std::exception& e) {
    std::cerr << "CFD failed: " << e.what() << '\n';
    return 1;
}
