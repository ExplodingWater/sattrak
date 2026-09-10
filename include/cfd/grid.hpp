#pragma once

#include "common/math_types.hpp"
#include "common/constants.hpp"
#include <vector>
#include <cmath>
#include <cassert>

namespace aero::cfd {

struct Face {
    double nx{1.0};      // Unit normal x
    double ny{0.0};      // Unit normal y
    double area{1.0};    // Face length/area [m]
};

struct Cell {
    double x{0.0};       // Cell centroid x [m]
    double y{0.0};       // Cell centroid y [m]
    double volume{1.0};  // Cell area/volume [m^2]
};

/**
 * @brief 2D Structured Curvilinear Grid for Finite Volume Aerodynamics.
 */
class StructuredGrid {
public:
    StructuredGrid(size_t nx, size_t ny)
        : nx_(nx), ny_(ny),
          xNodes_((nx + 1) * (ny + 1)),
          yNodes_((nx + 1) * (ny + 1)),
          cells_(nx * ny),
          iFaces_((nx + 1) * ny),
          jFaces_(nx * (ny + 1)) {}

    [[nodiscard]] size_t nx() const { return nx_; }
    [[nodiscard]] size_t ny() const { return ny_; }

    [[nodiscard]] size_t nodeIndex(size_t i, size_t j) const {
        return j * (nx_ + 1) + i;
    }

    [[nodiscard]] size_t cellIndex(size_t i, size_t j) const {
        return j * nx_ + i;
    }

    [[nodiscard]] size_t iFaceIndex(size_t i, size_t j) const {
        return j * (nx_ + 1) + i;
    }

    [[nodiscard]] size_t jFaceIndex(size_t i, size_t j) const {
        return j * nx_ + i;
    }

    double& xNode(size_t i, size_t j) { return xNodes_[nodeIndex(i, j)]; }
    double& yNode(size_t i, size_t j) { return yNodes_[nodeIndex(i, j)]; }
    [[nodiscard]] double xNode(size_t i, size_t j) const { return xNodes_[nodeIndex(i, j)]; }
    [[nodiscard]] double yNode(size_t i, size_t j) const { return yNodes_[nodeIndex(i, j)]; }

    Cell& cell(size_t i, size_t j) { return cells_[cellIndex(i, j)]; }
    [[nodiscard]] const Cell& cell(size_t i, size_t j) const { return cells_[cellIndex(i, j)]; }

    Face& iFace(size_t i, size_t j) { return iFaces_[iFaceIndex(i, j)]; }
    [[nodiscard]] const Face& iFace(size_t i, size_t j) const { return iFaces_[iFaceIndex(i, j)]; }

    Face& jFace(size_t i, size_t j) { return jFaces_[jFaceIndex(i, j)]; }
    [[nodiscard]] const Face& jFace(size_t i, size_t j) const { return jFaces_[jFaceIndex(i, j)]; }

