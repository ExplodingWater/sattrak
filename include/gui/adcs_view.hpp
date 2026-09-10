#pragma once

#include <vector>
#include <string>
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "common/math_types.hpp"
#include "adcs/quaternion.hpp"
#include "adcs/satellite_dynamics.hpp"
#include "adcs/mekf.hpp"
#include "adcs/controller.hpp"
#include "gui/gl_util.hpp"
#include "gui/satellite_model.hpp"

namespace gui {

struct GroundStation {
    std::string name;
    double latDeg;
    double lonDeg;
    unsigned int texture = 0;
    float imageAspect = 1.0f;
};

struct RollingBuffer {
    int maxSize = 600;
    int offset = 0;
    std::vector<float> timeData;
    std::vector<float> valData;

    void addPoint(float t, float val) {
        if ((int)timeData.size() < maxSize) {
            timeData.push_back(t);
            valData.push_back(val);
        } else {
            timeData[offset] = t;
            valData[offset] = val;
            offset = (offset + 1) % maxSize;
        }
    }

    void clear() {
        timeData.clear();
        valData.clear();
        offset = 0;
    }
};

enum class CameraPreset {
    FreeFlycam,         // WASD + Right-Click Look
    SatelliteOrbit,    // Follow satellite center; left-drag to orbit, wheel to dolly
    FreeOrbitArcball,   // Left-Click Arcball Orbit
    ChaseCam,           // Over-the-shoulder behind satellite
    TelescopeLookdown,  // Optical sensor boresight looking at target
    NadirEarth,         // Underside camera pointing at Earth center
    SolarWing,          // Solar array wingtip view
    GroundStationPOV,   // Standing on Earth looking up at satellite
    OrbitalPlane,       // High orbit normal plane overview
    SunEarthEcliptic    // Sunward vantage point
};

class ADCSView {
public:
    ADCSView();
    ~ADCSView();

    bool init();
    void handleInput(GLFWwindow* window, float dt);
    void update(float dt);
    void suspendAudio() { updateAlarm(false); }
    void render3D(int width, int height);
    void drawUI();
    void drawFlightOverlay();
    void setCameraPreset(CameraPreset preset) { m_camPreset = preset; }
    void cleanup();

    void injectTumble();
    void resetPointing();
    void onScroll(double yoffset);

private:
    friend struct ADCSViewChecks;
    void rotateFlycam(double dx, double dy);
    bool sceneAcceptsInput() const;
    void moveFlycam(double forward, double right, double up, bool boost, double dt);
    void drawSettings();
    void updateReentry();
    void drawReentry(const Mat4& viewProj, const Mat4& satModel, int height);
    void updateAlarm(bool active);
    bool m_entryPreview=false, m_alarmMuted=false, m_alarmPlaying=false;
    float m_descentKmS=0.25f;
    double m_heatFlux=0, m_dynamicPressure=0, m_projectedArea=0, m_airSpeed=0;
    aero::math::Vec3 m_airDirection{1,0,0};
    ShaderProgram m_entryShader;
    unsigned int m_entryVao=0, m_entryVbo=0;
    std::vector<AeroFacet> m_lowPolyFacets;

    void setRealScale(bool enabled);
    double spacecraftScale() const;
    bool satelliteHiddenByEarth() const;
    bool m_realScale = false;
    bool m_openSettings = false;
    float m_sensitivity = 0.0035f;
    float m_scrollSensitivity = 1.0f;
    float m_userSpeedKmS = 7.5f;
    float m_satelliteSpeedFactor = 1.0f;
    float m_savedTimeWarp = 5.0f;
    double m_actualUserSpeedKmS = 0;
    SatelliteModel m_landsatModel;
    int m_selectedModel = 0;
    void selectModel(int model);
    float modelFramingScale() const;
    ShaderProgram m_shader3D;
    ShaderProgram m_lineShader;
    ShaderProgram m_atmosphereShader;
    unsigned int m_starVao = 0, m_starVbo = 0;
    aero::math::Vec3 m_cameraEye{0, 10, 26};
    aero::math::Vec3 m_cameraForward{0, 0, -1};
    aero::math::Vec3 m_cameraUp{0, 1, 0};
    aero::math::Vec3 m_flyForward{0, 0, -1};
    aero::math::Vec3 m_flyUp{0, 1, 0};
    void enterFlycam();
    Mesh m_earthMesh;
    Mesh m_busMesh;
    Mesh m_solarPanelMesh;
    Mesh m_lensMesh;
    Mesh m_dishMesh;
    Mesh m_beaconMesh;
    unsigned int m_earthTexId = 0;

