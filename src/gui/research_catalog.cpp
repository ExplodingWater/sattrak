#include "gui/research_catalog.hpp"
#include <fstream>
#include <cmath>
#include <set>
#include "../../external/nlohmann/json.hpp"
#include <sstream>
#include <filesystem>
#include <iomanip>
namespace gui {
bool ResearchCatalog::load(const std::string& folder) {
    satellites.clear();
    int y,m,d,h,minute,second;
    std::ifstream date(folder+"/epoch.txt");
    if(!(date>>y>>m>>d>>h>>minute>>second) || y<2000 || y>2100 || m<1 || m>12 || d<1 || d>31 || h<0 || h>23 || minute<0 || minute>59 || second<0 || second>59) {
        status="Catalog date missing or invalid";return false;
    }
    epoch=libsgp4::DateTime(y,m,d,h,minute,second);
    snapshotEpoch=epoch;
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
            libsgp4::Tle tle(name,a,b);libsgp4::SGP4 orbit(tle);
            orbit.FindPosition(tle.Epoch());
            satellites.push_back({name,id,orbit,tle.Epoch(),tle.Inclination(true),tle.Eccentricity(),tle.MeanMotion(),"CelesTrak"});
        } catch(const std::exception&) {++skipped;}
    }
    status=std::to_string(satellites.size())+" satellites | "+epoch.ToString();
    if(skipped)status+=" | "+std::to_string(skipped)+" invalid records omitted";
    return !satellites.empty();
}
bool ResearchCatalog::stale(size_t index,double seconds) const {
    return index>=satellites.size() || !std::isfinite(seconds) ||
        std::abs((epoch-satellites[index].elementEpoch).TotalDays()+seconds/86400)>maxPredictionAgeDays;
}
std::optional<libsgp4::Eci> ResearchCatalog::prediction(size_t index,double seconds,bool allowStale) const {
    if(index>=satellites.size() || !std::isfinite(seconds) || (!allowStale&&stale(index,seconds)))return std::nullopt;
    try {
        auto state=satellites[index].orbit.FindPosition(epoch.AddSeconds(seconds));
        const double radius=state.Position().Magnitude(),speed=state.Velocity().Magnitude();
        // Reject nonphysical Earth satellite states even when inspecting stale elements.
        if(!std::isfinite(radius)||!std::isfinite(speed)||radius<=0 ||
           !std::isfinite(state.ToGeodetic().altitude)||state.ToGeodetic().altitude<=0 ||
           speed*speed>=2*398600.4418/radius)return std::nullopt;
        return state;
    }catch(const std::exception&){return std::nullopt;}
}
bool ResearchCatalog::position(size_t index,double seconds,aero::math::Vec3& point,bool allowStale) const {
    auto state=prediction(index,seconds,allowStale);if(!state)return false;
    try {
        auto date=epoch.AddSeconds(seconds);
        auto p=state->Position();
        double angle=date.ToGreenwichSiderealTime(),c=std::cos(angle),s=std::sin(angle);
        point={c*p.x+s*p.y,p.z,s*p.x-c*p.y};
        return std::isfinite(point.norm()) && point.norm()>6370;
    } catch(const std::exception&) {return false;}
}
bool ResearchCatalog::mergeSatnogs(const std::string& folder,std::string& error) {
    using Json=nlohmann::json;
    try {
        auto read=[&](const char* file){auto path=std::filesystem::path(folder)/file;
            if(std::filesystem::file_size(path)>16000000)throw std::runtime_error("SatNOGS response exceeds limit");
            std::ifstream input(path);Json data;input>>data;
            if(!data.is_array()||data.size()>30000)throw std::runtime_error("Invalid SatNOGS array");return data;};
        auto text=[](const Json& row,const char* key){auto i=row.find(key);return i!=row.end()&&i->is_string()?i->get<std::string>():std::string{};};
        auto number=[](const Json& row,const char* key,double fallback=0.0){auto i=row.find(key);return i!=row.end()&&i->is_number()?i->get<double>():fallback;};
        std::map<unsigned,SatelliteInfo> metadata;
        for(const auto& row:read("satellites.json")) {
            double id=number(row,"norad_cat_id");if(id<1||id>999999999)continue;
            metadata[unsigned(id)]={text(row,"sat_id"),text(row,"name"),text(row,"names"),text(row,"status"),text(row,"launched"),text(row,"deployed"),text(row,"operator"),text(row,"countries"),text(row,"updated"),{}};
        }
        for(const auto& row:read("transmitters.json")) {
            auto id=number(row,"norad_cat_id");if(id<1||id>999999999)continue;
            auto found=metadata.find(unsigned(id));if(found==metadata.end()||found->second.transmitters.size()>=100)continue;
            std::ostringstream line;line<<text(row,"description")<<" | "<<text(row,"mode")<<" | "<<text(row,"status");
            double low=number(row,"downlink_low"),high=number(row,"downlink_high");
            if(low>0){line<<" | "<<std::fixed<<std::setprecision(6)<<low/1e6;if(high>low)line<<" - "<<high/1e6;line<<" MHz";}
            double baud=number(row,"baud");if(baud>0)line<<" | "<<baud<<" baud";
            found->second.transmitters.push_back(line.str());
        }
        std::vector<CatalogStation> ground;std::set<int> groundIds;
        for(const auto& row:read("stations.json")) {
            auto id=number(row,"id");double lat=number(row,"lat",999),lon=number(row,"lng",999),alt=number(row,"altitude");
            if(id<1||id>100000000||!std::isfinite(lat)||!std::isfinite(lon)||std::abs(lat)>90||std::abs(lon)>180||!std::isfinite(alt)||alt< -500||alt>10000||!groundIds.insert(int(id)).second)continue;
            CatalogStation station;station.id=int(id);station.name=text(row,"name");station.status=text(row,"status");station.lastSeen=text(row,"last_seen");
            station.latitude=lat;station.longitude=lon;station.altitude=alt;station.horizon=number(row,"min_horizon");station.observations=int(std::clamp(number(row,"observations"),0.0,2e9));
            auto antennas=row.find("antenna");if(antennas!=row.end()&&antennas->is_array())for(const auto& antenna:*antennas){if(!station.antennas.empty())station.antennas+="; ";station.antennas+=text(antenna,"band")+" "+text(antenna,"antenna_type_name");}
            ground.push_back(std::move(station));
        }
        auto combined=satellites;
        std::map<unsigned,size_t> ids;for(size_t i=0;i<combined.size();++i)ids[combined[i].norad]=i;
        size_t added=0;
        for(const auto& row:read("tle.json")) {
            try {
                double value=number(row,"norad_cat_id");if(value<1||value>99999)continue;
                unsigned id=unsigned(value);auto metadataRow=metadata.find(id);
                if(metadataRow==metadata.end()||metadataRow->second.status=="decayed")continue;
                auto a=text(row,"tle1"),b=text(row,"tle2");
                auto checksum=[](const std::string& line,char type){if(line.size()!=69||line[0]!=type||line[68]<'0'||line[68]>'9')return false;int sum=0;for(int i=0;i<68;++i)sum+=line[i]>='0'&&line[i]<='9'?line[i]-'0':line[i]=='-'?1:0;return sum%10==line[68]-'0';};
                if(!checksum(a,'1')||!checksum(b,'2')||a.substr(2,5)!=b.substr(2,5)||std::stoul(a.substr(2,5))!=id)continue;
                libsgp4::Tle tle(metadataRow->second.name,a,b);libsgp4::SGP4 orbit(tle);orbit.FindPosition(tle.Epoch());
                ResearchSatellite satellite{metadataRow->second.name,id,orbit,tle.Epoch(),tle.Inclination(true),tle.Eccentricity(),tle.MeanMotion(),"SatNOGS / "+text(row,"tle_source")};
                auto existing=ids.find(id);
                if(existing==ids.end()){ids[id]=combined.size();combined.push_back(std::move(satellite));++added;}
                else if(combined[existing->second].elementEpoch<tle.Epoch())combined[existing->second]=std::move(satellite);
            }catch(const std::exception&){} // A malformed or decayed orbit must not discard other valid records.
        }
        if(metadata.empty()||ground.empty())throw std::runtime_error("Empty SatNOGS dataset");
        info=std::move(metadata);stations=std::move(ground);satellites=std::move(combined);
        std::ifstream downloaded(folder+"/downloaded.txt");std::getline(downloaded,satnogsDownloaded);
        satnogsOrigin=folder.starts_with("assets/")?"Bundled SatNOGS snapshot":"Cached SatNOGS snapshot";
        status=std::to_string(satellites.size())+" satellites | "+std::to_string(stations.size())+" SatNOGS ground stations";
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
