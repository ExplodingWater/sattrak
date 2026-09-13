#include "gui/adcs_view.hpp"
#include "gui/slew_profile.hpp"
#include "gui/numeric_widgets.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h"
#include <array>
#include <fstream>
#include <random>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#endif
#include <cstdint>
#include <cstring>

namespace gui {

const float EARTH_RADIUS_3D = 12.0f; // Scaled 3D Earth radius
const double KM_TO_3D = EARTH_RADIUS_3D / 6378.137;
const double EARTH_HIGH_LOD_DISTANCE = 2.0 * EARTH_RADIUS_3D;
const double EARTH_LOD_HYSTERESIS = 0.05;


// Each panel follows its edge until its title bar is dragged. ImGui retains
// detached positions; the small companion file retains the attachment intent.
static std::array<bool, 4>& detachedPanels() {
    static std::array<bool, 4> state = [] {
        std::array<bool, 4> result{};
        std::ifstream file("panel_layout.txt");
        for (auto& value : result) { int v = 0; if (file >> v) value = v != 0; }
        return result;
    }();
    return state;
}
static void savePanelLayout() {
    std::ofstream file("panel_layout.txt");
    for (bool value : detachedPanels()) file << value << ' ';
}
static void beginEdgePanel(const char* name, int index) {
    auto& detached = detachedPanels()[index];
    ImGuiWindow* existing = ImGui::FindWindowByName(name);
    if (!detached && existing && GImGui->MovingWindow == existing &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        detached = true;
        savePanelLayout();
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = std::max(120.0f, std::min(360.0f, (display.x - 30.0f) * 0.45f));
    const float available = std::max(100.0f, display.y - 55.0f);
    const bool right = index >= 2;
    const bool lower = index == 1 || index == 3;
    const float upper = available * (right ? 0.68f : 0.44f);
    if (!detached) {
        ImGui::SetNextWindowPos(ImVec2(right ? display.x - width - 10 : 10,
                                      lower ? 45 + upper + 10 : 45), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, lower ? available - upper - 10 : upper), ImGuiCond_Always);
    }
    ImGui::Begin(name, nullptr, detached ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoResize);
}
static void explain(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
static aero::math::Vec3 rotateVector(const aero::math::Vec3& v,
                                    const aero::math::Vec3& axis, double angle) {
    return v * std::cos(angle) + axis.cross(v) * std::sin(angle)
           + axis * (axis.dot(v) * (1.0 - std::cos(angle)));
}
void ADCSView::rotateFlycam(double dx, double dy) {
            const double sensitivity = m_sensitivity;
            m_flyForward = rotateVector(m_flyForward, m_flyUp, -dx*sensitivity).normalized();
            auto right = m_flyForward.cross(m_flyUp).normalized();
            m_flyForward = rotateVector(m_flyForward, right, -dy*sensitivity).normalized();
            m_flyUp = right.cross(m_flyForward).normalized();
}
void ADCSView::enterFlycam() {
    if (m_camPreset == CameraPreset::FreeFlycam) return;
    m_flycamPos = m_cameraEye;
    m_flyForward = m_cameraForward;
    m_flyUp = m_cameraUp;
    m_camPreset = CameraPreset::FreeFlycam;
}

// GLSL Shaders
static const char* vertexShader3D = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoords;

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vTexCoords = aTexCoords;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* fragmentShader3D = R"(
#version 330 core
out vec4 FragColor;

in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoords;

uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform vec4 uColor;
uniform vec3 uSunDir;
uniform vec3 uViewPos;
uniform float uRoughness;

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uSunDir);

    float ambientStrength = 0.22;
    vec3 ambient = ambientStrength * vec3(0.12, 0.18, 0.28);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 0.98, 0.92);

    vec3 viewDir = normalize(uViewPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), mix(96.0, 8.0, uRoughness));
    vec3 specular = 0.35 * spec * vec3(1.0, 1.0, 1.0);

    vec4 baseColor = uUseTexture ? texture(uTexture, vTexCoords) : uColor;
    if(baseColor.a<0.1)discard;
    float ocean = uUseTexture ? smoothstep(0.0, 0.12, baseColor.b - max(baseColor.r, baseColor.g)) : 1.0;
    vec3 result = (ambient + diffuse) * baseColor.rgb + specular * ocean * max(dot(norm, lightDir), 0.0);
    FragColor = vec4(result, baseColor.a);
}
)";

static const char* vertexShaderLine = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* fragmentShaderLine = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;

void main() {
    FragColor = uColor;
}
)";


static const char* atmosphereFragment = R"(
#version 330 core
in vec3 vFragPos;
out vec4 FragColor;
uniform vec3 uViewPos;
uniform vec3 uSunDir;
vec2 intersectSphere(vec3 origin, vec3 ray, float radius) {
    float b = dot(origin, ray);
    float d = b*b - dot(origin, origin) + radius*radius;
    if (d < 0.0) return vec2(1e5, -1e5);
    return vec2(-b - sqrt(d), -b + sqrt(d));
}
void main() {
    vec3 ray = normalize(vFragPos - uViewPos);
    vec3 sun = normalize(uSunDir);
    vec2 outer = intersectSphere(uViewPos, ray, 12.24);
    float start = max(outer.x, 0.0), end = outer.y;
    vec2 ground = intersectSphere(uViewPos, ray, 12.0);
    if (ground.x > 0.0) end = min(end, ground.x);
    if (end <= start) discard;
    float stepSize = (end-start)/16.0;
    vec3 beta = vec3(0.45, 1.05, 2.5);
    float depth = 0.0;
    vec3 scattering = vec3(0.0);
    for (int i=0; i<16; ++i) {
        vec3 p = uViewPos + ray*(start + (float(i)+0.5)*stepSize);
        float density = exp(-max(length(p)-12.0, 0.0)/0.035);
        depth += density * stepSize;
        vec2 shadow = intersectSphere(p, sun, 12.0);
        if (shadow.x > 0.0 && shadow.y > 0.0) continue;
        float sunLength = max(intersectSphere(p, sun, 12.24).y, 0.0);
        float sunDepth = 0.0;
        for (int j=0; j<8; ++j) {
            vec3 q = p + sun * ((float(j)+0.5)*sunLength/8.0);
            sunDepth += exp(-max(length(q)-12.0, 0.0)/0.035)*sunLength/8.0;
        }
        scattering += density * stepSize * exp(-beta*(depth+sunDepth));
    }
    float mu = dot(ray, sun);
    vec3 color = scattering * beta * (0.75*(1.0+mu*mu)) * 0.8;
    FragColor = vec4(color, 1.0);
}
)";

ADCSView::ADCSView()
    : m_dynamics(aero::adcs::SpacecraftParameters{}),
      m_mekf(aero::adcs::MEKFConfig{}),
      m_controller(aero::adcs::ControllerConfig{0.15, 0.25, 0.008}),
      m_trueGyroBias(0.012, -0.015, 0.010)
{
    m_satState.q = aero::adcs::Quaternion::identity();
    m_satState.omega = aero::math::Vec3(0.002, 0.001, -0.001);
    m_satState.hWheel = aero::math::Vec3::zero();

    m_stations = {
        {"Cape Canaveral (USSF)", 28.39, -80.60},
        {"Houston Mission Control", 29.56, -95.09},
        {"London Ground Station", 51.50, -0.12},
        {"Tokyo Tracking Station", 35.68, 139.76},
        {"Sydney Ground Antenna", -33.86, 151.20},
        {"Svalbard Deep Space", 78.22, 15.65},
        {"Kourou (ESA Spaceport)", 5.16, -52.65}
    };
}

ADCSView::~ADCSView() {
    cleanup();
}

bool ADCSView::init() {
    if (!m_shader3D.loadFromSource(vertexShader3D, fragmentShader3D)) return false;
    if (!m_lineShader.loadFromSource(vertexShaderLine, fragmentShaderLine)) return false;
    if (!m_atmosphereShader.loadFromSource(vertexShader3D, atmosphereFragment)) return false;
    const char* photos[] = {"cape", "houston", "london", "tokyo", "sydney", "svalbard", "kourou"};
    for (size_t i = 0; i < m_stations.size(); ++i) {
        m_stations[i].texture = loadTextureFromFile(std::string("assets/stations/") + photos[i] + ".jpg");
        if (m_stations[i].texture) {
            glBindTexture(GL_TEXTURE_2D, m_stations[i].texture);
            GLint w = 1, h = 1;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
            m_stations[i].imageAspect = float(w) / h;
        }
    }
    std::mt19937 random(7429);
    std::uniform_real_distribution<float> unit(0, 1);
    std::vector<float> stars;
    for (int i = 0; i < 3000; ++i) {
        float y = 2*unit(random)-1, angle = 6.2831853f*unit(random);
        float r = std::sqrt(1-y*y);
        stars.insert(stars.end(), {450*r*std::cos(angle), 450*y, 450*r*std::sin(angle)});
    }
    glGenVertexArrays(1, &m_starVao);
    glGenBuffers(1, &m_starVbo);
    glBindVertexArray(m_starVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_starVbo);
    glBufferData(GL_ARRAY_BUFFER, stars.size()*sizeof(float), stars.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    m_earthMeshes[0] = createSphereMesh(EARTH_RADIUS_3D, 128, 192);
    m_earthMeshes[1] = createSphereMesh(EARTH_RADIUS_3D, 256, 384);
    m_busMesh = createBoxMesh(0.7f, 0.7f, 1.2f);
    m_solarPanelMesh = createBoxMesh(1.4f, 0.04f, 0.8f);
    m_lensMesh = createCylinderMesh(0.24f, 0.28f, 0.55f, 32);
    m_dishMesh = createCylinderMesh(0.42f, 0.05f, 0.25f, 32);
    m_beaconMesh = createCylinderMesh(0.08f, 0.08f, 0.8f, 16);

    m_earthTexId = loadTextureFromFile("earth_texture.jpg");
    if (!m_landsatModel.load("assets/models/landsat8/landsat8.mesh"))
        std::cerr << "Landsat 8 asset unavailable; low poly remains available.\n";
    const char* modelFiles[]={"hubble","terra","goes","iss"};
    for(int i=0;i<4;++i) {
        const std::string folder=modelFiles[i];
        if(!m_additionalModels[i].load("assets/models/"+folder+"/"+folder+".mesh"))
            std::cerr<<folder<<" model unavailable\n";
    }
    std::ifstream preference("satellite_model.txt");
    int savedModel = 0;
    if (preference >> savedModel && savedModel >= 1 && savedModel <= 5) {
        m_selectedModel = savedModel;
        if(selectedDetailedModel()->parts.empty())m_selectedModel=0;
        m_satOrbitDistance = 8.0f * modelFramingScale();
    }

    m_researchCatalog.load("assets/catalog");
    std::ifstream discovered("research_mode_unlocked.txt");
    int unlocked=0; if(discovered>>unlocked) m_researchUnlocked=unlocked==1;
    m_satPosECI={EARTH_RADIUS_3D+m_orbitAltitudeKm*KM_TO_3D,0,0};
    m_previousTrailPosition=m_satPosECI;
    m_trail={{m_satPosECI,true}};
    glGenVertexArrays(1, &m_lineVao);
    glGenBuffers(1, &m_lineVbo);
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, 1024 * sizeof(float) * 3, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    return true;
}

void ADCSView::handleInput(GLFWwindow* window, float dt) {
    ImGuiIO& io = ImGui::GetIO();
    m_actualUserSpeedKmS = 0;
    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED) || glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_isRightClickDragging = false;
        return;
    }
    m_actualUserSpeedKmS = 0;
    const bool sceneInput = sceneAcceptsInput();
    if (sceneInput && io.MouseWheel != 0) onScroll(io.MouseWheel);

    // 1. Right-Click: Look Around (Yaw & Pitch)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && (sceneInput || m_isRightClickDragging)) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (!m_isRightClickDragging) {
            m_isRightClickDragging = true;
            enterFlycam();
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            glfwGetCursorPos(window, &mouseX, &mouseY);
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
        } else {
            float dx = (float)(mouseX - m_lastMouseX);
            float dy = (float)(mouseY - m_lastMouseY);
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;

            rotateFlycam(dx, dy);
        }
    } else {
        if (m_isRightClickDragging) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_isRightClickDragging = false;
    }

    // UI keyboard navigation retains focus after a click. Only an active editor,
    // widget, or popup owns movement keys; merely focusing a panel does not.
    if ((sceneInput || m_isRightClickDragging) && !io.WantTextInput && !ImGui::IsAnyItemActive()
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        auto key = [&](int k) { return glfwGetKey(window, k) == GLFW_PRESS ? 1.0 : 0.0; };
        moveFlycam(key(GLFW_KEY_W)-key(GLFW_KEY_S), key(GLFW_KEY_D)-key(GLFW_KEY_A),
            std::max(key(GLFW_KEY_E), key(GLFW_KEY_SPACE))-std::max(key(GLFW_KEY_Q), key(GLFW_KEY_C)),
            key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_RIGHT_SHIFT), dt);
    }

    // 3. Left-Click Drag: Rotate Earth OR Arcball Orbit
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && sceneInput) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        if (!m_isLeftClickDragging) {
            m_isLeftClickDragging = true;
            m_lastLeftMouseX = mouseX;
            m_lastLeftMouseY = mouseY;
        } else {
            float dx = (float)(mouseX - m_lastLeftMouseX);
            float dy = (float)(mouseY - m_lastLeftMouseY);
            m_lastLeftMouseX = mouseX;
            m_lastLeftMouseY = mouseY;

            if (!m_replayActive && ((m_isEarthManualRotate && m_camPreset != CameraPreset::SatelliteOrbit) || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)) {
                // Interactive Earth rotation around polar axis
                m_earthSpinAngleRad += dx * 0.008;
            } else if (m_camPreset == CameraPreset::SatelliteOrbit) {
                rotateSatelliteOrbit(dx, dy);
            } else if (m_camPreset == CameraPreset::FreeOrbitArcball) {
                m_arcballYaw += dx * m_sensitivity * (0.005f / 0.0035f);
                m_arcballPitch += dy * m_sensitivity * (0.005f / 0.0035f);
                m_arcballPitch = std::clamp(m_arcballPitch, -1.5f, 1.5f);
            }
        }
    } else {
        m_isLeftClickDragging = false;
    }
}