    unsigned int m_lineVao = 0;
    unsigned int m_lineVbo = 0;

    aero::adcs::SatelliteDynamics m_dynamics;
    aero::adcs::SatelliteState m_satState;
    aero::adcs::MEKF m_mekf;
    aero::adcs::AttitudeController m_controller;
    aero::math::Vec3 m_trueGyroBias{0.012, -0.015, 0.010};

    double m_orbitAltitudeKm = 650.0;
    double m_orbitInclinationDeg = 51.6;
    double m_trueAnomalyRad = 0.0;
    double m_earthSpinAngleRad = 0.0;
    bool m_autoEarthSpin = true;
    bool m_isEarthManualRotate = false;
    double m_simTimeSec = 0.0;
    double m_physicsAccumulator = 0.0;
    void advancePhysics(double dt);

    std::vector<GroundStation> m_stations;
    int m_selectedStationIdx = 0;
    double m_targetLatDeg = 28.39;
    double m_targetLonDeg = -80.60;
    std::string m_targetName = "Cape Canaveral";

    aero::math::Vec3 m_satPosECI{0, 0, 0};
    aero::math::Vec3 m_targetPosECI{0, 0, 0};
    aero::math::Vec3 m_muzzlePosECI{0, 0, 0};
    double m_slantRangeKm = 0.0;
    double m_elevationDeg = 0.0;
    bool m_isTargetVisible = true;
    bool m_isSatSunlit = true;
    bool m_isTargetDay = true;
    double m_slewErrorDeg = 0.0;
    double m_bodyRateNormDegS = 0.0;

    struct LatLonPoint { float lat, lon; };
    std::vector<LatLonPoint> m_groundTrack2D;

    // Camera State
    CameraPreset m_camPreset = CameraPreset::ChaseCam;

    // Free Flycam State (WASD + Right-Click)
    aero::math::Vec3 m_flycamPos{0, 10, 26};
    bool m_isRightClickDragging = false;
    double m_lastMouseX = 0.0, m_lastMouseY = 0.0;

    // Arcball State
    float m_satOrbitYaw = 0.8f;
    float m_satOrbitPitch = 0.35f;
    float m_satOrbitDistance = 8.0f; // Model units; meters in 1:1 mode
    aero::math::Vec3 satelliteOrbitOffset() const;
    void rotateSatelliteOrbit(float dx, float dy);
    float m_arcballYaw = 0.8f;
    float m_arcballPitch = 0.35f;
    float m_arcballDist = 42.0f;
    aero::math::Vec3 m_arcballCenter{0, 0, 0};
    bool m_isLeftClickDragging = false;
    double m_lastLeftMouseX = 0.0, m_lastLeftMouseY = 0.0;

    bool m_isPaused = false;
    float m_timeWarp = 5.0f;
    float m_earthSpinWarp = 1.0f;

    RollingBuffer m_bufOmegaNorm;
    RollingBuffer m_bufOmegaX, m_bufOmegaY, m_bufOmegaZ;
    RollingBuffer m_bufBiasEstX, m_bufBiasEstY, m_bufBiasEstZ;
    RollingBuffer m_bufBiasTrueX;
    RollingBuffer m_bufSlewError;

    void drawTelemetryHUD();
    void drawCharts();
    void drawControlPanel();
    void draw2DWorldMap();
    void drawTelescopeReticle(int width, int height);
    aero::math::Vec3 latLonToECEF(double latDeg, double lonDeg, double radius);
    void computeLineOfSight();
};

} // namespace gui
