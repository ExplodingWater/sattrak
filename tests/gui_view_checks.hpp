#pragma once
#include "gui/adcs_view.hpp"
#include <iostream>
#include "imgui_internal.h"

namespace gui {
// Invoked by --smoke-test while a real OpenGL context is active.
struct ADCSViewChecks {
    static void prepareFrame(ADCSView& view, int frame) {
        if (frame == 8) view.selectModel(1);
        if (frame == 12) {
            view.selectModel(1);
            view.setRealScale(true);
            view.m_satelliteSpeedFactor = 1;
            view.m_orbitAltitudeKm=75;
            view.m_entryPreview=true;
            view.m_descentKmS=0;
            view.m_alarmMuted=true;
            view.updateReentry();
            view.m_camPreset = CameraPreset::SatelliteOrbit;
        }
        if (frame == 16) {
            view.m_entryPreview=false;
            view.m_orbitAltitudeKm=650;
            view.updateReentry();
            view.m_camPreset = CameraPreset::FreeOrbitArcball;
            view.m_openSettings = true;
        }
    }
    static bool checkRouting(ADCSView& view, int frame) {
        if(frame==15 && (view.m_heatFlux<=100000 || view.m_alarmPlaying)) {
            std::cerr<<"Muted warning must retain heating and silence audio\n";return false;
        }
        if(frame==19 && (view.m_heatFlux>100000 || view.m_alarmPlaying)) return false;
        if (frame != 2 && frame != 3 && frame != 18) return true;
        bool expectedScene = frame == 3;
        auto before = view.m_arcballDist;
        bool accepts = view.sceneAcceptsInput();
        view.onScroll(1);
        bool moved = view.m_arcballDist != before;
        view.m_arcballDist = before;
        bool passed = accepts == expectedScene && moved == expectedScene;
        if (!passed) std::cerr << "Scroll routing failed at frame " << frame << '\n';
        return passed;
    }
    static bool run(ADCSView& view) {
        if (view.m_landsatModel.parts.size() != 33) {
            std::cerr << "Landsat 8 model failed to load all material groups\n";
            return false;
        }
        int triangles = 0;
        for (const auto& part : view.m_landsatModel.parts) triangles += part.mesh.indexCount / 3;
        if (triangles != 85364) return false;
        view.update(.02f);
        view.updateReentry();
        if(view.m_heatFlux>100000) {std::cerr<<"Unexpected orbital heating\n";return false;}
        view.m_orbitAltitudeKm=80; view.updateReentry();
        if(view.m_heatFlux<=100000 || view.m_projectedArea<=0) {std::cerr<<"Missing entry heating\n";return false;}
        double hot=view.m_heatFlux;
        view.m_orbitAltitudeKm=100; view.updateReentry();
        if(view.m_heatFlux>=hot) return false;
        view.m_orbitAltitudeKm=650; view.updateReentry();
        const auto oldState = view.m_satState.q;
        const double oldTime = view.m_simTimeSec;
        view.selectModel(1);
        if (view.m_selectedModel != 1 || view.m_satState.q.w != oldState.w || view.m_simTimeSec != oldTime) return false;
        view.selectModel(0);
        const auto originalPreset = view.m_camPreset;
        const auto originalPos = view.m_flycamPos;
        const auto originalForward = view.m_flyForward;
        const auto originalUp = view.m_flyUp;
        bool passed = true;
        auto check = [&](bool condition, const char* message) {
            if (!condition) { std::cerr << "Camera check failed: " << message << '\n'; passed = false; }
        };
        view.m_cameraEye = {3, 4, 5};
        view.m_cameraForward = {0, 0, -1};
        view.m_cameraUp = {0, 1, 0};
        view.m_camPreset = CameraPreset::ChaseCam;
        view.enterFlycam();
        check((view.m_flycamPos - view.m_cameraEye).norm() < 1e-9, "preset transition preserves position");
        view.rotateFlycam(100, 0);
        check(view.m_flyForward[0] > 0, "right drag turns right");
        view.m_flyForward = {0, 0, -1}; view.m_flyUp = {0, 1, 0};
        view.rotateFlycam(0, 100);
        check(view.m_flyForward[1] < 0, "down drag looks down");
        view.m_flyForward = {0, 0, -1}; view.m_flyUp = {0, 1, 0};
        view.rotateFlycam(0, -3.141592653589793 / 0.0035);
        check(view.m_flyForward[2] > 0.999, "pitch passes through pole without clamping");
        for (int i = 0; i < 10000; ++i) view.rotateFlycam(11, -7);
        check(std::abs(view.m_flyForward.dot(view.m_flyUp)) < 1e-9, "camera basis remains orthogonal");
        check(std::abs(view.m_flyForward.norm() - 1) < 1e-9, "camera direction remains normalized");
        view.m_camPreset = CameraPreset::ChaseCam;
        view.moveFlycam(1, 0, 0, false, 0.5);
        check(view.m_camPreset == CameraPreset::FreeFlycam, "WASD enters flight from a preset");
        check(std::abs((view.m_flycamPos-view.m_cameraEye).norm()-7.0) < 1e-8, "W advances at configured speed");
        view.m_flycamPos = {0,0,0};
        view.moveFlycam(1,1,0,false,0.5);
        check(std::abs(view.m_flycamPos.norm()-7.0) < 1e-8, "diagonal speed is normalized");
        {
            ADCSView normal, faster;
            normal.setRealScale(true); faster.setRealScale(true);
            faster.m_satelliteSpeedFactor = 2;
            normal.update(1.0f); faster.update(1.0f);
            check(std::abs(normal.m_simTimeSec-1) < 0.011, "real time ignores default 5x warp");
            check(std::abs(faster.m_trueAnomalyRad-2*normal.m_trueAnomalyRad) < 1e-10, "satellite speed changes orbit independently");
            check(std::abs(faster.m_simTimeSec-normal.m_simTimeSec) < 1e-10, "speed override preserves clock");
            normal.m_camPreset = CameraPreset::FreeFlycam;
            auto before = normal.m_flycamPos;
            normal.moveFlycam(1,0,0,true,1);
            check(std::abs((normal.m_flycamPos-before).norm()/normal.spacecraftScale()-22500) < 0.01, "real-scale movement and Shift use physical units");
            normal.setRealScale(false);
            check(normal.m_timeWarp == 5, "leaving 1:1 restores time warp");
        }
        {
            ADCSView orbit;
            orbit.m_satOrbitPitch = 0;
            orbit.rotateSatelliteOrbit(0, float(3.141592653589793 / orbit.m_sensitivity));
            check(orbit.satelliteOrbitOffset().z < 0, "satellite orbit crosses the pole");
            check(std::abs(orbit.satelliteOrbitOffset().norm()-8.0) < 1e-5, "orbit radius is stable through rotation");
            orbit.update(0.1f);
            orbit.m_camPreset = CameraPreset::SatelliteOrbit;
            // Offset math also determines the exact look-at target in render3D.
            auto eye = orbit.m_satPosECI + orbit.satelliteOrbitOffset();
            check(std::abs((eye-orbit.m_satPosECI).norm()-8.0) < 1e-5, "orbit offset follows the moving satellite");
            orbit.setRealScale(true);
            orbit.update(0.1f);
            // Offset math also determines the exact look-at target in render3D.
            check(std::abs(orbit.satelliteOrbitOffset().norm()/orbit.spacecraftScale()-8.0) < 0.001, "real-scale orbit distance is eight meters");
        }
        for (const auto& station : view.m_stations) check(station.texture != 0, "location image loaded");
        view.m_camPreset = originalPreset;
        view.m_flycamPos = originalPos;
        view.m_flyForward = originalForward;
        view.m_flyUp = originalUp;
        return passed;
    }
};
}
