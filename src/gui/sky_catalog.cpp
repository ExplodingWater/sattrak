#include "gui/sky_catalog.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace gui {
aero::math::Vec3 skyDirection(double raDegrees, double decDegrees) {
    constexpr double radians = 3.14159265358979323846 / 180;
    const double ra = raDegrees*radians, dec = decDegrees*radians;
    return {std::cos(dec)*std::cos(ra), std::sin(dec), -std::cos(dec)*std::sin(ra)};
}
bool SkyCatalog::load(const std::filesystem::path& folder, std::string& error) {
    SkyCatalog loaded;
    const auto valid = [](double ra, double dec) {
        return std::isfinite(ra) && std::isfinite(dec) && ra >= 0 && ra < 360 && dec >= -90 && dec <= 90;
    };
    auto read = [&](const char* name, auto parse) {
        std::ifstream file(folder/name);
        if (!file) { error=std::string("Cannot open sky asset: ")+name; return false; }
        std::string line; int number=0;
        while (std::getline(file,line)) {
            ++number;
            if (line.empty() || line[0]=='#') continue;
            std::istringstream in(line);
            if (!parse(in) || !(in >> std::ws).eof()) {
                error=std::string(name)+":"+std::to_string(number)+": invalid sky record"; return false;
            }
        }
        if (file.bad()) { error=std::string("Cannot read sky asset: ")+name; return false; }
        return true;
    };
    std::unordered_set<int> ids;
    if (!read("stars.txt", [&](auto& in) {
        SkyStar s; double ra,dec;
        if (!(in>>s.hip>>ra>>dec>>s.magnitude>>s.bv) || !valid(ra,dec) || s.hip<=0 ||
            !std::isfinite(s.magnitude) || s.magnitude < -3 || s.magnitude>6 ||
            !std::isfinite(s.bv) || s.bv < -1 || s.bv>5 || !ids.insert(s.hip).second) return false;
        s.direction=skyDirection(ra,dec);loaded.stars.push_back(s);return true;
    })) return false;
    if (!read("lines.txt", [&](auto& in) {
        std::string id;double ra1,dec1,ra2,dec2;
        if (!(in>>id>>ra1>>dec1>>ra2>>dec2) || !valid(ra1,dec1) || !valid(ra2,dec2)) return false;
        loaded.segments.push_back({skyDirection(ra1,dec1),skyDirection(ra2,dec2)});return true;
    })) return false;
    if (!read("labels.txt", [&](auto& in) {
        SkyLabel l;double ra,dec;
        if (!(in>>l.id>>ra>>dec>>std::quoted(l.name)) || !valid(ra,dec) || l.name.empty()) return false;
        l.direction=skyDirection(ra,dec);loaded.labels.push_back(l);return true;
    })) return false;
    if (loaded.stars.empty() || loaded.segments.empty() || loaded.labels.empty()) {
        error="Sky catalog is empty";return false;
    }
    *this=std::move(loaded);error.clear();return true;
}
} // namespace gui
