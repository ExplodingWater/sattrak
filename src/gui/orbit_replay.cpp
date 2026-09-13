#include "gui/orbit_replay.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace gui {
using aero::math::Vec3;
namespace {
constexpr double pi=3.141592653589793,mu=398600.4418,re=6378.137,omega=7.292115e-5;
std::string trim(std::string text) {
    auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
OrbitFrame readFrame(const std::string& text) {
    if(text=="EME2000" || text=="J2000")return OrbitFrame::EME2000;
    if(text=="ECEF" || text=="ITRF" || text=="ITRF93" || text=="ITRF97" || text=="ITRF2000" || text=="ITRF2008" || text=="ITRF2014" || text=="ITRF2020")return OrbitFrame::ECEF;
    throw std::runtime_error("Unsupported reference frame: "+text+". Use EME2000 or Earth-fixed ITRF/ECEF.");
}
Vec3 rotateZ(Vec3 p,double a) {const double c=std::cos(a),s=std::sin(a);return {c*p.x-s*p.y,s*p.x+c*p.y,p.z};}
}
double parseOrbitUtc(const std::string& value) {
    using namespace std::chrono;
    std::string t=value;
    if(!t.empty() && t.back()=='Z')t.pop_back();
    if(t.size()<19 || t[4]!='-' || t[7]!='-' || t[10]!='T' || t[13]!=':' || t[16]!=':')throw std::runtime_error("UTC must be YYYY-MM-DDTHH:MM:SS[.fraction][Z].");
    for(size_t i=0;i<t.size();++i)if(i!=4&&i!=7&&i!=10&&i!=13&&i!=16&&!(i==19&&t[i]=='.')&&(t[i]<'0'||t[i]>'9'))throw std::runtime_error("Invalid UTC timestamp.");
    if(t.size()>19 && (t[19]!='.'||t.size()==20))throw std::runtime_error("Invalid UTC fraction.");
    int y=std::stoi(t.substr(0,4)),m=std::stoi(t.substr(5,2)),d=std::stoi(t.substr(8,2));
    int h=std::stoi(t.substr(11,2)),minute=std::stoi(t.substr(14,2));double sec=std::stod(t.substr(17));
    year_month_day date{year{y},month{unsigned(m)},day{unsigned(d)}};
    if(!date.ok()||y<1957||y>2200||h>23||minute>59||sec>=60)throw std::runtime_error("Invalid UTC date/time (leap-second epochs are not supported).");
    return double(sys_days{date}.time_since_epoch().count())*86400+h*3600+minute*60+sec;
}
std::string formatOrbitUtc(double utc) {
    using namespace std::chrono;
    auto instant=sys_seconds{seconds{static_cast<long long>(std::floor(utc))}};
    auto days=floor<std::chrono::days>(instant);year_month_day date{days};hh_mm_ss time{instant-days};
    std::ostringstream out;out<<std::setfill('0')<<std::setw(4)<<int(date.year())<<'-'<<std::setw(2)<<unsigned(date.month())<<'-'<<std::setw(2)<<unsigned(date.day())<<'T'<<std::setw(2)<<time.hours().count()<<':'<<std::setw(2)<<time.minutes().count()<<':'<<std::setw(2)<<time.seconds().count()<<'Z';return out.str();
}
double earthSiderealAngle(double utc) {
    const double days=utc/86400+2440587.5-2451545.0,T=days/36525;
    return std::remainder((280.46061837+360.98564736629*days+.000387933*T*T-T*T*T/38710000)*pi/180,2*pi);
}
Vec3 orbitToScene(const Vec3& p) {return {p.x*12/re,p.z*12/re,-p.y*12/re};}
bool OrbitReplay::load(const std::filesystem::path& path,std::string& error) {
    size_t lineNumber=0;
    try {
        if(std::filesystem::file_size(path)>64*1024*1024)throw std::runtime_error("File exceeds 64 MiB limit.");
        std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open orbit file.");
        OrbitReplay next;next.name=path.filename().string();
        std::string line;bool oem=false,meta=false,cov=false,header=false,frameSet=false,timeSet=false,earth=false;int segments=0;
        while(std::getline(in,line)) {
            ++lineNumber;if(line.size()>16384)throw std::runtime_error("Line too long.");
            if(lineNumber==1 && line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);
            line=trim(line);if(line.empty())continue;
            if(line.starts_with("#")) {
                if(line.starts_with("# frame=")){if(header)throw std::runtime_error("Frame declaration must precede samples and header.");next.frame=readFrame(trim(line.substr(8)));frameSet=true;}
                continue;
            }
            if(line.starts_with("COMMENT"))continue;
            if(line.starts_with("CCSDS_OEM_VERS")){if(header)throw std::runtime_error("Unexpected OEM header.");oem=true;continue;}
            if(line=="META_START"){if(++segments>1)throw std::runtime_error("This importer accepts one OEM segment per file. Split segments before loading.");meta=true;continue;}
            if(line=="META_STOP"){if(!meta||!frameSet||!timeSet||!earth)throw std::runtime_error("OEM requires CENTER_NAME=EARTH, REF_FRAME, TIME_SYSTEM=UTC.");meta=false;header=true;continue;}
            if(line=="COVARIANCE_START"){cov=true;continue;}if(line=="COVARIANCE_STOP"){cov=false;continue;}if(cov)continue;
            if(oem && line.find('=')!=std::string::npos) {
                auto eq=line.find('=');auto key=trim(line.substr(0,eq)),value=trim(line.substr(eq+1));
                if(meta) {
                    if(key=="REF_FRAME"){next.frame=readFrame(value);frameSet=true;}
                    if(key=="TIME_SYSTEM"){if(value!="UTC")throw std::runtime_error("Only UTC OEM time is supported.");timeSet=true;}
                    if(key=="CENTER_NAME"){if(value!="EARTH")throw std::runtime_error("Only Earth-centered trajectories are supported.");earth=true;}
                    if(key=="REF_FRAME_EPOCH")throw std::runtime_error("Epoch-dependent reference frames are not supported.");
                }
                continue;
            }
            if(!oem && !header) {
                if(line!="utc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s")throw std::runtime_error("CSV header must be utc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s.");
                if(!frameSet)throw std::runtime_error("CSV requires # frame=EME2000 or # frame=ECEF before the header.");header=true;continue;
            }
            if(!header||meta)throw std::runtime_error("Position samples must follow metadata/header.");
            if(!oem)std::replace(line.begin(),line.end(),',',' ');
            std::istringstream row(line);row.imbue(std::locale::classic());std::string epoch;OrbitSample state;
            if(!(row>>epoch>>state.position.x>>state.position.y>>state.position.z>>state.velocity.x>>state.velocity.y>>state.velocity.z))throw std::runtime_error("Expected timestamp, three positions and three velocities.");
            // OEM acceleration columns are allowed, but replay uses cubic Hermite position/velocity interpolation.
            std::vector<double> extra;double x;while(row>>x)extra.push_back(x);
            if(!row.eof()||(!extra.empty()&&(!oem||extra.size()!=3)))throw std::runtime_error("Invalid trailing columns.");
            for(double a:extra)if(!std::isfinite(a))throw std::runtime_error("Nonfinite acceleration.");
            state.utc=parseOrbitUtc(epoch);
            if(!std::isfinite(state.position.norm())||!std::isfinite(state.velocity.norm())||state.position.norm()<6370||state.position.norm()>1e7||state.velocity.norm()>100)throw std::runtime_error("Invalid Earth-orbit state; check kilometer units.");
            if(!next.samples.empty()&&state.utc<=next.samples.back().utc)throw std::runtime_error("Timestamps must strictly increase (no duplicates).");
            next.samples.push_back(state);if(next.samples.size()>500000)throw std::runtime_error("Too many samples (maximum 500000).");
        }
        if(cov||meta||next.samples.size()<2)throw std::runtime_error("Incomplete file or fewer than two samples.");
        *this=std::move(next);error.clear();return true;
    } catch(const std::exception& ex) {error="Line "+std::to_string(lineNumber)+": "+ex.what();return false;}
}
bool OrbitReplay::sample(double utc,OrbitSample& out) const {
    if(samples.empty()||!std::isfinite(utc)||utc<samples.front().utc||utc>samples.back().utc)return false;
    auto b=std::lower_bound(samples.begin(),samples.end(),utc,[](const auto& s,double t){return s.utc<t;});
    if(b==samples.begin()||b->utc==utc){out=*b;return true;}
    const auto& a=*(b-1);double dt=b->utc-a.utc,u=(utc-a.utc)/dt,u2=u*u,u3=u2*u;
    out.utc=utc;
    out.position=a.position*(2*u3-3*u2+1)+a.velocity*(dt*(u3-2*u2+u))+b->position*(-2*u3+3*u2)+b->velocity*(dt*(u3-u2));
    out.velocity=a.position*((6*u2-6*u)/dt)+a.velocity*(3*u2-4*u+1)+b->position*((-6*u2+6*u)/dt)+b->velocity*(3*u2-2*u);
    return std::isfinite(out.position.norm())&&out.position.norm()>6370;
}
OrbitSample OrbitReplay::inertial(const OrbitSample& state) const {
    if(frame==OrbitFrame::EME2000)return state;
    double angle=earthSiderealAngle(state.utc);
    // GMST-only Earth rotation; no EOP, polar motion, precession/nutation or UT1 correction.
    return {state.utc,rotateZ(state.position,angle),rotateZ(state.velocity+Vec3(0,0,omega).cross(state.position),angle)};
}
double atmosphereDensity(double h) {
    const double heights[]={0,20,40,60,80,100,120,150,200,300,400,500,600,800,1000};
    const double rho[]={1.225,.0889,.003996,.0003097,1.846e-5,5.6e-7,2.22e-8,2.08e-9,2.5e-10,2.4e-11,3.7e-12,6.9e-13,1.5e-13,1.2e-14,3e-15};
    if(h<0)return rho[0];
    for(int i=0;i<14;++i)if(h<heights[i+1])return std::exp(std::log(rho[i])+(std::log(rho[i+1])-std::log(rho[i]))*(h-heights[i])/(heights[i+1]-heights[i]));
    return 0;
}
Vec3 orbitAcceleration(const Vec3& r,const Vec3& v,const OrbitForces& f) {
    double radius=r.norm();Vec3 a=r*(-mu/(radius*radius*radius));
    if(f.j2){double z2=r.z*r.z/(radius*radius),k=1.5*1.08262668e-3*mu*re*re/std::pow(radius,5);a+=Vec3(r.x*(5*z2-1),r.y*(5*z2-1),r.z*(5*z2-3))*k;}
    if(f.drag){Vec3 relative=(v-Vec3(0,0,omega).cross(r))*1000;double rho=atmosphereDensity(radius-re)*f.densityScale;a+=relative*(-.5*rho*f.cd*f.area/f.mass*relative.norm()/1000);}
    return a;
}
OrbitSample integrateOrbit(const OrbitSample& s,double dt,const OrbitForces& f) {
    auto a1=orbitAcceleration(s.position,s.velocity,f),v1=s.velocity;
    auto v2=s.velocity+a1*(dt/2),a2=orbitAcceleration(s.position+v1*(dt/2),v2,f);
    auto v3=s.velocity+a2*(dt/2),a3=orbitAcceleration(s.position+v2*(dt/2),v3,f);
    auto v4=s.velocity+a3*dt,a4=orbitAcceleration(s.position+v3*dt,v4,f);
    return {s.utc+dt,s.position+(v1+v2*2+v3*2+v4)*(dt/6),s.velocity+(a1+a2*2+a3*2+a4)*(dt/6)};
}
}
