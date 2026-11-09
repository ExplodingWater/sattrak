#pragma once
#include "../../external/nlohmann/json.hpp"
#include <array>
#include <charconv>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace gui::updates {
struct Version {
    std::array<unsigned,3> core{};
    std::vector<std::string> preview;
};
inline std::optional<Version> version(std::string text) {
    static const std::regex pattern(R"(^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$)");
    std::smatch match;
    if(text.size()>128 || !std::regex_match(text,match,pattern))return {};
    Version result;
    for(int i=0;i<3;++i) {
        const auto number=match[i+1].str();
        if(std::from_chars(number.data(),number.data()+number.size(),result.core[i]).ec!=std::errc{})return {};
    }
    const auto preview=match[4].str();
    for(size_t offset=0;offset<preview.size();) {
        auto end=preview.find('.',offset);if(end==std::string::npos)end=preview.size();
        auto item=preview.substr(offset,end-offset);
        if(item.find_first_not_of("0123456789")==std::string::npos && item.size()>1 && item[0]=='0')return {};
        result.preview.push_back(item);offset=end+1;
    }
    return result;
}
inline bool newer(const Version& candidate,const Version& installed) {
    if(candidate.core!=installed.core)return candidate.core>installed.core;
    if(candidate.preview.empty() || installed.preview.empty())return candidate.preview.empty() && !installed.preview.empty();
    for(size_t i=0;i<std::min(candidate.preview.size(),installed.preview.size());++i) {
        const auto& a=candidate.preview[i];const auto& b=installed.preview[i];if(a==b)continue;
        const bool an=a.find_first_not_of("0123456789")==std::string::npos;
        const bool bn=b.find_first_not_of("0123456789")==std::string::npos;
        if(an!=bn)return !an;
        if(an && a.size()!=b.size())return a.size()>b.size();
        return a>b;
    }
    return candidate.preview.size()>installed.preview.size();
}
struct Package { std::string url,sha256; uint64_t size=0; };
inline std::optional<Package> windowsPackage(const nlohmann::json& row,const std::string& tag) {
    if(!row.contains("assets") || !row["assets"].is_array())return {};
    static const std::regex namePattern(R"(^sattrak-[0-9A-Za-z.+-]+-windows-x64-setup\.exe$)");
    static const std::regex digestPattern("^sha256:[0-9a-f]{64}$");
    for(const auto& asset:row["assets"]) {
        if(!asset.is_object() || !asset.contains("name") || !asset["name"].is_string() ||
           !asset.contains("state") || asset["state"]!="uploaded" ||
           !asset.contains("digest") || !asset["digest"].is_string() ||
           !asset.contains("size") || !asset["size"].is_number_unsigned() ||
           !asset.contains("browser_download_url") || !asset["browser_download_url"].is_string())continue;
        const auto name=asset["name"].get<std::string>(),digest=asset["digest"].get<std::string>();
        if(!std::regex_match(name,namePattern) || !std::regex_match(digest,digestPattern))continue;
        const auto size=asset["size"].get<uint64_t>();if(size==0 || size>2ull*1024*1024*1024)continue;
        const auto url="https://github.com/ExplodingWater/sattrak/releases/download/"+tag+"/"+name;
        if(asset["browser_download_url"].get<std::string>()!=url)continue;
        return Package{url,digest.substr(7),size};
    }
    return {};
}
struct Release { std::string tag,url; std::optional<Package> package; };
inline std::optional<Release> selectRelease(const std::string& body,const std::string& installed) {
    const auto current=version(installed);
    if(!current)throw std::runtime_error("Invalid build version");
    if(body.size()>2*1024*1024)throw std::runtime_error("Release response too large");
    const auto rows=nlohmann::json::parse(body);
    if(!rows.is_array())throw std::runtime_error("Invalid release response");
    Version best=*current;std::optional<Release> selected;
    for(const auto& row:rows) {
        if(!row.is_object() || !row.contains("draft") || !row["draft"].is_boolean() || row["draft"].get<bool>() ||
           !row.contains("prerelease") || !row["prerelease"].is_boolean() ||
           !row.contains("tag_name") || !row["tag_name"].is_string() ||
           !row.contains("published_at") || !row["published_at"].is_string())continue;
        const auto tag=row["tag_name"].get<std::string>();const auto candidate=version(tag);
        if(!candidate)continue;
        if(current->preview.empty() && (row["prerelease"].get<bool>() || !candidate->preview.empty()))continue;
        if(!newer(*candidate,best))continue;
        best=*candidate;
        // Construct the trusted destination instead of opening API-supplied URLs
        selected=Release{tag,"https://github.com/ExplodingWater/sattrak/releases/tag/"+tag,windowsPackage(row,tag)};
    }
    return selected;
}
}
