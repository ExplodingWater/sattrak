#include "gui/cfd_view.hpp"
#include "cfd/oblique_shock.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include "imgui.h"

namespace gui {

CFDView::CFDView() {
}

CFDView::~CFDView() {
    cleanup();
}

bool CFDView::init() {
    m_pixelBuffer.resize(m_nx * m_ny * 4, 0);
    m_flowTexId = createDynamicRGBATexture(m_nx, m_ny);

    resetSimulation();
    return true;
}

void CFDView::resetSimulation() {
    m_hasSample = false;
    m_solverError.clear();
    m_grid = std::make_unique<aero::cfd::StructuredGrid>(
        aero::cfd::StructuredGrid::createCompressionRamp(m_nx, m_ny, 0.0, 2.0, 1.0, 0.5, m_rampAngleDeg)
    );

    aero::cfd::PrimitiveState inflow(1.225, m_machInf * 340.29, 0.0, 101325.0);
    aero::cfd::SolverConfig config;
    config.cfl = m_cfl;
    config.useRoeSolver = true;
    config.limiter = aero::cfd::LimiterType::Minmod;

    m_solver = std::make_unique<aero::cfd::CFDSolver>(*m_grid, inflow, config);

    updateAnalyticalTheory();
    updateFlowTexture();
}

void CFDView::updateAnalyticalTheory() {
    const auto shock = aero::cfd::weakObliqueShock(m_machInf,
        m_rampAngleDeg * aero::constants::PI / 180.0);
    m_hasAnalyticalSolution = shock.has_value();
    m_theoryBetaDeg = shock ? shock->betaRad * 180.0 / aero::constants::PI : 0;
    m_theoryM2 = shock ? shock->mach2 : 0;
    m_theoryP2P1 = shock ? shock->pressureRatio : 0;
    m_theoryT2T1 = shock ? shock->temperatureRatio : 0;
}

void CFDView::update(float /*dt*/) {
    if (!m_solver) return;
    if (m_isPaused) { updateFlowTexture(); return; }

    try {
    for (int step = 0; step < m_substepsPerFrame; ++step) {
        m_solver->step();
    }
    } catch (const std::exception& error) {
        m_solverError = error.what();
        m_isPaused = true;
        return;
    }

    // Probe numerical post-shock state at downstream wall
    size_t probeI = m_nx - 10;
    size_t probeJ = 1;
    const auto& U = m_solver->cellState(probeI, probeJ);
    aero::cfd::PrimitiveState W = U.toPrimitive();
    m_numM2 = W.mach();
    m_numP2P1 = W.p / 101325.0;
    m_numT2T1 = m_numP2P1 / (W.rho / 1.225);
    m_hasSample = true;

    if (m_hasAnalyticalSolution && m_theoryM2 > 0.0) {
        m_numErrorM2Percent = std::abs(m_numM2 - m_theoryM2) / m_theoryM2 * 100.0;
        m_numErrorPPercent = std::abs(m_numP2P1 - m_theoryP2P1) / m_theoryP2P1 * 100.0;
    }

    updateFlowTexture();
}

void CFDView::updateFlowTexture() {
    if (!m_solver) return;

    float minVal = 1e9f, maxVal = -1e9f;
    std::vector<float> field(m_nx * m_ny, 0.0f);

    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            const auto& U = m_solver->cellState(i, j);
            aero::cfd::PrimitiveState W = U.toPrimitive();
            float val = 0.0f;

            if (m_contourType == CFDContourType::MachNumber) {
                val = (float)W.mach();
            } else if (m_contourType == CFDContourType::StaticPressure) {
                val = (float)(W.p / 101325.0);
            } else if (m_contourType == CFDContourType::Density) {
                val = (float)(W.rho / 1.225);
            } else { // Numerical Schlieren: |grad(rho)|
                float drhox = 0.0f, drhoy = 0.0f;
                if (i > 0 && i < m_nx - 1) {
                    drhox = (float)(m_solver->cellState(i+1, j).rho() - m_solver->cellState(i-1, j).rho());
                }
                if (j > 0 && j < m_ny - 1) {
                    drhoy = (float)(m_solver->cellState(i, j+1).rho() - m_solver->cellState(i, j-1).rho());
                }
                val = std::sqrt(drhox * drhox + drhoy * drhoy);
            }

            field[j * m_nx + i] = val;
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
        }
    }

    if (maxVal <= minVal) maxVal = minVal + 1.0f;

    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            float norm = (field[j * m_nx + i] - minVal) / (maxVal - minVal);
            ColorRGB rgb;

            if (m_colormapIdx == 0) {
                rgb = sampleTurboColormap(norm);
            } else if (m_colormapIdx == 1) {
                rgb = sampleInfernoColormap(norm);
            } else {
                rgb = sampleCoolWarmColormap(norm);
            }

            int row = (m_ny - 1 - j);
            int idx = (row * m_nx + i) * 4;
            m_pixelBuffer[idx + 0] = rgb.r;
            m_pixelBuffer[idx + 1] = rgb.g;
            m_pixelBuffer[idx + 2] = rgb.b;
            m_pixelBuffer[idx + 3] = 255;
        }
    }

    updateDynamicRGBATexture(m_flowTexId, m_nx, m_ny, m_pixelBuffer.data());
}