bool ADCSView::sceneAcceptsInput() const {
    const auto& io = ImGui::GetIO();
    return ImGui::IsMousePosValid() && io.MousePos.x >= 0 && io.MousePos.y >= 36
        && io.MousePos.x < io.DisplaySize.x && io.MousePos.y < io.DisplaySize.y
        && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
        && !ImGui::IsAnyItemActive()
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}
void ADCSView::moveFlycam(double forward, double right, double up, bool boost, double dt) {
    if (forward == 0 && right == 0 && up == 0) return;
    enterFlycam();
    const double speed = (m_realScale ? m_userSpeedKmS * KM_TO_3D : 14.0) * (boost ? 3.0 : 1.0);
    const auto direction = (m_flyForward*forward + m_flyForward.cross(m_flyUp)*right + m_flyUp*up).normalized();
    m_flycamPos += direction * (speed * dt);
    m_actualUserSpeedKmS = speed / KM_TO_3D;
}
void ADCSView::onScroll(double yoffset) {
    if (!sceneAcceptsInput()) return;
    yoffset *= m_scrollSensitivity;
    if (m_camPreset == CameraPreset::FreeFlycam) {
        const auto forward = m_flyForward;
        m_flycamPos += forward * yoffset * (m_realScale ? m_userSpeedKmS * KM_TO_3D * 0.1 : 2.5);
    } else if (m_camPreset == CameraPreset::SatelliteOrbit) {
        m_satOrbitDistance = std::clamp(m_satOrbitDistance * float(std::exp(-yoffset * 0.12)), 0.25f, 200000.0f);
    } else if (m_camPreset == CameraPreset::FreeOrbitArcball) {
        m_arcballDist -= (float)yoffset * 2.0f;
        m_arcballDist = std::clamp(m_arcballDist, 13.5f, 150.0f);
    }
}

aero::math::Vec3 ADCSView::latLonToECEF(double latDeg, double lonDeg, double radius) {
    const double PI = 3.14159265358979323846;
    double phi = (90.0 - latDeg) * (PI / 180.0);
    double theta = (lonDeg + 180.0) * (PI / 180.0);

    return aero::math::Vec3(
        -radius * std::sin(phi) * std::cos(theta),
        radius * std::cos(phi),
        radius * std::sin(phi) * std::sin(theta)
    );
}

void ADCSView::computeLineOfSight() {
    aero::math::Vec3 tgtECEF = latLonToECEF(m_targetLatDeg, m_targetLonDeg, EARTH_RADIUS_3D);

    float cE = std::cos((float)m_earthSpinAngleRad);
    float sE = std::sin((float)m_earthSpinAngleRad);
    m_targetPosECI = aero::math::Vec3(
        tgtECEF[0] * cE + tgtECEF[2] * sE,
        tgtECEF[1],
        -tgtECEF[0] * sE + tgtECEF[2] * cE
    );

    // Muzzle position in ECI (+Z = 1.15 in body frame)
    aero::math::Vec3 muzzleBody(0.0, 0.0, 1.15 * spacecraftScale());
    m_muzzlePosECI = m_satPosECI + m_satState.q.rotateInverse(muzzleBody);

    aero::math::Vec3 rho = m_satPosECI - m_targetPosECI;
    m_slantRangeKm = rho.norm() / KM_TO_3D;

    aero::math::Vec3 upZenith = m_targetPosECI.normalized();
    double sinElev = rho.normalized().dot(upZenith);
    m_elevationDeg = std::asin(std::max(-1.0, std::min(1.0, sinElev))) * (180.0 / 3.14159265358979323846);
    m_isTargetVisible = (m_elevationDeg >= m_elevationMask);

    aero::math::Vec3 sunDir = aero::math::Vec3(150, 40, 80).normalized();
    double satProjSun = m_satPosECI.dot(sunDir);
    m_isSatSunlit = true;
    if (satProjSun < 0.0) {
        aero::math::Vec3 perp = m_satPosECI - sunDir * satProjSun;
        if (perp.norm() < EARTH_RADIUS_3D) {
            m_isSatSunlit = false;
        }
    }

    m_isTargetDay = (m_targetPosECI.dot(sunDir) > 0.0);
}

void ADCSView::update(float dt) {
    updateReentry();
    updateAlarm(m_heatFlux > 100000 && !m_alarmMuted);
    if (m_isPaused) return;
    if(m_replayActive)advanceReplay(dt);
    m_physicsAccumulator += static_cast<double>(dt) * (m_replayActive || m_realScale ? 1.0 : m_timeWarp);
    const double physicsDt = m_physicsStep;
    while (m_physicsAccumulator >= physicsDt) {
        advancePhysics(physicsDt);
        m_physicsAccumulator -= physicsDt;
        if(m_isPaused){m_physicsAccumulator=0;break;}
    }
}

void ADCSView::advancePhysics(double dt) {
    m_simTimeSec += dt;
    if (m_entryPreview && !m_replayActive && !m_numericalOrbit) m_orbitAltitudeKm = std::max(20.0, m_orbitAltitudeKm - m_descentKmS * dt);

    if(!m_replayActive) {
    if(m_numericalOrbit) {
        if(!m_numericalReady)initializeNumericalOrbit();
        auto next=integrateOrbit(m_numericalState,dt,m_orbitForces);
        if(!std::isfinite(next.position.norm())||!std::isfinite(next.velocity.norm())||next.position.norm()<6458.137){m_isPaused=true;m_expertMessage="Orbit reached 80 km altitude or an unstable state; paused. Reduce the step or reset the orbit.";return;}
        m_numericalState=next;m_currentOrbit=next;
        m_satPosECI=orbitToScene(next.position);m_orbitAltitudeKm=next.position.norm()-6378.137;
    } else {
    // 1. Orbital Kinematics (Keplerian circular orbit in ECI)
    double rKm = 6378.137 + m_orbitAltitudeKm;
    double r3D = EARTH_RADIUS_3D + m_orbitAltitudeKm * KM_TO_3D;
    double orbSpeed = std::sqrt(398600.44 / (rKm * rKm * rKm));
    m_trueAnomalyRad += orbSpeed * dt * (m_realScale ? m_satelliteSpeedFactor : 1.0);

    double incRad = m_orbitInclinationDeg * (3.14159265358979323846 / 180.0);
    double satX = r3D * std::cos(m_trueAnomalyRad);
    double satY = r3D * std::sin(m_trueAnomalyRad) * std::sin(incRad);
    double satZ = r3D * std::sin(m_trueAnomalyRad) * std::cos(incRad);
    m_satPosECI = aero::math::Vec3(satX, satY, satZ);
    }
    recordTrail();

    // 2. Earth Diurnal Spin (if auto spin enabled)
    if (m_autoEarthSpin) {
        double baseSpin = aero::constants::EARTH_OMEGA;
        m_earthSpinAngleRad += baseSpin * dt * m_earthSpinWarp;
        if (m_earthSpinAngleRad > 2.0 * 3.14159265358979323846) {
            m_earthSpinAngleRad -= 2.0 * 3.14159265358979323846;
        }
    }
    }

    computeLineOfSight();

    // 3. Closed-loop Pointing to Ground Target
    aero::math::Vec3 losVec = (m_targetPosECI - m_satPosECI).normalized();
    aero::math::Vec3 zAxis(0, 0, 1);
    aero::math::Vec3 rotAxis = zAxis.cross(losVec);
    double cosTheta = zAxis.dot(losVec);
    double angle = std::acos(std::clamp(cosTheta, -1.0, 1.0));

    aero::adcs::Quaternion qTarget = aero::adcs::Quaternion::identity();
    if (rotAxis.norm() > 1e-6) {
        qTarget = aero::adcs::Quaternion::fromRotationVector(rotAxis.normalized() * angle);
    } else if(cosTheta<0)qTarget=aero::adcs::Quaternion::fromRotationVector({3.141592653589793,0,0});

    // Reaction Wheel Controller Torque
    aero::math::Vec3 tauCmd = m_controller.computeControlTorque(m_satState.q, m_satState.omega, qTarget);
    const double slewRateDeg=m_customSlew?m_maxSlew:(m_selectedModel==2?0.1:0.25);
    const double slewAccelerationDeg=m_customSlew?m_slewAcceleration:0.02;
    constexpr double rad=3.141592653589793/180;
    if(m_realScale && !m_quickReposition) {
        auto acceleration=slewAcceleration(m_satState.q,m_satState.omega,qTarget,slewRateDeg*rad,slewAccelerationDeg*rad);
        const auto& inertia=m_dynamics.parameters().inertia;
        tauCmd=inertia*acceleration+m_satState.omega.cross(inertia*m_satState.omega+m_satState.hWheel);
    }

    // Step Spacecraft Dynamics
    auto disturbance=m_gravityGradient?m_dynamics.computeGravityGradient(m_satPosECI.norm()/KM_TO_3D*1000,m_satState.q.rotate(m_satPosECI.normalized())):aero::math::Vec3::zero();
    auto nextState=m_dynamics.step(m_satState, disturbance, tauCmd, dt);
    if(!std::isfinite(nextState.omega.norm())||nextState.omega.norm()>1000){m_isPaused=true;m_expertMessage="Attitude integration became unstable; reduce torque or timestep.";return;}
    m_satState=nextState;

    // MEKF Estimation
    aero::math::Vec3 gyroNoise,vectorNoise;
    if(m_sensorNoise){std::normal_distribution<double> normal;
        for(int i=0;i<3;++i){gyroNoise[i]=normal(m_sensorRandom)*m_gyroNoise/std::sqrt(dt);m_biasWalk[i]+=normal(m_sensorRandom)*m_biasDrift*std::sqrt(dt);vectorNoise[i]=normal(m_sensorRandom)*m_vectorNoise;}}
    m_mekf.predict(m_satState.omega + m_trueGyroBias + (m_sensorNoise?m_biasWalk:aero::math::Vec3{})+gyroNoise, dt);
    m_mekf.updateVector((m_satState.q.rotate(aero::math::Vec3(0, 0, 1))+vectorNoise).normalized(), aero::math::Vec3(0, 0, 1), m_vectorNoise);

    // Attitude Error
    aero::adcs::Quaternion qErr = qTarget.inverse() * m_satState.q;
    if (qErr.w < 0.0) {
        qErr = aero::adcs::Quaternion(-qErr.w, -qErr.x, -qErr.y, -qErr.z);
    }
    m_slewErrorDeg = 2.0 * std::acos(std::clamp(std::abs(qErr.w), -1.0, 1.0)) * (180.0 / 3.14159265);
    m_bodyRateNormDegS = m_satState.omega.norm() * (180.0 / 3.14159265);
    m_pointingSettledSec=m_slewErrorDeg<0.25 && m_bodyRateNormDegS<0.5?m_pointingSettledSec+dt:0;
    const double etaRate=m_realScale&&!m_quickReposition?slewRateDeg:5.0;
    m_repositionEtaSec=m_pointingSettledSec>=3?0:m_slewErrorDeg/etaRate+
        (m_realScale&&!m_quickReposition?(etaRate+std::max(0.0,m_bodyRateNormDegS-etaRate))/slewAccelerationDeg:1)+std::max(0.0,3-m_pointingSettledSec);

    // Record Telemetry History
    float tSec = (float)m_simTimeSec;
    m_bufOmegaNorm.addPoint(tSec, (float)m_bodyRateNormDegS);
    m_bufOmegaX.addPoint(tSec, (float)(m_satState.omega.x * 180.0 / 3.14159));
    m_bufOmegaY.addPoint(tSec, (float)(m_satState.omega.y * 180.0 / 3.14159));
    m_bufOmegaZ.addPoint(tSec, (float)(m_satState.omega.z * 180.0 / 3.14159));

    aero::math::Vec3 estBias = m_mekf.gyroBias();
    m_bufBiasEstX.addPoint(tSec, (float)estBias.x);
    m_bufBiasEstY.addPoint(tSec, (float)estBias.y);
    m_bufBiasEstZ.addPoint(tSec, (float)estBias.z);
    m_bufBiasTrueX.addPoint(tSec, (float)(m_trueGyroBias.x+(m_sensorNoise?m_biasWalk.x:0)));

    m_bufSlewError.addPoint(tSec, (float)m_slewErrorDeg);

    if(m_replayActive)return; // Replay history is rebuilt from timestamped samples on seek.
    // 2D ECEF Ground Track
    float cInv = std::cos(-(float)m_earthSpinAngleRad);
    float sInv = std::sin(-(float)m_earthSpinAngleRad);
    aero::math::Vec3 satECEF(
        m_satPosECI[0] * cInv + m_satPosECI[2] * sInv,
        m_satPosECI[1],
        -m_satPosECI[0] * sInv + m_satPosECI[2] * cInv
    );
    satECEF = satECEF.normalized();
    float satLat = 90.0f - float(std::acos(std::clamp(satECEF[1], -1.0, 1.0))) * (180.0f / 3.14159f);
    float satLon = float(std::atan2(satECEF[2], -satECEF[0])) * (180.0f / 3.14159f) - 180.0f;
    if (satLon < -180.0f) satLon += 360.0f;
    if (satLon > 180.0f) satLon -= 360.0f;

    if (m_groundTrack2D.empty() ||
        std::hypot(satLon - m_groundTrack2D.back().lon, satLat - m_groundTrack2D.back().lat) > 1.2f) {
        m_groundTrack2D.push_back({satLat, satLon});
        if (m_groundTrack2D.size() > 250) m_groundTrack2D.erase(m_groundTrack2D.begin());
    }
}