    /**
     * @brief Computes cell volumes, face areas, and face unit normals from node coordinates.
     */
    void computeMetrics() {
        // 1. I-Faces (vertical interfaces between (i,j) and (i+1,j), from node (i,j) to (i,j+1))
        for (size_t j = 0; j < ny_; ++j) {
            for (size_t i = 0; i <= nx_; ++i) {
                const double x0 = xNode(i, j);
                const double y0 = yNode(i, j);
                const double x1 = xNode(i, j + 1);
                const double y1 = yNode(i, j + 1);

                const double dx = x1 - x0;
                const double dy = y1 - y0;
                const double len = std::sqrt(dx * dx + dy * dy);

                Face& f = iFace(i, j);
                f.area = len;
                // Normal pointing in positive i-direction (to the right of vector from (x0,y0) to (x1,y1)):
                if (len > 1e-14) {
                    f.nx = dy / len;
                    f.ny = -dx / len;
                } else {
                    f.nx = 1.0;
                    f.ny = 0.0;
                }
            }
        }

        // 2. J-Faces (horizontal interfaces between (i,j) and (i,j+1), from node (i,j) to (i+1,j))
        for (size_t j = 0; j <= ny_; ++j) {
            for (size_t i = 0; i < nx_; ++i) {
                const double x0 = xNode(i, j);
                const double y0 = yNode(i, j);
                const double x1 = xNode(i + 1, j);
                const double y1 = yNode(i + 1, j);

                const double dx = x1 - x0;
                const double dy = y1 - y0;
                const double len = std::sqrt(dx * dx + dy * dy);

                Face& f = jFace(i, j);
                f.area = len;
                // Normal pointing in positive j-direction (to the left of vector from (x0,y0) to (x1,y1)):
                if (len > 1e-14) {
                    f.nx = -dy / len;
                    f.ny = dx / len;
                } else {
                    f.nx = 0.0;
                    f.ny = 1.0;
                }
            }
        }

        // 3. Cells: centroids and volumes (Shoelace formula for quadrilateral)
        for (size_t j = 0; j < ny_; ++j) {
            for (size_t i = 0; i < nx_; ++i) {
                const double x0 = xNode(i, j);         // BL
                const double y0 = yNode(i, j);
                const double x1 = xNode(i + 1, j);     // BR
                const double y1 = yNode(i + 1, j);
                const double x2 = xNode(i + 1, j + 1); // TR
                const double y2 = yNode(i + 1, j + 1);
                const double x3 = xNode(i, j + 1);     // TL
                const double y3 = yNode(i, j + 1);

                Cell& c = cell(i, j);
                c.x = 0.25 * (x0 + x1 + x2 + x3);
                c.y = 0.25 * (y0 + y1 + y2 + y3);

                // Shoelace formula:
                const double area = 0.5 * std::abs(
                    (x0 * y1 - x1 * y0) +
                    (x1 * y2 - x2 * y1) +
                    (x2 * y3 - x3 * y2) +
                    (x3 * y0 - x0 * y3)
                );
                c.volume = std::max(1e-14, area);
            }
        }
    }

    /**
     * @brief Generates a compression ramp (wedge) grid.
     * @param nx Number of cells in x
     * @param ny Number of cells in y
     * @param xMin Inflow coordinate [m]
     * @param xMax Outflow coordinate [m]
     * @param yMax Domain height [m]
     * @param rampX Location where the wedge ramp begins [m]
     * @param thetaDeg Wedge angle in degrees (e.g. 15.0 deg)
     */
    static StructuredGrid createCompressionRamp(
        size_t nx, size_t ny,
        double xMin, double xMax, double yMax,
        double rampX, double thetaDeg)
    {
        StructuredGrid grid(nx, ny);
        const double thetaRad = thetaDeg * constants::PI / 180.0;
        const double tanTheta = std::tan(thetaRad);

        const double dx = (xMax - xMin) / static_cast<double>(nx);

        for (size_t i = 0; i <= nx; ++i) {
            const double x = xMin + i * dx;
            const double yBottom = (x >= rampX) ? (x - rampX) * tanTheta : 0.0;

            for (size_t j = 0; j <= ny; ++j) {
                const double eta = static_cast<double>(j) / static_cast<double>(ny);
                grid.xNode(i, j) = x;
                grid.yNode(i, j) = yBottom + eta * (yMax - yBottom);
            }
        }

        grid.computeMetrics();
        return grid;
    }

    /**
     * @brief Generates a standard Cartesian rectangular grid.
     */
    static StructuredGrid createCartesian(
        size_t nx, size_t ny,
        double xMin, double xMax, double yMin, double yMax)
    {
        StructuredGrid grid(nx, ny);
        const double dx = (xMax - xMin) / static_cast<double>(nx);
        const double dy = (yMax - yMin) / static_cast<double>(ny);

        for (size_t i = 0; i <= nx; ++i) {
            for (size_t j = 0; j <= ny; ++j) {
                grid.xNode(i, j) = xMin + i * dx;
                grid.yNode(i, j) = yMin + j * dy;
            }
        }

        grid.computeMetrics();
        return grid;
    }

private:
    size_t nx_;
    size_t ny_;
    std::vector<double> xNodes_;
    std::vector<double> yNodes_;
    std::vector<Cell> cells_;
    std::vector<Face> iFaces_;
    std::vector<Face> jFaces_;
};

} // namespace aero::cfd