void CFDView::drawUI() {
    drawFlowfieldViewport();
    drawControlPanel();
    drawBenchmarkTable();
}

void CFDView::drawFlowfieldViewport() {
    ImGui::SetNextWindowPos(ImVec2(10, 35), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(980, 520), ImGuiCond_FirstUseEver);

    ImGui::Begin("2D Compressible Supersonic Flowfield", nullptr, ImGuiWindowFlags_None);

    ImGui::Text("Active Contour: %s | Mach Inflow: M=%.2f | Wedge: θ=%.1f°",
        m_contourType == CFDContourType::MachNumber ? "Local Mach Number M(x,y)" :
        m_contourType == CFDContourType::StaticPressure ? "Static Pressure Ratio p/p_inf" :
        m_contourType == CFDContourType::Density ? "Density Ratio ρ/ρ_inf" : "Numerical Schlieren |∇ρ|",
        m_machInf, m_rampAngleDeg);

    ImVec2 viewSize(940, 440);
    ImGui::Image((ImTextureID)(intptr_t)m_flowTexId, viewSize, ImVec2(0, 0), ImVec2(1, 1));

    ImGui::End();
}

void CFDView::drawControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(1005, 35), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 520), ImGuiCond_FirstUseEver);

    ImGui::Begin("Supersonic CFD Controls", nullptr, ImGuiWindowFlags_None);
    if (!m_solverError.empty()) ImGui::TextWrapped("Solver stopped: %s", m_solverError.c_str());
    if (m_solver) ImGui::Text("Scaled residual: %.3e", m_solver->residualNorm());

    if (ImGui::Button(m_isPaused ? "Resume Solver" : "Pause Solver", ImVec2(145, 32))) {
        m_isPaused = !m_isPaused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Flowfield", ImVec2(145, 32))) {
        resetSimulation();
    }

    ImGui::Separator();
    ImGui::Text("Flow Parameters:");

    float mInf = (float)m_machInf;
    if (ImGui::SliderFloat("Inflow Mach M_inf", &mInf, 1.2f, 5.0f, "M = %.2f")) {
        m_machInf = mInf;
        resetSimulation();
    }

    float rampDeg = (float)m_rampAngleDeg;
    if (ImGui::SliderFloat("Wedge Ramp θ", &rampDeg, 5.0f, 25.0f, "θ = %.1f°")) {
        m_rampAngleDeg = rampDeg;
        resetSimulation();
    }

    ImGui::SliderInt("Substeps/Frame", &m_substepsPerFrame, 1, 30);

    float cflVal = (float)m_cfl;
    if (ImGui::SliderFloat("CFL Number", &cflVal, 0.1f, 0.8f, "CFL = %.2f")) {
        m_cfl = cflVal;
        if (m_solver) m_solver->setCfl(m_cfl);
    }

    ImGui::Separator();
    ImGui::Text("Contour Display:");
    if (ImGui::RadioButton("Local Mach M", m_contourType == CFDContourType::MachNumber)) m_contourType = CFDContourType::MachNumber;
    if (ImGui::RadioButton("Pressure p/p_inf", m_contourType == CFDContourType::StaticPressure)) m_contourType = CFDContourType::StaticPressure;
    if (ImGui::RadioButton("Density ρ/ρ_inf", m_contourType == CFDContourType::Density)) m_contourType = CFDContourType::Density;
    if (ImGui::RadioButton("Numerical Schlieren |∇ρ|", m_contourType == CFDContourType::NumericalSchlieren)) m_contourType = CFDContourType::NumericalSchlieren;

    ImGui::Separator();
    ImGui::Text("Colormap Palette:");
    ImGui::Combo("Colormap", &m_colormapIdx, "Turbo (Google Perceptual)\0Inferno (High Contrast)\0Cool-Warm (Diverging)\0");

    ImGui::End();
}