aero::math::Vec3 ADCSView::satelliteOrbitOffset() const {
    return aero::math::Vec3(std::cos(m_satOrbitPitch)*std::sin(m_satOrbitYaw),
        std::sin(m_satOrbitPitch), std::cos(m_satOrbitPitch)*std::cos(m_satOrbitYaw))
        * (m_satOrbitDistance * spacecraftScale());
}
void ADCSView::rotateSatelliteOrbit(float dx, float dy) {
    m_satOrbitYaw = std::remainder(m_satOrbitYaw + dx*m_sensitivity, 6.283185307f);
    m_satOrbitPitch = std::remainder(m_satOrbitPitch + dy*m_sensitivity, 6.283185307f);
}
void ADCSView::updateEarthLod(double centerDistance) {
    // High: within 2 Earth radii. Low: beyond 2 Earth radii. Five percent
    // overlap keeps the selected mesh stable near the boundary.
    if (m_earthLod == 1) {
        if (centerDistance > EARTH_HIGH_LOD_DISTANCE * (1.0 + EARTH_LOD_HYSTERESIS)) m_earthLod = 0;
    } else if (centerDistance < EARTH_HIGH_LOD_DISTANCE * (1.0 - EARTH_LOD_HYSTERESIS)) {
        m_earthLod = 1;
    }
}
void ADCSView::render3D(int width, int height) {
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    float aspect = (float)width / (float)(height > 0 ? height : 1);
    Mat4 proj = Mat4::perspective(45.0f * (3.14159265f / 180.0f), aspect, 0.1f, 1000.0f);

    aero::math::Vec3 camEye;
    aero::math::Vec3 camTarget;
    aero::math::Vec3 camUp(0, 1, 0);

    // Compute Camera Vectors for all 9 perspectives
    if (m_camPreset == CameraPreset::FreeFlycam) {
        camEye = m_flycamPos;
        camTarget = m_flycamPos + m_flyForward;
        camUp = m_flyUp;
    } else if (m_camPreset == CameraPreset::SatelliteOrbit) {
        camEye = m_satPosECI + satelliteOrbitOffset();
        camTarget = m_satPosECI;
        // Analytic up remains orthogonal through the poles; no pitch clamp.
        camUp = aero::math::Vec3(-std::sin(m_satOrbitPitch)*std::sin(m_satOrbitYaw),
            std::cos(m_satOrbitPitch), -std::sin(m_satOrbitPitch)*std::cos(m_satOrbitYaw));
    } else if (m_camPreset == CameraPreset::FreeOrbitArcball) {
        float cx = m_arcballDist * std::cos(m_arcballPitch) * std::sin(m_arcballYaw);
        float cy = m_arcballDist * std::sin(m_arcballPitch);
        float cz = m_arcballDist * std::cos(m_arcballPitch) * std::cos(m_arcballYaw);
        camEye = m_arcballCenter + aero::math::Vec3(cx, cy, cz);
        camTarget = m_arcballCenter;
        camUp = aero::math::Vec3(0, 1, 0);
    } else if (m_camPreset == CameraPreset::ChaseCam) {
        aero::math::Vec3 offset = aero::math::Vec3(0, 2.2, -5.5) * (spacecraftScale() * modelFramingScale());
        camEye = m_satPosECI + m_satState.q.rotateInverse(offset);
        camTarget = m_satPosECI + m_satState.q.rotateInverse(aero::math::Vec3(0, 0, 15.0) * (spacecraftScale() * modelFramingScale()));
        camUp = m_satState.q.rotateInverse(aero::math::Vec3(0, 1, 0));
    } else if (m_camPreset == CameraPreset::TelescopeLookdown) {
        camEye = m_muzzlePosECI;
        camTarget = m_targetPosECI;
        aero::math::Vec3 los = (m_targetPosECI - m_satPosECI).normalized();
        aero::math::Vec3 right = los.cross(aero::math::Vec3(0, 1, 0)).normalized();
        camUp = right.cross(los).normalized();
    } else if (m_camPreset == CameraPreset::NadirEarth) {
        camEye = m_satPosECI + m_satState.q.rotateInverse(aero::math::Vec3(0, 1.2, 0) * (spacecraftScale() * modelFramingScale()));
        camTarget = aero::math::Vec3(0, 0, 0);
        camUp = m_satPosECI.normalized();
    } else if (m_camPreset == CameraPreset::SolarWing) {
        camEye = m_satPosECI + m_satState.q.rotateInverse(aero::math::Vec3(2.4, 0.4, -0.6) * (spacecraftScale() * modelFramingScale()));
        camTarget = m_satPosECI;
        camUp = m_satState.q.rotateInverse(aero::math::Vec3(0, 1, 0));
    } else if (m_camPreset == CameraPreset::GroundStationPOV) {
        aero::math::Vec3 zenith = m_targetPosECI.normalized();
        camEye = m_targetPosECI + zenith * 0.15; // Placed directly on ground in target city
        camTarget = m_satPosECI; // Looking straight up into sky at satellite
        camUp = zenith;
    } else if (m_camPreset == CameraPreset::OrbitalPlane) {
        double inc = m_orbitInclinationDeg * (3.14159265 / 180.0);
        camEye = aero::math::Vec3(0, std::cos(inc) * 50.0, -std::sin(inc) * 50.0);
        camTarget = aero::math::Vec3(0, 0, 0);
        camUp = aero::math::Vec3(0, std::sin(inc), std::cos(inc));
    } else { // SunEarthEcliptic
        camEye = aero::math::Vec3(150, 40, 80).normalized() * 58.0;
        camTarget = aero::math::Vec3(0, 0, 0);
        camUp = aero::math::Vec3(0, 1, 0);
    }

    m_cameraEye = camEye;
    updateEarthLod(camEye.norm());
    m_cameraForward = (camTarget - camEye).normalized();
    if (std::abs(m_cameraForward.dot(camUp.normalized())) > 0.999)
        camUp = std::abs(m_cameraForward[1]) < 0.9 ? aero::math::Vec3(0,1,0) : aero::math::Vec3(1,0,0);
    m_cameraUp = m_cameraForward.cross(camUp).normalized().cross(m_cameraForward).normalized();
    // World units are ~531 km: a fixed 0.1 near plane cut away nearby Earth.
    const float nearEarth = float(std::clamp((camEye.norm()-EARTH_RADIUS_3D)*0.05, 1e-6, 0.1));
    proj = Mat4::perspective(45.0f * 3.14159265f / 180.0f, aspect, nearEarth,
        float(std::max(1000.0, camEye.norm()+2*EARTH_RADIUS_3D)));
    Mat4 view = Mat4::lookAt(camEye, camTarget, camUp);
    Mat4 viewProj = Mat4::multiply(proj, view);

    // An eye-centered star sphere has no translation parallax and never clips as we fly.
    glDepthMask(GL_FALSE);
    m_lineShader.use();
    m_lineShader.setMat4("uMVP", Mat4::multiply(proj, Mat4::lookAt(aero::math::Vec3::zero(), m_cameraForward, m_cameraUp)));
    glBindVertexArray(m_starVao);
    m_lineShader.setVec4("uColor", 0.38f, 0.43f, 0.55f, 1);
    glPointSize(1.0f);
    glDrawArrays(GL_POINTS, 0, 2600);
    m_lineShader.setVec4("uColor", 0.8f, 0.85f, 1.0f, 1);
    glPointSize(2.0f);
    glDrawArrays(GL_POINTS, 2600, 400);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);

    // 1. Earth Sphere
    m_shader3D.use();
    m_shader3D.setFloat("uRoughness",0.5f);
    m_shader3D.setVec3("uSunDir", 150.0f, 40.0f, 80.0f);
    m_shader3D.setVec3("uViewPos", (float)camEye[0], (float)camEye[1], (float)camEye[2]);

    Mat4 earthModel = Mat4::rotateY((float)m_earthSpinAngleRad);
    Mat4 earthMVP = Mat4::multiply(viewProj, earthModel);
    m_shader3D.setMat4("uModel", earthModel);
    m_shader3D.setMat4("uMVP", earthMVP);
    m_shader3D.setInt("uUseTexture", 1);
    m_shader3D.setInt("uTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_earthTexId);
    m_earthMeshes[m_earthLod].draw();

    // Integrate atmospheric density along the view and sunlight rays.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // draw the far shell once, including from inside the atmosphere
    m_atmosphereShader.use();
    m_atmosphereShader.setVec3("uViewPos", (float)camEye[0], (float)camEye[1], (float)camEye[2]);
    m_atmosphereShader.setVec3("uSunDir", 150, 40, 80);
    Mat4 shell = Mat4::scale(1.02f, 1.02f, 1.02f);
    m_atmosphereShader.setMat4("uModel", shell);
    m_atmosphereShader.setMat4("uMVP", Mat4::multiply(viewProj, shell));
    m_earthMeshes[m_earthLod].draw();
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_shader3D.use();

    drawResearchAndTrail(viewProj, width, height);
    m_shader3D.use();

    // 2. Ground Target Pin
    aero::math::Vec3 tgtECEF = latLonToECEF(m_targetLatDeg, m_targetLonDeg, EARTH_RADIUS_3D);
    Mat4 beaconModel = Mat4::multiply(earthModel, Mat4::translate((float)tgtECEF[0], (float)tgtECEF[1], (float)tgtECEF[2]));
    Mat4 beaconMVP = Mat4::multiply(viewProj, beaconModel);
    m_shader3D.setMat4("uModel", beaconModel);
    m_shader3D.setMat4("uMVP", beaconMVP);
    m_shader3D.setInt("uUseTexture", 0);
    m_shader3D.setVec4("uColor", 0.94f, 0.27f, 0.27f, 1.0f);
    if (!m_realScale) m_beaconMesh.draw();

    // The close satellite pass uses meters centered on the satellite. This avoids
    // losing meter-scale detail in float matrices at planetary distances.
    if (m_realScale) {
        if (satelliteHiddenByEarth()) return;
        const auto localEye = (camEye - m_satPosECI) / spacecraftScale();
        const Mat4 localView = Mat4::lookAt(localEye, localEye + m_cameraForward, m_cameraUp);
        const double radius=selectedDetailedModel()?selectedDetailedModel()->radius:2.1;
        const float nearSatellite=float(std::clamp((localEye.norm()-radius)*.05,.001,10000.0));
        viewProj = Mat4::multiply(Mat4::perspective(45.0f * 3.14159265f / 180.0f, aspect, nearSatellite, 1.0e10f), localView);
        glClear(GL_DEPTH_BUFFER_BIT);
        m_shader3D.setVec3("uViewPos", float(localEye[0]), float(localEye[1]), float(localEye[2]));
    }

    // 3. Spacecraft Model
    Mat4 satRot = Mat4::fromQuaternion(m_satState.q.conjugate());
    Mat4 satTrans = Mat4::translate((float)m_satPosECI[0], (float)m_satPosECI[1], (float)m_satPosECI[2]);
    Mat4 satModel = m_realScale ? satRot : Mat4::multiply(satTrans, satRot);

    if (const auto* model=selectedDetailedModel()) {
        // Reduce only the overview model; retain the NASA dimensions in 1:1.
        if (!m_realScale) {float scale=float(overviewModelScale());satModel = Mat4::multiply(satModel, Mat4::scale(scale,scale,scale));}
        m_shader3D.setMat4("uModel",satModel);
        m_shader3D.setMat4("uMVP",Mat4::multiply(viewProj,satModel));
        model->draw(m_shader3D);
    } else {
    // Gold Bus
    Mat4 busMVP = Mat4::multiply(viewProj, satModel);
    m_shader3D.setMat4("uModel", satModel);
    m_shader3D.setMat4("uMVP", busMVP);
    m_shader3D.setVec4("uColor", 0.85f, 0.68f, 0.22f, 1.0f);
    m_busMesh.draw();

    // Solar Wings
    Mat4 leftWing = Mat4::multiply(satModel, Mat4::translate(-1.4f, 0.0f, 0.0f));
    m_shader3D.setMat4("uModel", leftWing);
    m_shader3D.setMat4("uMVP", Mat4::multiply(viewProj, leftWing));
    m_shader3D.setVec4("uColor", 0.11f, 0.31f, 0.85f, 1.0f);
    m_solarPanelMesh.draw();

    Mat4 rightWing = Mat4::multiply(satModel, Mat4::translate(1.4f, 0.0f, 0.0f));
    m_shader3D.setMat4("uModel", rightWing);
    m_shader3D.setMat4("uMVP", Mat4::multiply(viewProj, rightWing));
    m_solarPanelMesh.draw();

    // Telescope Barrel
    Mat4 lensTrans = Mat4::multiply(satModel, Mat4::translate(0.0f, 0.0f, 0.85f));
    Mat4 lensModel = Mat4::multiply(lensTrans, Mat4::rotateX(1.57079f));
    m_shader3D.setMat4("uModel", lensModel);
    m_shader3D.setMat4("uMVP", Mat4::multiply(viewProj, lensModel));
    m_shader3D.setVec4("uColor", 0.12f, 0.16f, 0.24f, 1.0f);
    m_lensMesh.draw();

    }

    drawReentry(viewProj, satModel, height);
    if(m_realScale) drawResearchAndTrail(viewProj,width,height,true);

    if (m_realScale) return; // No oversized guide laser in the physical-scale view.

    // 4. Laser Tracking Beam from Muzzle (GREEN when visible, RED when occulted)
    m_lineShader.use();
    m_lineShader.setMat4("uMVP", viewProj);

    float lineVerts[6] = {
        (float)m_muzzlePosECI[0], (float)m_muzzlePosECI[1], (float)m_muzzlePosECI[2],
        (float)m_targetPosECI[0], (float)m_targetPosECI[1], (float)m_targetPosECI[2]
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lineVerts), lineVerts);

    if (m_isTargetVisible) {
        m_lineShader.setVec4("uColor", 0.06f, 0.73f, 0.51f, 0.95f); // Green
    } else {
        m_lineShader.setVec4("uColor", 0.94f, 0.27f, 0.27f, 0.85f); // Red
    }

    glLineWidth(2.5f);
    glBindVertexArray(m_lineVao);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}


