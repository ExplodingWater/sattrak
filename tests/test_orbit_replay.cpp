#include "gui/orbit_replay.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace gui;
using aero::math::Vec3;
void check(bool okay,const char* what){if(!okay)throw std::runtime_error(what);}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Fixture directory argument");auto folder=std::filesystem::path(argv[1]);
        OrbitReplay oem,csv;std::string error;
        check(oem.load(folder/"demo.oem",error),error.c_str());check(csv.load(folder/"demo.csv",error),error.c_str());
        check(oem.samples.size()==601&&csv.samples.size()==601,"Both formats parsed");
        double t=oem.samples.front().utc;OrbitSample out;
        check(oem.sample(t+5,out),"Interpolate between samples");
        const double radius=7028.137,n=std::sqrt(398600.4418/std::pow(radius,3)),a=n*5,inc=51.6*3.141592653589793/180;
        Vec3 exact{radius*std::cos(a),radius*std::sin(a)*std::cos(inc),radius*std::sin(a)*std::sin(inc)};
        check((out.position-exact).norm()<1e-5,"Hermite position agrees with independent analytic orbit to centimeters");
        OrbitSample csvState;check(csv.sample(t+5,csvState)&&(out.position-csvState.position).norm()<1e-9,"CSV/OEM same trajectory");
        check(!oem.sample(t-1,out)&&!oem.sample(t+6001,out),"No extrapolation");
        check(oem.sample(t+6000,out),"Exact final sample");
        check(std::abs(parseOrbitUtc("2000-01-01T12:00:00Z")-946728000)<1e-6,"Known UTC epoch");
        check(std::abs(std::remainder(earthSiderealAngle(946728000)-280.46061837*3.141592653589793/180,2*3.141592653589793))<1e-10,"GMST at J2000");
        bool rejected=false;try{parseOrbitUtc("2026-02-30T00:00:00Z");}catch(...){rejected=true;}check(rejected,"Reject invalid calendar day");
        auto invalid=folder.parent_path().parent_path()/"invalid-orbit-test.csv";
        // A test-specific file in CTest's build directory, never beside user imports.
        invalid="invalid-orbit-test.csv";
        {std::ofstream f(invalid);f<<"# frame=EME2000\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n2026-09-13T00:00:00,7000,0,0,0,7,0\n2026-09-13T00:00:00,7000,0,0,0,7,0\n";}
        check(!oem.load(invalid,error)&&oem.samples.size()==601,"Reject duplicate epochs transactionally");
        {std::ofstream f(invalid);f<<"# frame=TEME\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n";}
        check(!oem.load(invalid,error),"Reject unsupported frames");
        {std::ofstream f(invalid);f<<"# frame=ECEF\nutc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s\n2026-09-13T00:00:00,42164,0,0,0,0,0\n2026-09-13T00:01:00,42164,0,0,0,0,0\n";}
        OrbitReplay fixed;check(fixed.load(invalid,error),"Earth-fixed format");check(fixed.sample(t+30,out),"Fixed sample");auto inertial=fixed.inertial(out);
        check(std::abs(inertial.velocity.norm()-42164*7.292115e-5)<1e-9,"Earth-fixed velocity includes Earth rotation");
        OrbitSample state{0,{7000,0,0},{0,std::sqrt(398600.4418/7000),0}},initial=state;
        OrbitForces forces;const double period=2*3.141592653589793*std::sqrt(std::pow(7000,3)/398600.4418),step=period/6000;
        for(int i=0;i<6000;++i)state=integrateOrbit(state,step,forces);
        check((state.position-initial.position).norm()<1e-5,"RK4 returns circular orbit after one period");
        forces.j2=true;auto j2=orbitAcceleration(initial.position,initial.velocity,forces);forces.j2=false;auto gravity=orbitAcceleration(initial.position,initial.velocity,forces);
        check(j2.x<gravity.x&&std::abs(j2.y)<1e-12,"J2 increases inward equatorial acceleration");
        forces.drag=true;forces.area=10;Vec3 r{6578.137,0,0},v{0,7.8,0};auto drag=orbitAcceleration(r,v,forces);forces.drag=false;
        check((drag-orbitAcceleration(r,v,forces)).dot(v)<0,"Drag removes orbital energy");
        std::cout<<"Orbit import, UTC/frame conversion, replay interpolation, RK4 and drag/J2 checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
