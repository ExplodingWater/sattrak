#pragma once

#include "cfd/cfd_solver.hpp"
#include <fstream>
#include <string>
#include <iomanip>
#include <cmath>
#include <iostream>

namespace aero::cfd {

/**
 * @brief Exports 2D Structured Flowfield to ParaView Legacy VTK format (.vtk).
 */
class VTKExporter {
public:
    static bool exportStructuredGrid(const CFDSolver& solver, const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: Unable to open file " << filename << " for VTK export.\n";
            return false;
        }

        const auto& grid = solver.grid();
        const size_t nx = grid.nx();
        const size_t ny = grid.ny();
        const size_t numPoints = (nx + 1) * (ny + 1);
        const size_t numCells = nx * ny;

        // 1. VTK Header
        out << "# vtk DataFile Version 3.0\n";
        out << "2D Compressible Aerodynamics Solution\n";
        out << "ASCII\n";
        out << "DATASET STRUCTURED_GRID\n";
        out << "DIMENSIONS " << (nx + 1) << " " << (ny + 1) << " 1\n";

        // 2. Grid Point Coordinates
        out << "POINTS " << numPoints << " double\n";
        out << std::setprecision(6) << std::fixed;
        for (size_t j = 0; j <= ny; ++j) {
            for (size_t i = 0; i <= nx; ++i) {
                out << grid.xNode(i, j) << " " << grid.yNode(i, j) << " 0.0\n";
            }
        }

        // 3. Cell Data Fields
        out << "\nCELL_DATA " << numCells << "\n";

        // Mach Number
        out << "SCALARS Mach_Number double 1\n";
        out << "LOOKUP_TABLE default\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                out << prim.mach() << "\n";
            }
        }

        // Static Pressure [Pa]
        out << "\nSCALARS Pressure_Pa double 1\n";
        out << "LOOKUP_TABLE default\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                out << prim.p << "\n";
            }
        }

        // Static Density [kg/m^3]
        out << "\nSCALARS Density_kg_m3 double 1\n";
        out << "LOOKUP_TABLE default\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                out << prim.rho << "\n";
            }
        }

        // Static Temperature [K]
        out << "\nSCALARS Temperature_K double 1\n";
        out << "LOOKUP_TABLE default\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                out << prim.temperature() << "\n";
            }
        }

        // Numerical Schlieren (|grad(rho)|) to highlight shock waves
        out << "\nSCALARS Numerical_Schlieren double 1\n";
        out << "LOOKUP_TABLE default\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const double rhoCenter = solver.cellState(i, j).toPrimitive().rho;
                const double rhoEast   = (i + 1 < nx) ? solver.cellState(i + 1, j).toPrimitive().rho : rhoCenter;
                const double rhoWest   = (i > 0)      ? solver.cellState(i - 1, j).toPrimitive().rho : rhoCenter;
                const double rhoNorth  = (j + 1 < ny) ? solver.cellState(i, j + 1).toPrimitive().rho : rhoCenter;
                const double rhoSouth  = (j > 0)      ? solver.cellState(i, j - 1).toPrimitive().rho : rhoCenter;

                const double dx = (i + 1 < nx && i > 0) ? (grid.cell(i + 1, j).x - grid.cell(i - 1, j).x) : 0.01;
                const double dy = (j + 1 < ny && j > 0) ? (grid.cell(i, j + 1).y - grid.cell(i, j - 1).y) : 0.01;

                const double gradX = (rhoEast - rhoWest) / std::max(1e-5, dx);
                const double gradY = (rhoNorth - rhoSouth) / std::max(1e-5, dy);
                const double schlieren = std::sqrt(gradX * gradX + gradY * gradY);

                out << schlieren << "\n";
            }
        }

        // Velocity Vectors [m/s]
        out << "\nVECTORS Velocity_m_s double\n";
        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                out << prim.u << " " << prim.v << " 0.0\n";
            }
        }

        out.flush();
        if (!out) return false;
        std::cout << "Successfully exported ParaView VTK dataset to: " << filename << "\n";
        return true;
    }

    /**
     * @brief Exports 2D Flowfield Cell Solution to CSV format.
     */
    static bool exportCSV(const CFDSolver& solver, const std::string& filename) {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Error: Unable to open file " << filename << " for CSV export.\n";
            return false;
        }

        const auto& grid = solver.grid();
        const size_t nx = grid.nx();
        const size_t ny = grid.ny();

        out << "cell_x,cell_y,mach,pressure_pa,density_kg_m3,temperature_k,u_m_s,v_m_s,numerical_schlieren\n";
        out << std::setprecision(6) << std::fixed;

        for (size_t j = 0; j < ny; ++j) {
            for (size_t i = 0; i < nx; ++i) {
                const auto prim = solver.cellState(i, j).toPrimitive();
                const double x = grid.cell(i, j).x;
                const double y = grid.cell(i, j).y;

                const double rhoCenter = prim.rho;
                const double rhoEast   = (i + 1 < nx) ? solver.cellState(i + 1, j).toPrimitive().rho : rhoCenter;
                const double rhoWest   = (i > 0)      ? solver.cellState(i - 1, j).toPrimitive().rho : rhoCenter;
                const double rhoNorth  = (j + 1 < ny) ? solver.cellState(i, j + 1).toPrimitive().rho : rhoCenter;
                const double rhoSouth  = (j > 0)      ? solver.cellState(i, j - 1).toPrimitive().rho : rhoCenter;

                const double dx = (i + 1 < nx && i > 0) ? (grid.cell(i + 1, j).x - grid.cell(i - 1, j).x) : 0.01;
                const double dy = (j + 1 < ny && j > 0) ? (grid.cell(i, j + 1).y - grid.cell(i, j - 1).y) : 0.01;

                const double gradX = (rhoEast - rhoWest) / std::max(1e-5, dx);
                const double gradY = (rhoNorth - rhoSouth) / std::max(1e-5, dy);
                const double schlieren = std::sqrt(gradX * gradX + gradY * gradY);

                out << x << "," << y << ","
                    << prim.mach() << ","
                    << prim.p << ","
                    << prim.rho << ","
                    << prim.temperature() << ","
                    << prim.u << ","
                    << prim.v << ","
                    << schlieren << "\n";
            }
        }

        out.flush();
        if (!out) return false;
        std::cout << "Successfully exported CFD flowfield CSV to: " << filename << "\n";
        return true;
    }
};

} // namespace aero::cfd