void CFDView::drawBenchmarkTable() {
    ImGui::SetNextWindowPos(ImVec2(10, 565), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1335, 230), ImGuiCond_FirstUseEver);

    ImGui::Begin("Analytical Rankine-Hugoniot Verification Benchmark", nullptr, ImGuiWindowFlags_None);
    if (!m_hasSample) {
        ImGui::TextUnformatted("Awaiting a numerical sample.");
        ImGui::End();
        return;
    }

    if (m_hasAnalyticalSolution) {
        ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f),
            "● ANALYTICAL SOLUTION EXISTENCE: ATTACHED OBLIQUE SHOCK WAVE (θ < θ_max)");

        if (ImGui::BeginTable("RHBenchmarkTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Aerodynamic Property");
            ImGui::TableSetupColumn("Analytical Theory (θ-β-M)");
            ImGui::TableSetupColumn("Numerical 2D Finite Volume");
            ImGui::TableSetupColumn("Residual Error Norm");
            ImGui::TableSetupColumn("Verification Status");
            ImGui::TableHeadersRow();

            // Row 1: Oblique Shock Wave Angle
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Oblique Shock Angle β");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f°", m_theoryBetaDeg);
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("Not measured");
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted("N/A");
            ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted("UNVERIFIED");

            // Row 2: Post-Shock Mach Number M2
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Post-Shock Mach M_2");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", m_theoryM2);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", m_numM2);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f %%", m_numErrorM2Percent);
            ImGui::TableSetColumnIndex(4);
            if (m_numErrorM2Percent < 1.0) {
                ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "PASSED (< 1.0%%)");
            } else {
                ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "OUTSIDE TOLERANCE");
            }

            // Row 3: Static Pressure Ratio p2/p1
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Static Pressure Ratio p_2 / p_1");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", m_theoryP2P1);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", m_numP2P1);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f %%", m_numErrorPPercent);
            ImGui::TableSetColumnIndex(4);
            if (m_numErrorPPercent < 1.0) {
                ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "PASSED (< 1.0%%)");
            } else {
                ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "OUTSIDE TOLERANCE");
            }

            // Row 4: Temperature Ratio T2/T1
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Temperature Ratio T_2 / T_1");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", m_theoryT2T1);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", m_numT2T1);
            const double temperatureError = std::abs(m_numT2T1 / m_theoryT2T1 - 1.0) * 100.0;
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f %%", temperatureError);
            ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(
                temperatureError < 1.0 ? "PASSED (< 1.0%)" : "OUTSIDE TOLERANCE");

            ImGui::EndTable();
        }
    } else {
        ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f),
            "● DETACHED BOW SHOCK REGIME: Ramp angle θ exceeds maximum deflection angle θ_max for M_inf = %.2f!", m_machInf);
    }

    ImGui::End();
}

void CFDView::cleanup() {
    if (m_flowTexId) {
        glDeleteTextures(1, &m_flowTexId);
        m_flowTexId = 0;
    }
}

} // namespace gui
