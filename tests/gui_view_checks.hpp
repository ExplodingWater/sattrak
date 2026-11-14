#pragma once
#include "gui/adcs_view.hpp"
#include "gui/solar_environment.hpp"
#include <iostream>
#include "imgui_internal.h"

namespace gui {
// Invoked by --smoke-test while a real OpenGL context is active.
struct ADCSViewChecks {
    static void prepareEnvironmentFrame(ADCSView& view,int frame) {
        view.m_isPaused=true;view.m_hudVisible=false;view.m_showAltimeters=false;
        view.m_showResearch=false;view.m_showTrail=false;view.setMarkerOpacity(0);
        view.m_skyEpochUtc=946728000;view.m_simTimeSec=0;
        const auto sun=solarEnvironment(view.m_skyEpochUtc).direction;
        const auto tangent=sun.cross({0,1,0}).normalized();
        view.m_camPreset=CameraPreset::FreeFlycam;
        view.m_cameraFov=45;
        view.m_flycamPos=frame<4?sun*30:frame<8?-sun*30:frame<12?-sun*30+tangent*13:sun*20+tangent*20;
        view.m_flyForward=frame<4?sun:frame<12?sun:-view.m_flycamPos.normalized();
        view.m_flyUp={0,1,0};view.computeLineOfSight();
        view.m_fullscreenMap=frame==18;
        if(frame>=16) {
            view.m_flycamPos=sun*11.9;
            view.m_flyForward=sun;
            view.m_hudVisible=true;
            view.m_skySettings.lines=true;
            view.m_skySettings.labels=true;
            view.m_showResearch=true;
            view.m_researchUnlocked=true;
        }
    }
    static bool checkEarthRendering(ADCSView& view,int frame) {
        if(frame!=18 && frame!=19)return true;
        unsigned char pixel[4]{};
        glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        const bool underground=view.cameraAltitudeKm()<0 && pixel[0]>=3 && pixel[0]<=5
            && pixel[1]>=4 && pixel[1]<=6 && pixel[2]>=6 && pixel[2]<=8;
        const auto* map=ImGui::FindWindowByName(frame==18?"Earth-fixed map##fullscreen":"2D Ground Track (ECEF)");
        if(map && view.m_earthTexId && underground) {
            for(const auto& cmd:map->DrawList->CmdBuffer)
                if(cmd.ElemCount && cmd.GetTexID()==(ImTextureID)(intptr_t)view.m_earthTexId)return true;
        }
        std::cerr<<"Underground framebuffer or ECEF map texture check failed\n";
        return false;
    }
    static void prepareCatalogFrame(ADCSView& view,int frame) {
        view.m_showResearch=true;view.m_researchUnlocked=true;view.m_showCatalogStations=true;view.m_includeOfflineStations=true;
        view.m_camPreset=CameraPreset::FreeOrbitArcball;view.m_arcballDist=42;
        if(frame==0){view.m_selectedResearch=25544;view.m_researchCatalog.epoch=libsgp4::DateTime::Now();}
        if(frame==8&&!view.m_researchCatalog.stations.empty())view.m_selectedCatalogStation=view.m_researchCatalog.stations.front().id;
        if(frame==16){view.m_selectedResearch=0;view.m_selectedCatalogStation=0;view.m_openSettings=true;}
    }
    static void showStartupChoice(ADCSView& view){view.m_askInternet=true;}
    static bool checkDataFallback(ADCSView& view) {
        const auto count=view.m_researchCatalog.satellites.size();
        std::promise<std::string> result;view.m_catalogDownload=result.get_future();result.set_value("ERROR: simulated offline service");
        view.drawInternetChoice();
        bool okay=view.m_researchCatalog.satellites.size()==count&&view.m_dataStatus.find("using saved prediction")!=std::string::npos;
        const bool shown=view.m_showTrail,replay=view.m_replayActive;
        view.m_showTrail=true;view.m_replayActive=false;
        okay &= !view.hasActivePathRecording();
        view.m_replayActive=true;okay &= !view.hasActivePathRecording();
        view.m_showTrail=shown;view.m_replayActive=replay;
        ADCSView recording;
        recording.advancePhysics(.01);recording.startPathRecording();
        okay &= recording.hasActivePathRecording();
        recording.update(1.2f);recording.m_recording.stop();
        OrbitReplay saved;std::string error;
        okay &= !recording.hasActivePathRecording()&&saved.load(recording.m_recording.path(),error)&&saved.samples.size()>=2;
        recording.m_mass=100;recording.m_inertiaValues[0]=10;recording.m_wheelTorque=20;recording.m_wheelMomentum=30;
        recording.m_physicsStep=.05f;recording.m_physicsAccumulator=.04;
        recording.m_gyroNoise=.01f;recording.m_biasDrift=.001f;recording.m_vectorNoise=.1f;
        recording.m_biasValues[0]=.1f;recording.m_biasWalk={1,2,3};recording.m_satState.hWheel={1,2,3};
        recording.m_gravityGradient=recording.m_sensorNoise=recording.m_customSlew=recording.m_numericalOrbit=true;
        recording.m_maxSlew=5;recording.m_slewAcceleration=1;recording.m_elevationMask=30;
        recording.m_orbitForces={true,true,100,100,5,50};
        recording.resetExpertSettings();
        okay &= recording.m_mass==6&&recording.m_inertiaValues[0]==.04f&&recording.m_wheelTorque==.005f&&recording.m_wheelMomentum==.05f;
        okay &= recording.m_physicsStep==.01f&&recording.m_physicsAccumulator==0&&recording.m_gyroNoise==.0005f&&recording.m_biasDrift==.00005f&&recording.m_vectorNoise==.001f;
        okay &= recording.m_biasValues[0]==.012f&&recording.m_biasWalk.norm()==0&&recording.m_satState.hWheel.norm()==0;
        okay &= !recording.m_gravityGradient&&!recording.m_sensorNoise&&!recording.m_customSlew&&!recording.m_numericalOrbit;
        okay &= recording.m_maxSlew==.25f&&recording.m_slewAcceleration==.02f&&recording.m_elevationMask==0;
        okay &= !recording.m_orbitForces.j2&&!recording.m_orbitForces.drag&&recording.m_orbitForces.mass==6&&recording.m_orbitForces.area==1&&recording.m_orbitForces.cd==2.2&&recording.m_orbitForces.densityScale==1;
        okay &= std::abs(recording.m_dynamics.computeAngularAcceleration({},{},{},{.005,0,0}).x-.125)<1e-10;
        recording.m_camPreset=CameraPreset::FreeFlycam;recording.m_flyForward={0,0,-1};recording.m_flyUp={0,1,0};
        recording.lookWithArrows(1,0,.1);okay &= recording.m_flyForward.x>0;
        recording.m_flyForward={0,0,-1};recording.m_flyUp={0,1,0};recording.lookWithArrows(0,1,.1);okay &= recording.m_flyForward.y>0;
        recording.m_flyForward={0,0,-1};recording.m_flyUp={0,1,0};recording.lookWithArrows(1,0,1);auto once=recording.m_flyForward;
        recording.m_flyForward={0,0,-1};recording.m_flyUp={0,1,0};for(int i=0;i<60;++i)recording.lookWithArrows(1,0,1.0/60);
        okay &= (recording.m_flyForward-once).norm()<1e-10;
        if(!okay)std::cerr<<"Recording/fallback integration check failed: "<<error<<'\n';
        return okay;
    }
    static void prepareOrbitFrame(ADCSView& view,int frame) {
        if(frame==0){view.loadReplay("assets/orbits/demo.oem");view.loadReplay("assets/orbits/demo-comparison.oem",true);view.seekReplay(1200);view.m_replayWindow=true;}
        if(frame==8)view.seekReplay(2400);
        if(frame==12){view.stopReplay();view.m_numericalOrbit=true;view.m_orbitForces.j2=true;view.m_orbitForces.drag=true;view.m_sensorNoise=true;view.m_gravityGradient=true;view.initializeNumericalOrbit();for(int i=0;i<200;++i)view.advancePhysics(.005);}
    }
    static bool checkOrbitFrame(ADCSView& view,int frame) {
        if(frame!=1 && frame!=9)return true;
        const double seconds=frame==1?1200:2400;
        if(!view.m_replayActive || view.m_replay.samples.size()!=601 || view.m_reference.samples.size()!=601) {
            std::cerr<<"Trajectory fixtures did not load: "<<view.m_replayMessage<<'\n';return false;
        }
        bool okay=true;
        okay &= std::abs(view.m_replaySeconds-seconds)<1e-6 && view.m_trail.size()>10;
        okay &= std::abs(view.m_currentOrbit.utc-view.m_replay.samples.front().utc-seconds)<1e-6;
        if(frame==1) {
            const auto oldPosition=view.m_satPosECI;const auto trailSize=view.m_trail.size();
            const auto file=std::filesystem::path("invalid-replay-ui-test.csv");
            {std::ofstream out(file);out<<"# frame=EME2000\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n2026-09-13T00:00:00Z,7000,,0,0,7,0\n";}
            okay &= !view.loadReplay(file) && view.m_replayMessage.find("Line 3:")!=std::string::npos;
            okay &= !view.loadReplay(file,true) && view.m_reference.samples.size()==601;
            okay &= view.m_replayActive && view.m_isPaused && view.m_replaySeconds==seconds && view.m_trail.size()==trailSize;
            okay &= (view.m_satPosECI-oldPosition).norm()==0;
            std::filesystem::remove(file);
        }
        if(!okay)std::cerr<<"Trajectory replay/seek/error preservation check failed: "<<view.m_replayMessage<<'\n';
        return okay;
    }
    // Deterministic cameras for clean captures of the actual application renderer.
    static void prepareBrandingFrame(ADCSView& view,int frame) {
        view.m_isPaused=true;view.m_hudVisible=false;view.m_showAltimeters=false;
        view.m_settingsVisible=false;view.m_openSettings=false;view.m_replayWindow=false;
        view.m_showResearch=false;view.m_showTrail=false;view.m_multiView=false;
        view.m_discoveryUntil=0;view.m_entryPreview=false;view.m_alarmMuted=true;
        view.setMarkerOpacity(0);view.m_orbitAltitudeKm=650;
        view.m_cameraFov=48;view.m_earthSpinAngleRad=.4;
        if(frame%4!=0)return;
        view.setRealScale(false);
        if(frame<4) {
            view.selectModel(1);
            view.m_camPreset=CameraPreset::FreeOrbitArcball;
            view.m_arcballCenter={};view.m_arcballDist=34;view.m_arcballYaw=.9f;view.m_arcballPitch=.25f;
        } else {
            const int model=frame<8?1:frame<12?2:frame<16?5:3;
            view.selectModel(model);view.m_camPreset=CameraPreset::SatelliteOrbit;
            view.setRealScale(true);
            view.m_satOrbitDistance=7*view.modelFramingScale();
            view.m_satOrbitYaw=.45f;view.m_satOrbitPitch=.1f;
        }
    }
    static void prepareMultiFrame(ADCSView& view,int frame) {
        if(frame==0){view.enableMultiView(true);view.m_viewCount=8;}
        if(frame==8)view.m_separateViews=true;
        if(frame==16){view.m_separateViews=false;view.enableMultiView(false);}
    }
    static void prepareModelFrame(ADCSView& view,int frame) {
        if(frame%4!=0)return;
        view.selectModel(2+std::min(frame/4,3));
        view.setRealScale(frame>=12);
        view.m_camPreset=CameraPreset::SatelliteOrbit;
        // Exercise both sides of the ISS bounding-sphere/near-plane transition.
        view.m_satOrbitDistance=frame>=16?65.0f:frame>=12?73.0f:8*view.modelFramingScale();
        view.m_satOrbitYaw=0.7f;
        view.m_satOrbitPitch=0.35f;
        view.m_showResearch=false;view.m_showTrail=false;view.m_discoveryUntil=0;
        view.m_alarmMuted=true;
    }
    static void prepareIssDepthFrame(ADCSView& view,int frame) {
        if(frame%4!=0)return;
        prepareModelFrame(view,16);
        constexpr float distances[]={73,65,50,25,10};
        view.m_satOrbitDistance=distances[std::min(frame/4,4)];
        view.m_isPaused=true; // Compare distance alone, without changing attitude or lighting.
    }
    static void prepareFrame(ADCSView& view, int frame) {
        if(frame==0) {
            view.m_showTrail=true;
            // Deterministic long-history fixture for rendering, without accelerating attitude integration.
            view.m_trail.clear();
            double radius=12+(650.0*12/6378.137), inc=view.m_orbitInclinationDeg*3.141592653589793/180;
            for(int i=0;i<=600;++i) {
                double a=(i-600)*.008;
                view.m_trail.push_back({{radius*std::cos(a),radius*std::sin(a)*std::sin(inc),radius*std::sin(a)*std::cos(inc)},i==0});
            }
            view.m_researchUnlocked=true;view.m_showResearch=true;
            view.m_discoveryUntil=ImGui::GetTime()+6;
        }
        if (frame == 8) view.selectModel(1);
        if (frame == 12) {
            view.selectModel(1);
            view.setRealScale(true);
            view.m_satelliteSpeedFactor = 1;
            view.m_orbitAltitudeKm=23;
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
        if(!view.m_realScale || view.m_timeWarp!=1 || view.m_earthSpinWarp!=1) {
            std::cerr<<"Startup must use 1:1 scale and real time\n";return false;
        }
        {
            const auto saved=view.m_satState.q;
            for(const auto& axis:std::array<aero::math::Vec3,4>{{{1,0,0},{0,1,0},{0,0,1},{1,2,3}}}) {
                view.m_satState.q=aero::adcs::Quaternion::fromRotationVector(axis.normalized()*1.2);
                view.computeLineOfSight();
                const auto expected=view.m_satState.q.rotateInverse({0,0,1});
                const auto model=view.spacecraftRotation();
                const aero::math::Vec3 renderedAxis{model.m[8],model.m[9],model.m[10]};
                if((renderedAxis-expected).norm()>1e-6){std::cerr<<"Rendered payload disagrees with boresight\n";return false;}
                if((view.m_boresightECI-expected).norm()>1e-10)return false;
                const auto delta=(view.m_targetPosECI-view.m_muzzlePosECI).normalized();
                const auto error=std::acos(std::clamp(expected.dot(delta),-1.0,1.0))*180/3.141592653589793;
                if(std::abs(error-view.m_slewErrorDeg)>1e-8)return false;
            }
            view.m_satState.q=saved;view.computeLineOfSight();
            view.setRealScale(false);
            if(view.m_timeWarp!=5 || view.spacecraftScale()!=1)return false;
            view.setRealScale(true);
        }
        const int expectedTriangles[]={21129,117120,93121,247547};
        for(int i=0;i<4;++i) {
            int count=0;
            for(const auto& part:view.m_additionalModels[i].parts)count+=part.mesh.indexCount/3;
            if(count!=expectedTriangles[i]){std::cerr<<"Incomplete spacecraft model "<<i<<"\n";return false;}
        }
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
        {
            if(view.m_researchCatalog.satellites.size()<200) {
                std::cerr<<view.m_researchCatalog.status<<"\n";return false;
            }
            aero::math::Vec3 p0,p1;
            if(!view.m_researchCatalog.position(0,0,p0) || !view.m_researchCatalog.position(0,60,p1) || (p1-p0).norm()<1) return false;
            libsgp4::SGP4 reference(libsgp4::Tle("VANGUARD 1",
                "1 00005U 58002B   00179.78495062  .00000023  00000-0  28098-4 0  4753",
                "2 00005  34.2682 348.7242 1859667 331.7664  19.3264 10.82419157413667"));
            auto r=reference.FindPosition(0.0).Position();
            if(std::abs(r.x-7022.46529266)>.001 || std::abs(r.y+1400.08296755)>.001 || std::abs(r.z-.03995155)>.001) {
                std::cerr<<"SGP4 reference position mismatch\n";return false;
            }
            ADCSView secret;
            constexpr int code[]={GLFW_KEY_UP,GLFW_KEY_UP,GLFW_KEY_DOWN,GLFW_KEY_DOWN,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_LEFT,GLFW_KEY_RIGHT,GLFW_KEY_B,GLFW_KEY_A,GLFW_KEY_ENTER};
            for(int key:code)secret.secretKey(key,GLFW_PRESS,true);
            if(secret.m_researchUnlocked)return false;
            secret.secretKey(GLFW_KEY_UP,GLFW_PRESS,false);
            secret.secretKey(GLFW_KEY_UP,GLFW_REPEAT,false);
            if(secret.m_secretProgress!=1)return false;
            secret.secretKey(GLFW_KEY_X,GLFW_PRESS,false);
            if(secret.m_secretProgress!=0)return false;
            for(int i=0;i<10;++i)secret.secretKey(code[i],GLFW_PRESS,false);
            if(secret.m_researchUnlocked)return false;
            secret.secretKey(GLFW_KEY_ENTER,GLFW_PRESS,false);
            if(!secret.m_researchUnlocked || std::abs(secret.m_discoveryUntil-ImGui::GetTime()-6)>1e-6)return false;
            if(secret.m_showTrail || secret.m_showResearch)return false;
            ADCSView history;
            history.m_satPosECI={13.2,0,0};history.m_previousTrailPosition=history.m_satPosECI;
            history.m_trail={{history.m_satPosECI,true}};
            auto first=history.m_trail.front().position;
            for(int i=1;i<=70000;++i){history.m_simTimeSec=i;history.m_satPosECI={13.2,double(i)*1e-7,0};history.recordTrail();}
            if(history.m_trail.size()>65536 || (history.m_trail.front().position-first).norm()!=0 || history.m_showTrail)return false;
            history.m_satPosECI={20,0,0};history.recordTrail();
            if(!history.m_trail.back().start)return false;
            history.setRealScale(true);
            if(history.m_trail.size()!=1 || history.m_trailInterval!=1 || history.m_nextTrailTime!=history.m_simTimeSec+1)return false;
            history.m_trail.push_back({{25,0,0},false});
            history.setRealScale(false);
            if(history.m_trail.size()!=1)return false;
        }
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
        const double expectedTravel=view.m_userSpeedKmS*(12.0/6378.137)*0.5;
        check(std::abs((view.m_flycamPos-view.m_cameraEye).norm()-expectedTravel) < 1e-8, "W advances at configured physical speed");
        view.m_flycamPos = {0,0,0};
        view.moveFlycam(1,1,0,false,0.5);
        check(std::abs(view.m_flycamPos.norm()-expectedTravel) < 1e-8, "diagonal speed is normalized");
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
