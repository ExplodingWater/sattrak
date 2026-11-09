#include "gui/sky_catalog.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_set>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    int failures=0;
    auto check=[&](bool value,const char* message){if(!value){std::cerr<<message<<'\n';++failures;}};
    using aero::math::Vec3;
    using gui::skyDirection;
    // These must agree with orbitToScene, including the negative scene Z.
    check((skyDirection(0,0)-Vec3{1,0,0}).norm()<1e-12,"Vernal equinox axis");
    check((skyDirection(90,0)-Vec3{0,0,-1}).norm()<1e-12,"RA increases toward negative scene Z");
    check((skyDirection(0,90)-Vec3{0,1,0}).norm()<1e-12,"North celestial pole");
    check((skyDirection(360,-30)-skyDirection(0,-30)).norm()<1e-12,"RA wrap");
    gui::SkyCatalog sky;std::string error;
    check(sky.load(argv[1],error),error.c_str());
    check(sky.stars.size()==5044,"Bundled magnitude-six catalog completeness");
    check(sky.segments.size()==743,"Bundled constellation segment count");
    check(sky.labels.size()==89,"88 constellations, Serpens has two regions");
    std::unordered_set<std::string> constellations;
    for(const auto& label:sky.labels)constellations.insert(label.id);
    check(constellations.size()==88,"All 88 constellation identifiers");
    for(const auto& s:sky.stars)check(std::abs(s.direction.norm()-1)<1e-12,"Unit star directions");
    auto star=[&](int id)->const gui::SkyStar* {
        auto it=std::find_if(sky.stars.begin(),sky.stars.end(),[&](const auto& s){return s.hip==id;});
        return it==sky.stars.end()?nullptr:&*it;
    };
    const auto* sirius=star(32349);const auto* polaris=star(11767);
    check(sirius && (sirius->direction-skyDirection(101.2872,-16.7161)).norm()<.00001 &&
        std::abs(sirius->magnitude+1.44)<.1,"Sirius reference coordinates and brightness");
    check(polaris && polaris->direction.y>.9999,"Polaris is close to north celestial pole");
    const auto* betelgeuse=star(27989);const auto* rigel=star(24436);
    check(betelgeuse && rigel && betelgeuse->bv>rigel->bv+1,"Orion red/blue star color indices");
    // Failed reloads must preserve the previous valid catalog.
    const auto root=std::filesystem::temp_directory_path()/
        ("sattrak-sky-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    for(const auto* name:{"stars.txt","lines.txt","labels.txt"})
        std::filesystem::copy_file(std::filesystem::path(argv[1])/name,root/name);
    for(const auto* invalid:{"1 360 0 1 0.5\n","1 0 91 1 0.5\n","1 0 0 nan 0.5\n","1 0 0 1 0.5 extra\n","1 0 0 1 0.5\n1 1 1 1 0.5\n"}) {
        {std::ofstream out(root/"stars.txt");out<<invalid;}
        check(!sky.load(root,error) && error.find("stars.txt:")==0 && sky.stars.size()==5044,"Reject malformed catalog transactionally");
    }
    std::filesystem::remove(root/"stars.txt");
    check(!sky.load(root,error) && error.find("Cannot open")!=std::string::npos && sky.labels.size()==89,"Missing asset diagnostic");
    std::filesystem::remove_all(root);
    return failures?1:0;
}