double ADCSView::spacecraftScale() const {
    // Model dimensions are meters: 0.7 x 0.7 x 1.2 m bus, approximately 4.2 m wingspan.
    return m_realScale ? KM_TO_3D / 1000.0 : 1.0;
}
void ADCSView::setRealScale(bool enabled) {
    if (enabled == m_realScale) return;
    const float previousFraming=modelFramingScale();
    m_realScale = enabled;
    m_satOrbitDistance*=modelFramingScale()/previousFraming;
    for(auto& view:m_views)view.camera.satDistance*=modelFramingScale()/previousFraming;
    m_trail={{m_satPosECI,true}};
    m_previousTrailPosition=m_satPosECI;
    m_trailInterval=1;
    m_nextTrailTime=m_simTimeSec+1;
    if (enabled) {
        m_savedTimeWarp = m_timeWarp;
        m_timeWarp = 1;
        m_satelliteSpeedFactor = 1;
        m_earthSpinWarp = 1;
    } else m_timeWarp = m_savedTimeWarp;
    m_physicsAccumulator = 0;
    computeLineOfSight();
}
bool ADCSView::satelliteHiddenByEarth() const {
    const auto ray = m_satPosECI - m_cameraEye;
    const double length2 = ray.dot(ray);
    if (length2 < 1e-20) return false;
    const double t = std::clamp(-m_cameraEye.dot(ray) / length2, 0.0, 1.0);
    return (m_cameraEye + ray*t).norm() < EARTH_RADIUS_3D;
}
float ADCSView::modelFramingScale() const {
    const auto* model=selectedDetailedModel();
    return model ? float(model->radius*(m_realScale?1:overviewModelScale())/2.1) : 1.0f;
}
const SatelliteModel* ADCSView::selectedDetailedModel() const {
    if(m_selectedModel==1)return &m_landsatModel;
    if(m_selectedModel>=2 && m_selectedModel<=5)return &m_additionalModels[m_selectedModel-2];
    return nullptr;
}
double ADCSView::overviewModelScale() const {
    const auto* model=selectedDetailedModel();
    return model ? std::min(0.3,2.5/double(model->radius)) : 1.0;
}
void ADCSView::selectModel(int model) {
    if (model < 0 || model > 5 || (model == 1 && m_landsatModel.parts.empty())
        || (model>=2 && m_additionalModels[model-2].parts.empty())) return;
    float previous = modelFramingScale();
    m_selectedModel = model;
    m_satOrbitDistance *= modelFramingScale() / previous;
    std::ofstream("satellite_model.txt") << model;
}

