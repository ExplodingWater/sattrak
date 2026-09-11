#include "gui/research_catalog.hpp"
#include <fstream>
#include <cmath>
#include <set>
namespace gui {
bool ResearchCatalog::load(const std::string& folder) {
    satellites.clear();
    int y,m,d,h,minute,second;
    std::ifstream date(folder+"/epoch.txt");
    if(!(date>>y>>m>>d>>h>>minute>>second) || y<2000 || y>2100 || m<1 || m>12 || d<1 || d>31 || h<0 || h>23 || minute<0 || minute>59 || second<0 || second>59) {
        status="Catalog date missing or invalid";return false;
    }
    epoch=libsgp4::DateTime(y,m,d,h,minute,second);
    std::ifstream input(folder+"/research.tle");
    std::string name,a,b; size_t skipped=0; std::set<unsigned> ids;
    while(std::getline(input,name) && satellites.size()<10000) {
        if(!std::getline(input,a) || !std::getline(input,b)){++skipped;break;}
        if(!a.empty()&&a.back()=='\r')a.pop_back();
        if(!b.empty()&&b.back()=='\r')b.pop_back();
        if(!name.empty()&&name.back()=='\r')name.pop_back();
        auto valid=[](const std::string& line,char type){
            if(line.size()!=69 || line[0]!=type || line[68]<'0'||line[68]>'9')return false;
            int sum=0;for(int i=0;i<68;++i)sum+=line[i]>='0'&&line[i]<='9'?line[i]-'0':line[i]=='-'?1:0;
            return sum%10==line[68]-'0';
        };
        try {
            if(!valid(a,'1') || !valid(b,'2') || a.substr(2,5)!=b.substr(2,5)) {++skipped;continue;}
            unsigned id=unsigned(std::stoul(a.substr(2,5)));
            if(!ids.insert(id).second)continue;
            libsgp4::SGP4 orbit(libsgp4::Tle(name,a,b));
            orbit.FindPosition(epoch);
            satellites.push_back({name,id,orbit});
        } catch(const std::exception&) {++skipped;}
    }
    status=std::to_string(satellites.size())+" satellites | "+epoch.ToString();
    if(skipped)status+=" | "+std::to_string(skipped)+" invalid records omitted";
    return !satellites.empty();
}
bool ResearchCatalog::position(size_t index,double seconds,aero::math::Vec3& point) const {
    if(index>=satellites.size() || !std::isfinite(seconds))return false;
    try {
        auto date=epoch.AddSeconds(seconds);
        auto p=satellites[index].orbit.FindPosition(date).Position();
        double angle=date.ToGreenwichSiderealTime(),c=std::cos(angle),s=std::sin(angle);
        point={c*p.x+s*p.y,p.z,s*p.x-c*p.y};
        return std::isfinite(point.norm()) && point.norm()>6370;
    } catch(const std::exception&) {return false;}
}
}
