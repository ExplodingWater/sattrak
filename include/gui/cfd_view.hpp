#pragma once

#include <vector>
#include <string>
#include <memory>
#include "cfd/euler_state.hpp"
#include "cfd/grid.hpp"
#include "cfd/cfd_solver.hpp"
#include "gui/gl_util.hpp"

namespace gui {

enum class CFDContourType {
    MachNumber,
    StaticPressure,
    Density,
    NumericalSchlieren
};

class CFDView {
public:
    CFDView();
    ~CFDView();

    bool init();
    void update(float dt);
    void drawUI();
    void cleanup();

    void resetSimulation();

private:
    int m_nx = 80;
    int m_ny = 40;
    double m_machInf = 2.5;
    double m_rampAngleDeg = 15.0;
    double m_cfl = 0.5;
    int m_substepsPerFrame = 5;
    bool m_isPaused = false;

    CFDContourType m_contourType = CFDContourType::MachNumber;
    int m_colormapIdx = 0;

    std::unique_ptr<aero::cfd::StructuredGrid> m_grid;
    std::unique_ptr<aero::cfd::CFDSolver> m_solver;

    unsigned int m_flowTexId = 0;
    std::vector<unsigned char> m_pixelBuffer;

    double m_theoryBetaDeg = 0.0;
    double m_theoryM2 = 0.0;
    double m_theoryP2P1 = 0.0;
    double m_theoryT2T1 = 0.0;
    bool m_hasAnalyticalSolution = true;

    double m_numM2 = 0.0;
    double m_numP2P1 = 0.0;
    double m_numErrorM2Percent = 0.0;
    double m_numErrorPPercent = 0.0;
    double m_numT2T1 = 0.0;
    bool m_hasSample = false;
    std::string m_solverError;

    void updateAnalyticalTheory();
    void updateFlowTexture();
    void drawFlowfieldViewport();
    void drawControlPanel();
    void drawBenchmarkTable();
};

} // namespace gui