// Visual engineering estimate, not coupled aerothermochemical CFD.
void ADCSView::updateReentry() {
    using aero::math::Vec3;
    double density=atmosphereDensity(m_orbitAltitudeKm)*(m_numericalOrbit&&!m_replayActive?m_orbitForces.densityScale:1);
    double a=m_trueAnomalyRad, inc=m_orbitInclinationDeg*3.141592653589793/180;
    double speed=std::sqrt(398600.44/(6378.137+m_orbitAltitudeKm))*1000*(m_realScale?m_satelliteSpeedFactor:1);
    Vec3 velocity=Vec3(-std::sin(a),std::cos(a)*std::sin(inc),std::cos(a)*std::cos(inc))*speed;
    if(m_replayActive||m_numericalOrbit)velocity=orbitToScene(m_currentOrbit.velocity)*(1000/KM_TO_3D);
    if(m_entryPreview && m_orbitAltitudeKm>20) velocity=velocity-m_satPosECI.normalized()*(m_descentKmS*1000);
    Vec3 position=m_satPosECI*(1000/KM_TO_3D);
    velocity=velocity-Vec3(0,aero::constants::EARTH_OMEGA,0).cross(position);
    m_airSpeed=velocity.norm();
    m_airDirection=m_satState.q.rotate(velocity.normalized()); // World to body, same convention as rendered model.
    if(m_lowPolyFacets.empty()) {
        auto box=[&](Vec3 center,Vec3 size) {
            for(int axis=0;axis<3;++axis) for(int sign:{-1,1}) {
                Vec3 n(0,0,0); n[axis]=sign;
                // Surface grid keeps the glow attached to bus and deployed panels.
                int u=(axis+1)%3,v=(axis+2)%3;
                for(int i=0;i<8;++i) for(int j=0;j<8;++j) {
                    Vec3 c=center; c[axis]+=sign*size[axis]/2;
                    c[u]+=((i+.5)/8-.5)*size[u]; c[v]+=((j+.5)/8-.5)*size[v];
                    m_lowPolyFacets.push_back({c,n,size[u]*size[v]/64});
                }
            }
        };
        box({0,0,0},{.7,.7,1.2});box({-1.4,0,0},{1.4,.04,.8});box({1.4,0,0},{1.4,.04,.8});
    }
    const auto* detailed=selectedDetailedModel();
    const auto& facets=detailed?detailed->facets:m_lowPolyFacets;
    m_projectedArea=0;
    for(const auto& f:facets) m_projectedArea+=f.area*std::max(0.0,f.normal.dot(m_airDirection));
    double radius=std::clamp(std::sqrt(m_projectedArea/3.141592653589793),.05,8.0);
    m_heatFlux=1.74153e-4*std::sqrt(density/radius)*std::pow(m_airSpeed,3);
    m_dynamicPressure=.5*density*m_airSpeed*m_airSpeed;
}
void ADCSView::updateAlarm(bool active) {
    if(active==m_alarmPlaying) return;
#ifdef _WIN32
    // Persistent in-memory PCM: two alternating warning tones, then a quiet gap.
    static const std::vector<char> wave=[] {
        constexpr int rate=22050, samples=rate;
        std::vector<char> w(44+samples*2,0);
        auto put=[&](int offset,uint32_t value,int bytes){for(int i=0;i<bytes;++i)w[offset+i]=char(value>>(8*i));};
        std::memcpy(w.data(),"RIFF",4);put(4,uint32_t(w.size()-8),4);std::memcpy(w.data()+8,"WAVEfmt ",8);
        put(16,16,4);put(20,1,2);put(22,1,2);put(24,rate,4);put(28,rate*2,4);put(32,2,2);put(34,16,2);
        std::memcpy(w.data()+36,"data",4);put(40,samples*2,4);
        for(int i=0;i<samples;++i){double t=double(i)/rate, phase=std::fmod(t,.25);
            double env=t<.5?std::min({1.0,phase/.01,(.25-phase)/.02}):0;
            int16_t sample=int16_t(4500*env*std::sin(6.283185307*t*(t<.25?880:1175)));
            put(44+i*2,uint16_t(sample),2);
        } return w;
    }();
    if(active) PlaySoundA(wave.data(),nullptr,SND_MEMORY|SND_ASYNC|SND_LOOP|SND_NODEFAULT);
    else PlaySoundA(nullptr,nullptr,0);
#endif
    m_alarmPlaying=active;
}
void ADCSView::drawReentry(const Mat4& viewProj,const Mat4& satModel,int height) {
    if(m_heatFlux<=100000) return;
    if(!m_entryVao) {
        const char* vs=R"(#version 330 core
layout(location=0) in vec3 p; layout(location=1) in float heat;
uniform mat4 uMVP; uniform float uSize; out float glow;
void main(){gl_Position=uMVP*vec4(p,1);gl_PointSize=clamp(uSize/max(gl_Position.w,.01),2,100);glow=heat;})";
        const char* fs=R"(#version 330 core
in float glow; out vec4 color;
void main(){float r=length(gl_PointCoord-vec2(.5))*2;if(r>1)discard;
color=vec4(mix(vec3(1,.12,.01),vec3(1,.8,.35),glow),pow(1-r,2)*.6*glow);})";
        if(!m_entryShader.loadFromSource(vs,fs))return;
        glGenVertexArrays(1,&m_entryVao);glGenBuffers(1,&m_entryVbo);
        glBindVertexArray(m_entryVao);glBindBuffer(GL_ARRAY_BUFFER,m_entryVbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(3*sizeof(float)));
    }
    const auto* detailed=selectedDetailedModel();
    const auto& facets=detailed?detailed->facets:m_lowPolyFacets;
    std::vector<float> particles; particles.reserve(24000);
    double intensity=std::clamp((m_heatFlux-100000)/500000.0,0.0,1.0);
    size_t step=std::max(size_t(1),facets.size()/1200);
    double radius=detailed?detailed->radius:2.1;
    for(size_t i=0;i<facets.size();i+=step){const auto& f=facets[i];double facing=f.normal.dot(m_airDirection);
        if(facing<.05)continue;
        for(int j=0;j<4;++j){double age=std::fmod(m_simTimeSec*.8+double(i)*.618+j*.25,1.0);
            auto point=f.center+f.normal*(.1+radius*.06)-m_airDirection*(age*radius*2.5);
            auto side=m_airDirection.cross(f.normal);
            point=point+side*(std::sin(age*15+i)*age*radius*.15);
            particles.insert(particles.end(),{float(point[0]),float(point[1]),float(point[2]),float(intensity*std::sqrt(facing)*(1-age))});
        }
    }
    m_entryShader.use();m_entryShader.setMat4("uMVP",Mat4::multiply(viewProj,satModel));
    m_entryShader.setFloat("uSize",float(height*radius*.10*(!m_realScale?overviewModelScale():1)));
    glBindVertexArray(m_entryVao);glBindBuffer(GL_ARRAY_BUFFER,m_entryVbo);
    glBufferData(GL_ARRAY_BUFFER,particles.size()*sizeof(float),particles.data(),GL_STREAM_DRAW);
    glEnable(GL_PROGRAM_POINT_SIZE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS,0,GLsizei(particles.size()/4));
    glDepthMask(GL_TRUE);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_PROGRAM_POINT_SIZE);glBindVertexArray(0);
}


void ADCSView::secretKey(int key,int action,bool typing) {
    if(action!=GLFW_PRESS || m_researchUnlocked) return;
    if(typing) {m_secretProgress=0;return;}
    if(key==GLFW_KEY_LEFT_SHIFT || key==GLFW_KEY_RIGHT_SHIFT || key==GLFW_KEY_LEFT_CONTROL || key==GLFW_KEY_RIGHT_CONTROL) return;
    constexpr int code[]={GLFW_KEY_UP,GLFW_KEY_UP,GLFW_KEY_DOWN,GLFW_KEY_DOWN,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_B,GLFW_KEY_A,GLFW_KEY_ENTER};
    // Keep the longest suffix matching a prefix, including overlapping Up presses.
    std::vector<int> entered(code,code+m_secretProgress);entered.push_back(key);
    m_secretProgress=0;
    for(size_t length=1;length<=std::min(entered.size(),size_t(11));++length)
        if(std::equal(entered.end()-length,entered.end(),code))m_secretProgress=unsigned(length);
    if(m_secretProgress==11) {
        m_researchUnlocked=true;m_secretProgress=0;
        m_discoveryUntil=ImGui::GetTime()+6.0;
        std::ofstream("research_mode_unlocked.txt")<<1;
    }
}
void ADCSView::drawDiscoveryAlert() {
    if(ImGui::GetTime()>=m_discoveryUntil)return;
    auto size=ImGui::GetIO().DisplaySize;
    if(size.x<80 || size.y<80)return;
    const char* message="A new mode has been discovered!";
    auto* font=ImGui::GetFont();
    float fontSize=std::min(28.0f,(size.x-32)/float(std::strlen(message))/.62f);
    auto text=font->CalcTextSizeA(fontSize,FLT_MAX,0,message);
    ImVec2 position((size.x-text.x)/2,std::min(size.y-text.y-24,size.y*.5f+200));
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({position.x-14,position.y-10},{position.x+text.x+14,position.y+text.y+10},IM_COL32(0,0,0,220),6);
    // Two color cycles per second; text remains visible throughout the six seconds.
    ImU32 color=int(ImGui::GetTime()*4)%2?IM_COL32(255,60,40,255):IM_COL32(255,235,40,255);
    draw->AddText(font,fontSize,position,color,message);
}
void ADCSView::recordTrail() {
    bool jump=(m_satPosECI-m_previousTrailPosition).norm()>10*KM_TO_3D;
    m_previousTrailPosition=m_satPosECI;
    if(m_trail.empty())m_trail.push_back({m_satPosECI,true});
    if(jump || m_simTimeSec>=m_nextTrailTime) {
        m_trail.push_back({m_satPosECI,jump});m_nextTrailTime=m_simTimeSec+m_trailInterval;
        if(m_trail.size()>65536) {
            std::vector<TrailPoint> simplified;simplified.reserve(m_trail.size()/2+2);
            for(size_t i=0;i<m_trail.size();++i)
                if(i==0 || i+1==m_trail.size() || i%2==0 || m_trail[i].start || m_trail[i+1].start) simplified.push_back(m_trail[i]);
            m_trail.swap(simplified);m_trailInterval*=2;
        }
    }
}
void ADCSView::drawResearchAndTrail(const Mat4& vp,int width,int height,bool closePass) {
    using aero::math::Vec3;
    if(width<=0 || height<=0)return;
    if(!closePass)drawReplayComparison(vp);
    auto occulted=[&](Vec3 p){auto delta=p-m_cameraEye;double length=delta.norm();if(length<1e-10)return false;
        auto direction=delta/length;double b=m_cameraEye.dot(direction),c=m_cameraEye.dot(m_cameraEye)-EARTH_RADIUS_3D*EARTH_RADIUS_3D;
        double disc=b*b-c;if(disc<0)return false;
        double hit=-b-std::sqrt(disc);return hit>0 && hit<length;
    };
    if(!closePass && m_showResearch && m_researchUnlocked) {
        std::vector<float> points; m_catalogRendered=0;
        std::string hover;float nearest=100;
        auto mouse=ImGui::GetIO().MousePos;
        for(size_t i=0;i<m_researchCatalog.satellites.size();++i) {
            Vec3 p;if(!m_researchCatalog.position(i,m_simTimeSec,p))continue;
            p=p*KM_TO_3D;double c=std::cos(m_earthSpinAngleRad),s=std::sin(m_earthSpinAngleRad);
            p={c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
            points.insert(points.end(),{float(p.x),float(p.y),float(p.z)});++m_catalogRendered;
            double clip[4]{};for(int row=0;row<4;++row)clip[row]=vp.m[row]*p.x+vp.m[4+row]*p.y+vp.m[8+row]*p.z+vp.m[12+row];
            if(clip[3]>0 && !occulted(p)) {
                float x=float((clip[0]/clip[3]*.5+.5)*width),y=float((.5-clip[1]/clip[3]*.5)*height);
                // Framebuffer to ImGui coordinates (also works on high-DPI displays).
                x*=ImGui::GetIO().DisplaySize.x/width;y*=ImGui::GetIO().DisplaySize.y/height;
                float distance=(x-mouse.x)*(x-mouse.x)+(y-mouse.y)*(y-mouse.y);
                if(distance<nearest){nearest=distance;hover=m_researchCatalog.satellites[i].name+" | NORAD "+std::to_string(m_researchCatalog.satellites[i].norad);}
            }
        }
        m_lineShader.use();m_lineShader.setMat4("uMVP",vp);m_lineShader.setVec4("uColor",.35f,.85f,1,1);
        glBindVertexArray(m_lineVao);glBindBuffer(GL_ARRAY_BUFFER,m_lineVbo);
        glBufferData(GL_ARRAY_BUFFER,std::max(size_t(3072),points.size())*sizeof(float),nullptr,GL_DYNAMIC_DRAW);
        if(!points.empty())glBufferSubData(GL_ARRAY_BUFFER,0,points.size()*sizeof(float),points.data());
        glPointSize(5);glDrawArrays(GL_POINTS,0,GLsizei(points.size()/3));glPointSize(1);glBindVertexArray(0);
        if(!m_renderingMulti && !hover.empty() && sceneAcceptsInput()) {
            auto* draw=ImGui::GetForegroundDrawList();auto extent=ImGui::CalcTextSize(hover.c_str());
            ImVec2 at(std::max(8.0f,std::min(mouse.x+12,ImGui::GetIO().DisplaySize.x-extent.x-8)),std::max(8.0f,mouse.y-25));
            draw->AddRectFilled({at.x-4,at.y-3},{at.x+extent.x+4,at.y+extent.y+3},IM_COL32(0,0,0,225),3);
            draw->AddText(at,IM_COL32(160,230,255,255),hover.c_str());
        }
    }
    if(!m_showTrail || m_trail.size()<1 || (m_realScale!=closePass))return;
    if(!m_historyVao) {
        if(!m_ribbonShader.loadFromSource("#version 330 core\nlayout(location=0)in vec4 clip;void main(){gl_Position=clip;}",
            "#version 330 core\nout vec4 color;void main(){color=vec4(.55,1,.65,1);}"))return;
        glGenVertexArrays(1,&m_historyVao);glGenBuffers(1,&m_historyVbo);
        glBindVertexArray(m_historyVao);glBindBuffer(GL_ARRAY_BUFFER,m_historyVbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);
    }
    std::vector<float> vertices;vertices.reserve(m_trail.size()*24);
    auto segment=[&](Vec3 start,Vec3 end){
        if((end-start).norm()<1e-12 || occulted((start+end)*.5))return;
        double distanceKm=(((start+end)*.5)-m_cameraEye).norm()/KM_TO_3D;
        double pixels=1.1+2.2*std::clamp(distanceKm/3000.0,0.0,1.0);
        auto project=[&](Vec3 p){if(closePass)p=(p-m_satPosECI)/spacecraftScale();std::array<double,4> clip{};
            for(int row=0;row<4;++row)clip[row]=vp.m[row]*p.x+vp.m[4+row]*p.y+vp.m[8+row]*p.z+vp.m[12+row];return clip;};
        auto a=project(start),b=project(end);
        double da=a[2]+a[3],db=b[2]+b[3];if(da<=0 && db<=0)return;
        if(da<=0 || db<=0){double t=da/(da-db);auto clipped=a;for(int i=0;i<4;++i)clipped[i]=a[i]+t*(b[i]-a[i]);if(da<=0)a=clipped;else b=clipped;}
        if(a[3]<=1e-12 || b[3]<=1e-12)return;
        double dx=(b[0]/b[3]-a[0]/a[3])*width,dy=(b[1]/b[3]-a[1]/a[3])*height;
        double norm=std::hypot(dx,dy);if(norm<1e-9)return;
        double nx=-dy/norm*pixels/width,ny=dx/norm*pixels/height;
        auto vertex=[&](const auto& p,int sign){vertices.insert(vertices.end(),{float(p[0]+sign*nx*p[3]),float(p[1]+sign*ny*p[3]),float(p[2]),float(p[3])});};
        vertex(a,1);vertex(a,-1);vertex(b,1);vertex(b,1);vertex(a,-1);vertex(b,-1);
    };
    for(size_t i=1;i<m_trail.size();++i)if(!m_trail[i].start)segment(m_trail[i-1].position,m_trail[i].position);
    segment(m_trail.back().position,m_satPosECI);
    m_ribbonShader.use();glBindVertexArray(m_historyVao);glBindBuffer(GL_ARRAY_BUFFER,m_historyVbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()/4));glBindVertexArray(0);
}

