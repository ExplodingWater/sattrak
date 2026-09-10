#pragma once

#include "cfd/grid.hpp"
#include "cfd/euler_state.hpp"
#include "cfd/riemann_solvers.hpp"
#include "cfd/muscl_reconstruction.hpp"
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace aero::cfd {

struct SolverConfig {
    double cfl{0.6};                     // Courant-Friedrichs-Lewy stability number in (0, 1]
    LimiterType limiter{LimiterType::Minmod}; // MUSCL slope limiter
    bool useRoeSolver{true};             // true: Roe + entropy fix, false: Rusanov
    double convergenceTol{1e-6};         // Dimensionless full-state residual threshold
    size_t maxIterations{4000};          // Maximum iteration count
};

/**
 * @brief 2D Compressible Finite Volume Euler Equation Solver.
 */
class CFDSolver {
public:
    CFDSolver(StructuredGrid grid, PrimitiveState freestream, SolverConfig config = {})
        : grid_(std::move(grid)),
          freestream_(freestream),
          uFreestream_(ConservativeState::fromPrimitive(freestream_)),
          config_(config),
          U_(grid_.nx() * grid_.ny(), uFreestream_) {
        setCfl(config_.cfl);
        if (nx() == 0 || ny() == 0 || !uFreestream_.isPhysical() ||
            !std::isfinite(config_.convergenceTol) || config_.convergenceTol <= 0.0) {
            throw std::invalid_argument("Invalid CFD grid, freestream or convergence tolerance");
        }
        for (size_t j = 0; j < ny(); ++j) for (size_t i = 0; i < nx(); ++i) {
            const double volume = grid_.cell(i, j).volume;
            if (!std::isfinite(volume) || volume <= 0.0)
                throw std::invalid_argument("CFD cell volume must be positive");
        }
    }

    void setCfl(double cfl) {
        if (!std::isfinite(cfl) || cfl <= 0.0 || cfl > 1.0)
            throw std::invalid_argument("CFL must be in (0, 1]");
        config_.cfl = cfl;
    }

    [[nodiscard]] double residualNorm() const { return residualNorm_; }

    [[nodiscard]] size_t nx() const { return grid_.nx(); }
    [[nodiscard]] size_t ny() const { return grid_.ny(); }
    [[nodiscard]] const StructuredGrid& grid() const { return grid_; }
    [[nodiscard]] const std::vector<ConservativeState>& states() const { return U_; }

    [[nodiscard]] const ConservativeState& cellState(size_t i, size_t j) const {
        return U_[grid_.cellIndex(i, j)];
    }

    ConservativeState& cellState(size_t i, size_t j) {
        return U_[grid_.cellIndex(i, j)];
    }

    /**
     * @brief Computes a stable global timestep via the most restrictive cell CFL condition.
     */
    [[nodiscard]] double computeTimeStep() const {
        double dtMin = 1e10;
        for (size_t j = 0; j < ny(); ++j) {
            for (size_t i = 0; i < nx(); ++i) {
                const auto prim = cellState(i, j).toPrimitive();
                const double a = prim.speedOfSound();
                const double speed = prim.speed();
                const double lambdaMax = speed + a;

                const double cellVol = grid_.cell(i, j).volume;
                // Characteristic length scale = cell area / average perimeter
                const double dx = grid_.iFace(i, j).area;
                const double dy = grid_.jFace(i, j).area;
                const double charLen = cellVol / (dx + dy + 1e-12);

                const double dtCell = config_.cfl * (charLen / std::max(1e-4, lambdaMax));
                if (dtCell < dtMin) dtMin = dtCell;
            }
        }
        if (!std::isfinite(dtMin) || dtMin <= 0.0)
            throw std::runtime_error("No positive finite CFD timestep");
        return dtMin;
    }

    /**
     * @brief Evaluates spatial flux residuals R(U) for all cells: dU/dt = R(U).
     */
    [[nodiscard]] std::vector<ConservativeState> computeResiduals(const std::vector<ConservativeState>& stateVec) const {
        const size_t nx_ = nx();
        const size_t ny_ = ny();
        if (stateVec.size() != nx_ * ny_)
            throw std::invalid_argument("CFD state vector size does not match grid");
        std::vector<ConservativeState> residuals(nx_ * ny_, ConservativeState(0, 0, 0, 0));

        auto getState = [&](size_t i, size_t j) -> const ConservativeState& {
            return stateVec[grid_.cellIndex(i, j)];
        };

        // 1. Accumulate I-Faces (interfaces in x-direction between cell (i-1,j) and (i,j))
        for (size_t j = 0; j < ny_; ++j) {
            for (size_t i = 0; i <= nx_; ++i) {
                const Face& face = grid_.iFace(i, j);
                ConservativeState UL, UR;

                if (i == 0) {
                    // Supersonic Inflow: Left state is Dirichlet freestream
                    UL = uFreestream_;
                    UR = getState(0, j);
                } else if (i == nx_) {
                    // Supersonic Outflow: Extrapolate interior state
                    UL = getState(nx_ - 1, j);
                    UR = UL;
                } else {
                    // Interior interface: MUSCL reconstruction
                    if (config_.limiter == LimiterType::FirstOrder || i < 2 || i + 1 >= nx_) {
                        UL = getState(i - 1, j);
                        UR = getState(i, j);
                    } else {
                        PrimitiveState pL, pR;
                        reconstructPrimitiveInterface(
                            getState(i - 2, j).toPrimitive(),
                            getState(i - 1, j).toPrimitive(),
                            getState(i, j).toPrimitive(),
                            getState(i + 1, j).toPrimitive(),
                            config_.limiter, pL, pR);
                        UL = ConservativeState::fromPrimitive(pL);
                        UR = ConservativeState::fromPrimitive(pR);
                    }
                }

                // Numerical flux through vertical face
                ConservativeState flux = config_.useRoeSolver
                    ? computeRoeFlux(UL, UR, face.nx, face.ny)
                    : computeRusanovFlux(UL, UR, face.nx, face.ny);

                flux *= face.area;

                if (i > 0) {
                    residuals[grid_.cellIndex(i - 1, j)] -= flux;
                }
                if (i < nx_) {
                    residuals[grid_.cellIndex(i, j)] += flux;
                }
            }
        }

        // 2. Accumulate J-Faces (interfaces in y-direction between cell (i,j-1) and (i,j))
        for (size_t j = 0; j <= ny_; ++j) {
            for (size_t i = 0; i < nx_; ++i) {
                const Face& face = grid_.jFace(i, j);
                ConservativeState UL, UR;

                if (j == 0) {
                    // Bottom Boundary: Slip Wall (Euler tangency)
                    const ConservativeState uInt = getState(i, 0);
                    const PrimitiveState pInt = uInt.toPrimitive();

                    // Reflect velocity across face normal: v_ghost = v_int - 2 * (v_int . n) * n
                    const double vn = pInt.u * face.nx + pInt.v * face.ny;
                    const PrimitiveState pGhost{
                        pInt.rho,
                        pInt.u - 2.0 * vn * face.nx,
                        pInt.v - 2.0 * vn * face.ny,
                        pInt.p
                    };

                    UL = ConservativeState::fromPrimitive(pGhost);
                    UR = uInt;
                } else if (j == ny_) {
                    // Top Boundary: Supersonic Outflow / Farfield freestream
                    const ConservativeState uInt = getState(i, ny_ - 1);
                    UL = uInt;
                    UR = uFreestream_; // Inflow/Farfield freestream
                } else {
                    // Interior interface: MUSCL reconstruction
                    if (config_.limiter == LimiterType::FirstOrder || j < 2 || j + 1 >= ny_) {
                        UL = getState(i, j - 1);
                        UR = getState(i, j);
                    } else {
                        PrimitiveState pL, pR;
                        reconstructPrimitiveInterface(
                            getState(i, j - 2).toPrimitive(),
                            getState(i, j - 1).toPrimitive(),
                            getState(i, j).toPrimitive(),
                            getState(i, j + 1).toPrimitive(),
                            config_.limiter, pL, pR);
                        UL = ConservativeState::fromPrimitive(pL);
                        UR = ConservativeState::fromPrimitive(pR);
                    }
                }

                ConservativeState flux = config_.useRoeSolver
                    ? computeRoeFlux(UL, UR, face.nx, face.ny)
                    : computeRusanovFlux(UL, UR, face.nx, face.ny);

                flux *= face.area;

                if (j > 0) {
                    residuals[grid_.cellIndex(i, j - 1)] -= flux;
                }
                if (j < ny_) {
                    residuals[grid_.cellIndex(i, j)] += flux;
                }
            }
        }

        // Divide by cell volumes to get dU/dt = -Sum(Flux) / Volume
        for (size_t j = 0; j < ny_; ++j) {
            for (size_t i = 0; i < nx_; ++i) {
                const double invVol = 1.0 / grid_.cell(i, j).volume;
                residuals[grid_.cellIndex(i, j)] *= invVol;
            }
        }

        return residuals;
    }

    /**
     * @brief Marches the solution forward by one time step using SSP-RK2 (Heun's method).
     * @return L2 norm of the density change (residual)
     */
    double step() {
        double dt = computeTimeStep();
        const size_t totalCells = nx() * ny();

        // Stage 1: U^(1) = U^n + dt * R(U^n)
        const auto R0 = computeResiduals(U_);
        // Dimensionless steady residual across mass, both momenta and energy.
        // Unlike a state increment, this does not shrink just because CFL shrinks.
        const double speedScale = freestream_.speed() + freestream_.speedOfSound();
        const double scales[4] = {freestream_.rho, freestream_.rho * speedScale,
                                  freestream_.rho * speedScale, uFreestream_.E()};
        double area = 0.0, sum = 0.0;
        for (size_t j = 0; j < ny(); ++j) for (size_t i = 0; i < nx(); ++i)
            area += grid_.cell(i, j).volume;
        const double timeScale = std::sqrt(area) / speedScale;
        for (const auto& r : R0) for (size_t k = 0; k < 4; ++k) {
            const double value = r.u[k] * timeScale / scales[k];
            sum += value * value;
        }
        residualNorm_ = std::sqrt(sum / (4.0 * totalCells));
        if (!std::isfinite(residualNorm_)) throw std::runtime_error("Nonfinite CFD residual");
        std::vector<ConservativeState> U1(totalCells);
        std::vector<ConservativeState> next(totalCells);
        for (int retry = 0; retry < 16; ++retry, dt *= 0.5) {
            bool valid = true;
            for (size_t k = 0; k < totalCells; ++k) {
                U1[k] = U_[k] + R0[k] * dt;
                valid = valid && U1[k].isPhysical();
            }
            if (!valid) continue;

            // Stage 2: U^(n+1) = 0.5 * U^n + 0.5 * U^(1) + 0.5 * dt * R(U^(1))
            const auto R1 = computeResiduals(U1);
            double l2Sum = 0.0;
            for (size_t k = 0; k < totalCells; ++k) {
                const ConservativeState unew = U_[k] * 0.5 + U1[k] * 0.5 + R1[k] * (0.5 * dt);
                const double dRho = unew.rho() - U_[k].rho();
                l2Sum += dRho * dRho;
                next[k] = unew;
                valid = valid && unew.isPhysical();
            }
            if (!valid || !std::isfinite(l2Sum)) continue;
            U_.swap(next);
            return std::sqrt(l2Sum / static_cast<double>(totalCells));
        }
        throw std::runtime_error("CFD step failed positivity checks after 16 retries");
    }

    /**
     * @brief Solves until steady state convergence or max iterations reached.
     */
    bool solve(bool verbose = true) {
        if (verbose) {
            std::cout << "\n=======================================================\n";
            std::cout << "  Supersonic CFD Finite Volume Solver\n";
            std::cout << "  Grid: " << nx() << " x " << ny() << " (" << nx() * ny() << " cells)\n";
            std::cout << "  Scheme: " << (config_.useRoeSolver ? "Roe with Entropy Fix" : "Rusanov LLF") << "\n";
            std::cout << "  Limiter: " << (config_.limiter == LimiterType::Minmod ? "Minmod TVD" : "2nd-order") << "\n";
            std::cout << "  Freestream Mach: " << freestream_.mach() << " | P0: " << freestream_.p << " Pa\n";
            std::cout << "=======================================================\n";
        }

        for (size_t iter = 1; iter <= config_.maxIterations; ++iter) {
            step();
            const double res = residualNorm_;

            if (verbose && (iter % 100 == 0 || iter == 1 || iter == config_.maxIterations)) {
                std::cout << "  Iter " << std::setw(5) << iter
                          << " | Scaled steady residual: " << std::scientific << std::setprecision(4) << res
                          << " | dt: " << computeTimeStep() * 1e6 << " us\n";
            }

            if (res < config_.convergenceTol && iter > 50) {
                if (verbose) {
                    std::cout << ">>> Converged to tolerance " << config_.convergenceTol
                              << " at iteration " << iter << " <<<\n";
                }
                return true;
            }
        }

        if (verbose) {
            std::cout << "Reached max iterations (" << config_.maxIterations << ").\n";
        }
        return false;
    }

private:
    StructuredGrid grid_;
    PrimitiveState freestream_;
    ConservativeState uFreestream_;
    SolverConfig config_;
    std::vector<ConservativeState> U_;
    double residualNorm_{0.0};
};

} // namespace aero::cfd