void ADCSView::drawSettings() {
    if (m_openSettings) { ImGui::OpenPopup("Camera & simulation settings"); m_openSettings = false; }
    ImGui::SetNextWindowSize(ImVec2(std::min(640.0f, ImGui::GetIO().DisplaySize.x-24), std::min(760.0f,ImGui::GetIO().DisplaySize.y-70)), ImGuiCond_Appearing);
    bool open = true;
    if (ImGui::BeginPopupModal("Camera & simulation settings", &open)) {
        ImGui::PushItemWidth(220);
        drawInternetSettings();
        ImGui::Checkbox("Satellite trail since t=0", &m_showTrail);
        explain("Light-green flight history recorded from simulation start, even while hidden. Older samples are progressively simplified. It gets visually thinner nearby. Position jumps start a new segment.");
        if(m_researchUnlocked) {
            ImGui::BeginDisabled(m_researchCatalog.satellites.empty());
            if(ImGui::Checkbox("Research satellites", &m_showResearch) && m_showResearch) {
                m_camPreset=CameraPreset::FreeOrbitArcball;
                m_arcballCenter={0,0,0};m_arcballDist=42;
            }
            ImGui::EndDisabled();
            ImGui::TextWrapped("%s",m_researchCatalog.status.c_str());
            ImGui::TextWrapped("CelesTrak: science, Earth resources and weather. Hover a dot for its name. Prediction starts at session UTC and advances with simulation time. Older elements are less reliable; not live tracking.");
            if(m_simTimeSec>7*86400) ImGui::TextWrapped("Orbital predictions become less reliable far from the catalog date.");
        }
        ImGui::Separator();
        ImGui::BeginDisabled(m_replayActive||m_numericalOrbit);
        if (ImGui::Checkbox("Atmospheric entry preview", &m_entryPreview)) {
            m_orbitAltitudeKm=m_entryPreview ? 80 : 650;
            if(m_entryPreview) m_camPreset=CameraPreset::SatelliteOrbit;
        }
        explain("Prescribed descent with geometry-dependent heating. Does not solve drag-driven trajectory, ablation or breakup. Disable to return to 650 km.");
        if(m_entryPreview) {
            float altitude=float(m_orbitAltitudeKm);
            if(sliderFloatExact("Entry altitude",&altitude,20,150,"%.1f km","%.1f")) m_orbitAltitudeKm=altitude;
            sliderFloatExact("Descent speed",&m_descentKmS,0,1,"%.2f km/s","%.2f");
        }
        ImGui::Separator();
        ImGui::EndDisabled();
        const char* names[]={"Low poly","Landsat 8","Hubble Space Telescope","Terra","GOES-R","International Space Station"};
        if (ImGui::BeginCombo("Satellite model", names[m_selectedModel])) {
            for(int i=0;i<6;++i) {
                const bool missing=i==1?m_landsatModel.parts.empty():i>=2?m_additionalModels[i-2].parts.empty():false;
                ImGui::BeginDisabled(missing);
                if(ImGui::Selectable(names[i],m_selectedModel==i))selectModel(i);
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        explain("Choose a locally stored spacecraft. Switching updates geometry and camera framing while preserving the orbit and control simulation.");
        if (m_selectedModel == 1) ImGui::TextDisabled("Model: NASA / Christopher R. Meaney");
        if (m_selectedModel >= 2) ImGui::TextDisabled(m_selectedModel==3?"Model: NASA / Christian A. Lopez":"Model: NASA");
        if (m_landsatModel.parts.empty()) ImGui::TextWrapped("Landsat 8 model could not be loaded.");
        ImGui::Separator();
        sliderFloatExact("Look sensitivity", &m_sensitivity, 0.0002f, 0.012f, "%.4f rad/px", "%.4f", ImGuiSliderFlags_Logarithmic);
        explain("Mouse-look rotation per pixel. Applies to free look; lower values give finer aim.");
        sliderFloatExact("Wheel sensitivity", &m_scrollSensitivity, 0.1f, 5.0f, "%.2fx", "%.2f");
        explain("Zoom/dolly sensitivity. The wheel affects the scene only while the cursor is outside every panel and popup.");
        ImGui::Separator();
        bool realScale = m_realScale;
        if (ImGui::Checkbox("1:1 physical scale & real time", &realScale)) setRealScale(realScale);
        explain("Earth and orbit retain their physical dimensions. The selected spacecraft uses meter-scale geometry. One real second advances one simulation second.");
        ImGui::Checkbox("Force quick repositioning to target",&m_quickReposition);
        explain("Override the conservative 1:1 slew profile with the fast demonstration controller. Off: 0.25 degrees/s (Hubble: 0.1), with 0.02 degrees/s squared acceleration. These are illustrative limits, not vehicle-specific flight software.");
        ImGui::TextWrapped(m_selectedModel == 0
            ? "Earth radius: 6,378 km. Satellite bus: 0.7 x 0.7 x 1.2 m; wingspan: about 4.2 m."
            : "Earth radius: 6,378 km. Detailed spacecraft use meter-scale geometry; Terra's source units are calibrated to its 6.8 m bus. Model selection does not change mass, inertia or orbital elements.");
        ImGui::BeginDisabled(!m_realScale);
        sliderFloatExact("User speed", &m_userSpeedKmS, 0.001f, 1000.0f, "%.3f km/s", "%.3f", ImGuiSliderFlags_Logarithmic);
        explain("Free-flight speed in kilometers per real second. Shift triples it. Diagonal movement is normalized.");
        ImGui::BeginDisabled(m_replayActive||m_numericalOrbit);
        sliderFloatExact("Satellite speed", &m_satelliteSpeedFactor, 0.0f, 20.0f, "%.2fx circular orbit", "%.2f");
        explain("One is the circular-orbit speed at this altitude. Zero holds orbital position; other values override orbital motion for experimentation without changing the simulation clock or Earth rotation.");
        double orbitalSpeed = m_replayActive||m_numericalOrbit?m_currentOrbit.velocity.norm():std::sqrt(398600.44 / (6378.137 + m_orbitAltitudeKm));
        ImGui::Text("Satellite: %.3f km/s", orbitalSpeed * m_satelliteSpeedFactor);
        if (ImGui::Button("Reset speeds to orbital speed")) {
            m_satelliteSpeedFactor = 1;
            m_userSpeedKmS = float(orbitalSpeed);
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Go beside satellite (20 m)")) {
            m_camPreset = CameraPreset::FreeFlycam;
            m_flycamPos = m_satPosECI + m_satPosECI.normalized() * (20 * spacecraftScale());
            m_flyForward = (m_satPosECI - m_flycamPos).normalized();
            auto up = std::abs(m_flyForward[1]) < 0.9 ? aero::math::Vec3(0,1,0) : aero::math::Vec3(1,0,0);
            m_flyUp = m_flyForward.cross(up).normalized().cross(m_flyForward);
            ImGui::CloseCurrentPopup();
        }
        explain("Place the free camera 20 meters above the satellite. It continues moving at the selected speed; set satellite speed to zero to inspect it at rest.");
        ImGui::EndDisabled();
        ImGui::TextWrapped("WASD: fly | E/Q: rise/descend | Shift: boost. Move the pointer onto the 3D scene to fly. Hold right mouse to look.");
        drawExpertSettings();
        ImGui::PopItemWidth();
        if (ImGui::Button("Close", ImVec2(-1, 28))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void ADCSView::drawFlightOverlay() {
    if (!m_realScale) return;
    const auto display = ImGui::GetIO().DisplaySize;
    if (display.x < 100 || display.y < 100) return;
    auto* draw = ImGui::GetBackgroundDrawList();
    const auto delta = m_satPosECI - m_cameraEye;
    const auto right = m_cameraForward.cross(m_cameraUp);
    double x = delta.dot(right), y = delta.dot(m_cameraUp), z = delta.dot(m_cameraForward);
    const double tanHalf = std::tan(45.0 * 3.141592653589793 / 360.0);
    double nx = x / (std::max(std::abs(z), 1e-9) * tanHalf * display.x/display.y);
    double ny = -y / (std::max(std::abs(z), 1e-9) * tanHalf);
    const bool offscreen = z <= 0 || std::abs(nx) > 0.92 || std::abs(ny) > 0.86;
    if (offscreen) {
        if (std::abs(nx)+std::abs(ny) < 1e-8) nx = 1;
        double factor = std::max(std::abs(nx)/0.88, std::abs(ny)/0.80);
        nx /= factor; ny /= factor;
    }
    ImVec2 point(float((nx+1)*display.x/2), float((ny+1)*display.y/2));
    const bool occluded = satelliteHiddenByEarth();
    ImU32 color = occluded ? IM_COL32(255,185,65,255) : IM_COL32(110,225,255,255);
    draw->AddCircleFilled(point, 4, color);
    draw->AddCircle(point, 8, color, 24, 1.5f);
    char label[128];
    snprintf(label, sizeof(label), "SAT-01 | %.3f km%s", delta.norm()/KM_TO_3D,
        z <= 0 ? " | behind camera" : offscreen ? " | off screen" : occluded ? " | behind Earth" : "");
    ImVec2 size = ImGui::CalcTextSize(label);
    ImVec2 textPos(std::clamp(point.x+12, 8.0f, std::max(8.0f, display.x-size.x-8)),
                   std::clamp(point.y-24, 40.0f, display.y-24));
    draw->AddRectFilled(ImVec2(textPos.x-4,textPos.y-3), ImVec2(textPos.x+size.x+4,textPos.y+size.y+3), IM_COL32(0,0,0,200), 3);
    draw->AddText(textPos, color, label);
    char speed[128];
    snprintf(speed, sizeof(speed), "USER %.3f km/s  |  SET %.3f km/s", m_actualUserSpeedKmS, m_userSpeedKmS);
    ImFont* font = ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();
    auto textSize = font->CalcTextSizeA(20, FLT_MAX, 0, speed);
    // Right of the default charts when space permits; otherwise keep it on screen.
    float panelWidth = std::max(120.0f, std::min(360.0f, (display.x-30)*0.45f));
    ImVec2 speedPos(std::min(panelWidth+24, std::max(8.0f, display.x-textSize.x-12)), display.y*0.55f);
    draw->AddRectFilled(ImVec2(speedPos.x-5,speedPos.y-4), ImVec2(speedPos.x+textSize.x+5,speedPos.y+textSize.y+4), IM_COL32(0,0,0,190), 3);
    draw->AddText(font, 20, speedPos, IM_COL32(255,55,55,255), speed);
}

void ADCSView::drawUI() {
    drawTelemetryHUD();
    drawCharts();
    drawControlPanel();
    draw2DWorldMap();
    drawOrbitReplay();
    drawSettings();
    drawInternetChoice();

    // Draw Optical Reticle in Sensor Lookdown mode
    if (!m_multiView && m_camPreset == CameraPreset::TelescopeLookdown) {
        ImGuiIO& io = ImGui::GetIO();
        drawTelescopeReticle((int)io.DisplaySize.x, (int)io.DisplaySize.y);
    }
}

void ADCSView::drawTelescopeReticle(int width, int height) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 center((float)width * 0.5f, (float)height * 0.5f);

    ImU32 reticleColor = m_isTargetVisible ? IM_COL32(16, 185, 129, 200) : IM_COL32(239, 68, 68, 200);

    drawList->AddCircle(center, 90.0f, reticleColor, 64, 1.5f);
    drawList->AddCircle(center, 40.0f, reticleColor, 32, 1.0f);
    drawList->AddCircleFilled(center, 3.0f, reticleColor);

    // Crosshairs
    drawList->AddLine(ImVec2(center.x - 120, center.y), ImVec2(center.x - 50, center.y), reticleColor, 1.5f);
    drawList->AddLine(ImVec2(center.x + 50, center.y), ImVec2(center.x + 120, center.y), reticleColor, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y - 120), ImVec2(center.x, center.y - 50), reticleColor, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y + 50), ImVec2(center.x, center.y + 120), reticleColor, 1.5f);

    // Target Range text
    char buf[64];
    snprintf(buf, sizeof(buf), "RANGE: %.1f KM | SLEW ERR: %.2f deg", m_slantRangeKm, m_slewErrorDeg);
    drawList->AddText(ImVec2(center.x - 110, center.y + 105), reticleColor, buf);
}

void ADCSView::drawTelemetryHUD() {
    beginEdgePanel("GNC & Orbital Telemetry", 0);
    if (m_heatFlux > 100000) {
        float pulse=0.35f+0.65f*float(0.5+0.5*std::sin(ImGui::GetTime()*9));
        ImGui::TextColored(ImVec4(1,pulse*0.35f,0.05f,1), "REENTRY HEATING!");
        ImGui::SameLine();
        if(ImGui::SmallButton(m_alarmMuted ? "Unmute" : "Mute")) {
            m_alarmMuted=!m_alarmMuted; updateAlarm(!m_alarmMuted);
        }
    }
    if (m_entryPreview || m_heatFlux > 100000) {
        ImGui::Text("Heat estimate: %.0f kW/m2", m_heatFlux/1000);
        explain("Approximate Sutton-Graves stagnation heat flux; not a material failure prediction.");
        ImGui::Text("Air speed %.2f km/s | q %.0f Pa",m_airSpeed/1000,m_dynamicPressure);
        ImGui::Text("Windward facet area: %.2f m2",m_projectedArea);
    }


    if (m_isTargetVisible) {
        if (m_slewErrorDeg < 0.5) {
            ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "LOCKED ON (LOS ACQUIRED)");
        } else {
            ImGui::TextColored(ImVec4(0.96f, 0.62f, 0.04f, 1.0f), "SLEWING (TARGET IN LOS)");
        }
    } else {
        ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "OCCULTED (TARGET BEHIND EARTH)");
    }
    explain("Tracking state: locked below 0.5 degrees pointing error, slewing while aligning, or occulted when Earth blocks the target.");
    ImGui::Separator();

    ImGui::Text("Target Name:    %s", m_targetName.c_str());
    explain("The selected ground location tracked by the spacecraft optical boresight.");
    ImGui::Text("Target (ECEF):  %.2f° N, %.2f° E", m_targetLatDeg, m_targetLonDeg);
    explain("Earth-fixed latitude and longitude. Negative latitude is south; negative longitude is west. The location rotates with Earth.");
    ImGui::Text("Target Regime:  %s", m_isTargetDay ? "LOCAL DAY" : "LOCAL NIGHT");
    explain("Day or night at the ground target, determined by its surface normal relative to the Sun.");

    ImGui::Separator();
    if (m_isTargetVisible) {
        ImGui::TextColored(ImVec4(0.06f, 0.73f, 0.51f, 1.0f), "LOS Status:     VISIBLE (+%.1f° El)", m_elevationDeg);
    explain("Line of sight from the ground target to the spacecraft. Elevation above zero means the satellite is above the local horizon.");
    } else {
        ImGui::TextColored(ImVec4(0.94f, 0.27f, 0.27f, 1.0f), "LOS Status:     OCCULTED (%.1f° El)", m_elevationDeg);
    explain("Line of sight from the ground target to the spacecraft. Elevation above zero means the satellite is above the local horizon.");
    }
    ImGui::Text("Slant Range:    %.1f km", m_slantRangeKm);
    explain("Straight-line distance between the spacecraft and the ground target, in kilometers.");
    ImGui::Text("Solar Regime:   %s", m_isSatSunlit ? "100% [SUNLIT]" : "0% [UMBRA / ECLIPSE]");
    explain("Whether Earth blocks sunlight from reaching the satellite. This simulation uses a binary sunlit/umbra model.");

    ImGui::Separator();
    ImGui::Text("Slew Error:     %.2f°", m_slewErrorDeg);
    explain("Angular separation between the telescope boresight and the commanded target direction. Below 0.5 degrees the target is considered locked.");
    ImGui::Text("Angular Rate:   %.3f °/s", m_bodyRateNormDegS);
    explain("Magnitude of spacecraft rotation about all three body axes, in degrees per second.");
    ImGui::Text("MEKF Covariance: 3-sigma %.3f deg",
        std::max({m_mekf.attitude3Sigma()[0], m_mekf.attitude3Sigma()[1], m_mekf.attitude3Sigma()[2]}) * 180.0 / 3.14159265);
    explain("Largest per-axis three-sigma attitude uncertainty calculated from the multiplicative extended Kalman filter covariance.");

    ImGui::End();
}

void ADCSView::drawCharts() {
    beginEdgePanel("Live Strip-Charts (ImPlot)", 1);

    if (ImPlot::BeginPlot("Body Rate |omega| (°/s)", ImVec2(-1, 160))) {
        ImPlot::SetupAxes("Sim Time (s)", "Rate (°/s)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (!m_bufOmegaNorm.timeData.empty()) {
            ImPlot::PlotLine("Norm", m_bufOmegaNorm.timeData.data(), m_bufOmegaNorm.valData.data(), (int)m_bufOmegaNorm.timeData.size());
            ImPlot::PlotLine("omega_x", m_bufOmegaX.timeData.data(), m_bufOmegaX.valData.data(), (int)m_bufOmegaX.timeData.size());
            ImPlot::PlotLine("omega_y", m_bufOmegaY.timeData.data(), m_bufOmegaY.valData.data(), (int)m_bufOmegaY.timeData.size());
            ImPlot::PlotLine("omega_z", m_bufOmegaZ.timeData.data(), m_bufOmegaZ.valData.data(), (int)m_bufOmegaZ.timeData.size());
        }
        ImPlot::EndPlot();
    }
    explain("Live history against simulation time. Body rates show the magnitude and X/Y/Z components in degrees per second. Gyro biases compare filter estimates with the injected sensor offset in radians per second.");

    if (ImPlot::BeginPlot("Gyro Bias Estimation vs Truth (rad/s)", ImVec2(-1, 160))) {
        ImPlot::SetupAxes("Sim Time (s)", "Bias", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (!m_bufBiasEstX.timeData.empty()) {
            ImPlot::PlotLine("True bias_x", m_bufBiasTrueX.timeData.data(), m_bufBiasTrueX.valData.data(), (int)m_bufBiasTrueX.timeData.size());
            ImPlot::PlotLine("Est bias_x", m_bufBiasEstX.timeData.data(), m_bufBiasEstX.valData.data(), (int)m_bufBiasEstX.timeData.size());
            ImPlot::PlotLine("Est bias_y", m_bufBiasEstY.timeData.data(), m_bufBiasEstY.valData.data(), (int)m_bufBiasEstY.timeData.size());
            ImPlot::PlotLine("Est bias_z", m_bufBiasEstZ.timeData.data(), m_bufBiasEstZ.valData.data(), (int)m_bufBiasEstZ.timeData.size());
        }
        ImPlot::EndPlot();
    }
    explain("Live history against simulation time. Body rates show the magnitude and X/Y/Z components in degrees per second. Gyro biases compare filter estimates with the injected sensor offset in radians per second.");

    ImGui::End();
}

void ADCSView::drawControlPanel() {
    beginEdgePanel("Flight Director & Camera", 2);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (ImGui::Button("Settings / 1:1 mode", ImVec2(-1, 28))) m_openSettings = true;
    if (ImGui::Button("Recorded orbit replay / import",ImVec2(-1,28)))m_replayWindow=true;

    if (ImGui::Button(m_isPaused ? "Resume Sim" : "Pause Sim", ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 28))) {
        m_isPaused = !m_isPaused;
    }
    explain("Pause or resume simulation time. Camera movement and rendering continue.");
    ImGui::SameLine();
    if (ImGui::Button("Inject Tumble", ImVec2(-1, 28))) {
        injectTumble();
    }
    explain("Apply a random angular velocity disturbance to test attitude recovery.");

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "EARTH ROTATION & SPIN:");
    explain("Earth rotates around its polar axis. Manual rotation changes the longitude facing the spacecraft.");
    ImGui::BeginDisabled(m_replayActive);
    ImGui::Checkbox("Auto-Spin Diurnal (15°/h)", &m_autoEarthSpin);
    explain("Rotate Earth automatically at its diurnal rate, scaled by simulation time warp and spin warp.");
    ImGui::Checkbox("Drag Earth", &m_isEarthManualRotate);
    explain("Use left-drag to rotate Earth instead of orbiting the camera. Ctrl plus left-drag also rotates Earth.");

    float earthRotDeg = (float)(m_earthSpinAngleRad * 180.0 / 3.14159265);
    while (earthRotDeg < 0.0f) earthRotDeg += 360.0f;
    while (earthRotDeg >= 360.0f) earthRotDeg -= 360.0f;
    if (sliderFloatExact("Manual Rotation", &earthRotDeg, 0.0f, 360.0f, "%.1f°", "%.1f")) {
        m_earthSpinAngleRad = earthRotDeg * (3.14159265 / 180.0);
    }
    explain("Earth rotation angle about the polar axis, in degrees.");

    const float third = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
    if (ImGui::Button("East +15", ImVec2(third, 24))) {
        m_earthSpinAngleRad += 15.0 * (3.14159265 / 180.0);
    }
    explain("Rotate Earth eastward by 15 degrees.");
    ImGui::SameLine();
    if (ImGui::Button("West -15", ImVec2(third, 24))) {
        m_earthSpinAngleRad -= 15.0 * (3.14159265 / 180.0);
    }
    explain("Rotate Earth westward by 15 degrees.");
    ImGui::SameLine();
    if (ImGui::Button("Center City", ImVec2(third, 24))) {
        m_earthSpinAngleRad = -m_targetLonDeg * (3.14159265 / 180.0);
    }
    explain("Rotate the selected target longitude to the reference meridian.");
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "CAMERA PERSPECTIVES:");
    drawMultiViewControls();
    explain("Choose a viewpoint. Right-drag enters free flight from the current view without jumping to a new position.");
    ImGui::TextDisabled("Controls: Hold Right-Click + WASD to fly");
    explain("Hold right mouse to look in any direction. WASD moves, E/Space rises, Q/C descends, Shift accelerates, and Escape releases the pointer.");

    ImGui::BeginDisabled(m_multiView);
    if (ImGui::Selectable("  Free Flycam (WASD + Right-Click)", m_camPreset == CameraPreset::FreeFlycam)) {
        enterFlycam();
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Free Orbit Arcball (Left-Click)", m_camPreset == CameraPreset::FreeOrbitArcball)) {
        m_camPreset = CameraPreset::FreeOrbitArcball;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Satellite Orbit (Centered + Left-Drag)", m_camPreset == CameraPreset::SatelliteOrbit)) {
        m_camPreset = CameraPreset::SatelliteOrbit;
    }
    explain("Keeps the moving satellite at screen center. Left-drag orbits fully around it, including over the poles; wheel changes distance. Works at both display scales. Right-drag or WASD returns to free flight.");
    if (m_camPreset == CameraPreset::SatelliteOrbit) {
        if (ImGui::Button("Reset satellite orbit view")) {
            m_satOrbitYaw = 0.8f; m_satOrbitPitch = 0.35f; m_satOrbitDistance = 8.0f * modelFramingScale();
        }
    }
    if (ImGui::Selectable("  Orbit Chase Cam (Over Shoulder)", m_camPreset == CameraPreset::ChaseCam)) {
        m_camPreset = CameraPreset::ChaseCam;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Telescope Sensor Lookdown (Reticle)", m_camPreset == CameraPreset::TelescopeLookdown)) {
        m_camPreset = CameraPreset::TelescopeLookdown;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Nadir View (Earth Centered)", m_camPreset == CameraPreset::NadirEarth)) {
        m_camPreset = CameraPreset::NadirEarth;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Solar Array Wingtip View", m_camPreset == CameraPreset::SolarWing)) {
        m_camPreset = CameraPreset::SolarWing;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Ground Station POV (Look Up into Sky!)", m_camPreset == CameraPreset::GroundStationPOV)) {
        m_camPreset = CameraPreset::GroundStationPOV;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Orbital Plane Normal View", m_camPreset == CameraPreset::OrbitalPlane)) {
        m_camPreset = CameraPreset::OrbitalPlane;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");
    if (ImGui::Selectable("  Sun-Earth Ecliptic Line", m_camPreset == CameraPreset::SunEarthEcliptic)) {
        m_camPreset = CameraPreset::SunEarthEcliptic;
    }
    explain("Select this camera perspective. Right-drag switches to unrestricted free flight while retaining the current viewpoint.");

    ImGui::Separator();
    ImGui::EndDisabled();
    ImGui::TextColored(ImVec4(0.22f, 0.74f, 0.97f, 1.0f), "SIMULATION TIME & ORBIT:");
    explain("Adjust simulated time, circular orbit altitude, and orbital inclination.");
    ImGui::BeginDisabled(m_realScale||m_replayActive);
    sliderFloatExact("Time Warp", &m_timeWarp, 1.0f, 50.0f, "%.0fx", "%.0f");
    ImGui::EndDisabled();
    explain("Simulated seconds advanced per real second. Affects the orbit, attitude dynamics and Earth rotation.");
    ImGui::BeginDisabled(m_replayActive);
    sliderFloatExact("Spin Warp", &m_earthSpinWarp, 0.0f, 30.0f, "%.0fx", "%.0f");
    ImGui::EndDisabled();
    explain("Additional multiplier for Earth rotation only. Zero stops automatic Earth rotation.");

    ImGui::BeginDisabled(m_replayActive||m_numericalOrbit);
    float alt = (float)m_orbitAltitudeKm;
    if (sliderFloatExact("Altitude", &alt, 400.0f, 1500.0f, "%.0f km", "%.0f")) {
        m_orbitAltitudeKm = alt;
    }
    explain("Circular orbit height above the reference Earth surface, in kilometers.");

    float inc = (float)m_orbitInclinationDeg;
    if (sliderFloatExact("Inclination", &inc, 0.0f, 98.0f, "%.1f°", "%.1f")) {
        m_orbitInclinationDeg = inc;
    }
    explain("Tilt of the orbital plane relative to the equator. Zero is equatorial; 90 degrees is polar.");
    ImGui::EndDisabled();

    ImGui::Separator();
    if (ImGui::Button("Restore edge layout")) {
        detachedPanels().fill(false);
        savePanelLayout();
    }
    explain("Reattach all four panels to the left and right edges. Drag a title bar to keep that panel at a custom position.");
    ImGui::Text("Ground Station Target:");
    explain("Choose a ground target from the location photo cards below. The selection updates tracking and the ground camera.");
    for (size_t i = 0; i < m_stations.size(); ++i) {
        bool isSelected = (m_selectedStationIdx == (int)i);
        ImGui::PushID((int)i);
        ImVec2 row = ImGui::GetCursorScreenPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable("##station", isSelected, 0, ImVec2(0, isSelected?132.0f:110.0f))) {
            m_selectedStationIdx = (int)i;
            m_targetName = m_stations[i].name;
            m_targetLatDeg = m_stations[i].latDeg;
            m_targetLonDeg = m_stations[i].lonDeg;
            m_pointingSettledSec=0;
        }
        explain("Select this location as the tracking target. The photograph shows the location; coordinates determine the simulated line of sight.");
        auto* draw = ImGui::GetWindowDrawList();
        if (m_stations[i].texture) {
            float imageAspect = m_stations[i].imageAspect;
            float cardAspect = (rowWidth - 8) / 76;
            float u = std::min(1.0f, cardAspect / imageAspect) * 0.5f;
            float v = std::min(1.0f, imageAspect / cardAspect) * 0.5f;
            draw->AddImage((ImTextureID)(intptr_t)m_stations[i].texture, ImVec2(row.x+4,row.y+4), ImVec2(row.x+rowWidth-4,row.y+80), ImVec2(0.5f-u, 0.5f-v), ImVec2(0.5f+u, 0.5f+v));
        }
        else
            draw->AddText(ImVec2(row.x+8,row.y+30), IM_COL32(150,160,180,255), "Location image unavailable");
        draw->AddText(ImVec2(row.x+4,row.y+87), IM_COL32(235,240,250,255), m_stations[i].name.c_str());
        if(isSelected && m_repositionEtaSec>0) {
            const int eta=int(std::ceil(std::min(m_repositionEtaSec,359999.0)));
            char label[80];snprintf(label,sizeof(label),"Repositioning... ETA: %02d:%02d:%02d",eta/3600,(eta/60)%60,eta%60);
            draw->AddText(ImVec2(row.x+4,row.y+108),IM_COL32(255,220,40,255),label);
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))ImGui::SetTooltip("Estimated attitude slew and settling time, recalculated as the target moves. Does not predict when Earth stops blocking the target. Pausing freezes progress.");
        }
        ImGui::PopID();
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

void ADCSView::draw2DWorldMap() {
    beginEdgePanel("2D Ground Track (ECEF)", 3);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float mapWidth = std::max(40.0f, ImGui::GetContentRegionAvail().x);
    ImVec2 mapSize(mapWidth, mapWidth * 0.5f);

    drawList->AddRectFilled(p0, ImVec2(p0.x + mapSize.x, p0.y + mapSize.y), IM_COL32(6, 12, 24, 255));
    drawList->AddRect(p0, ImVec2(p0.x + mapSize.x, p0.y + mapSize.y), IM_COL32(56, 189, 248, 100));

    if (m_groundTrack2D.size() > 1) {
        for (size_t i = 0; i < m_groundTrack2D.size() - 1; ++i) {
            auto& pt1 = m_groundTrack2D[i];
            auto& pt2 = m_groundTrack2D[i+1];
            if (std::abs(pt1.lon - pt2.lon) < 160.0f) {
                float x1 = p0.x + ((pt1.lon + 180.0f) / 360.0f) * mapSize.x;
                float y1 = p0.y + ((90.0f - pt1.lat) / 180.0f) * mapSize.y;
                float x2 = p0.x + ((pt2.lon + 180.0f) / 360.0f) * mapSize.x;
                float y2 = p0.y + ((90.0f - pt2.lat) / 180.0f) * mapSize.y;
                drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(56, 189, 248, 120), 1.5f);
            }
        }
    }

    float tgtX = p0.x + (float)((m_targetLonDeg + 180.0) / 360.0) * mapSize.x;
    float tgtY = p0.y + (float)((90.0 - m_targetLatDeg) / 180.0) * mapSize.y;
    drawList->AddCircleFilled(ImVec2(tgtX, tgtY), 4.5f, IM_COL32(239, 68, 68, 255));
    drawList->AddCircle(ImVec2(tgtX, tgtY), 8.0f, IM_COL32(239, 68, 68, 180), 0, 1.5f);

    if (!m_groundTrack2D.empty()) {
        auto& sat = m_groundTrack2D.back();
        float satX = p0.x + ((sat.lon + 180.0f) / 360.0f) * mapSize.x;
        float satY = p0.y + ((90.0f - sat.lat) / 180.0f) * mapSize.y;
        drawList->AddCircleFilled(ImVec2(satX, satY), 5.0f, IM_COL32(56, 189, 248, 255));

        ImU32 lineColor = m_isTargetVisible ? IM_COL32(16, 185, 129, 150) : IM_COL32(239, 68, 68, 150);
        drawList->AddLine(ImVec2(satX, satY), ImVec2(tgtX, tgtY), lineColor, 1.2f);
    }

    ImGui::Dummy(mapSize);
    explain("Earth-fixed ground track: cyan is the satellite and its recent path; red is the selected target. The connecting line indicates visibility. Longitude runs left to right, latitude top to bottom.");
    ImGui::End();
}

void ADCSView::injectTumble() {
    m_satState.omega = aero::math::Vec3(
        ((double)rand() / RAND_MAX - 0.5) * 0.25,
        ((double)rand() / RAND_MAX - 0.5) * 0.25,
        ((double)rand() / RAND_MAX - 0.5) * 0.25
    );
}

void ADCSView::resetPointing() {
    m_satState.omega = aero::math::Vec3(0, 0, 0);
    m_satState.q = aero::adcs::Quaternion::identity();
}

void ADCSView::cleanup() {
    cleanupViews();
    for (auto& station : m_stations) {
        if (station.texture) glDeleteTextures(1, &station.texture);
        station.texture = 0;
    }
    if (m_starVao) glDeleteVertexArrays(1, &m_starVao);
    if (m_starVbo) glDeleteBuffers(1, &m_starVbo);
    m_starVao = m_starVbo = 0;
    updateAlarm(false);
    m_entryShader.cleanup();
    if(m_entryVao) glDeleteVertexArrays(1,&m_entryVao);
    if(m_entryVbo) glDeleteBuffers(1,&m_entryVbo);
    m_entryVao=m_entryVbo=0;
    m_ribbonShader.cleanup();
    if(m_historyVao) glDeleteVertexArrays(1,&m_historyVao);
    if(m_historyVbo) glDeleteBuffers(1,&m_historyVbo);
    m_historyVao=m_historyVbo=0;
    m_atmosphereShader.cleanup();
    m_landsatModel.cleanup();
    for(auto& model:m_additionalModels)model.cleanup();
    for (auto& mesh : m_earthMeshes) mesh.cleanup();
    m_busMesh.cleanup();
    m_solarPanelMesh.cleanup();
    m_lensMesh.cleanup();
    m_dishMesh.cleanup();
    m_beaconMesh.cleanup();
    if (m_earthTexId) glDeleteTextures(1, &m_earthTexId);
    if (m_lineVao) glDeleteVertexArrays(1, &m_lineVao);
    if (m_lineVbo) glDeleteBuffers(1, &m_lineVbo);
    m_earthTexId = m_lineVao = m_lineVbo = 0;
    m_shader3D.cleanup();
    m_lineShader.cleanup();
}

} // namespace gui
