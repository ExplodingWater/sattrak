#include "gui/update_release.hpp"
#include <iostream>
using namespace gui::updates;
int main() {
    int failures=0;
    auto check=[&](bool okay,const char* name){if(!okay){std::cerr<<name<<'\n';++failures;}};
    auto greater=[&](const char* a,const char* b){return newer(*version(a),*version(b));};
    check(greater("1.10.0","1.9.9"),"Numeric minor ordering");
    check(greater("2.0.0","1.99.99"),"Major ordering");
    check(greater("1.0.0","1.0.0-preview.20261103"),"Stable replaces preview");
    check(!greater("1.0.0-preview.20261104","1.0.0"),"Preview cannot replace stable of same version");
    check(greater("1.0.0-preview.10","1.0.0-preview.9"),"Numeric preview ordering");
    check(greater("1.0.0-beta","1.0.0-alpha"),"Preview name ordering");
    check(greater("1.0.0-beta.1","1.0.0-beta"),"Longer preview ordering");
    check(!greater("v1.0.0+new","1.0.0+old"),"Build metadata is not an update");
    for(const auto* invalid:{"", "1.0", "1.0.0-01", "01.0.0", "1.0.0/evil", "v1.0.0\n", "999999999999999999.0.0"})
        check(!version(invalid),"Reject malformed version");
    auto entry=[](const char* tag,bool preview=false,bool draft=false){
        return nlohmann::json{{"tag_name",tag},{"prerelease",preview},{"draft",draft},{"published_at","2026-11-03T17:00:00Z"},{"html_url","https://example.invalid"}};
    };
    nlohmann::json list=nlohmann::json::array({entry("v1.0.1"),entry("v1.2.0"),entry("v1.1.0"),entry("v9.0.0",false,true),entry("v2.0.0-preview.1",true)});
    auto chosen=selectRelease(list.dump(),"1.0.0");
    check(chosen && chosen->tag=="v1.2.0","Choose highest stable regardless of response order");
    check(chosen && chosen->url=="https://github.com/ExplodingWater/sattrak/releases/tag/v1.2.0","Ignore untrusted URL field");
    chosen=selectRelease(list.dump(),"1.0.0-preview.20261103");
    check(chosen && chosen->tag=="v2.0.0-preview.1","Preview builds can receive previews");
    check(!selectRelease(list.dump(),"10.0.0"),"Never offer downgrade");
    check(!selectRelease(nlohmann::json::array({entry("v1.0.0")}).dump(),"1.0.0"),"Equal version stays quiet");
    check(!selectRelease("[]","1.0.0"),"Empty releases stay quiet");
    auto packaged=entry("v1.1.0");
    const std::string assetUrl="https://github.com/ExplodingWater/sattrak/releases/download/v1.1.0/sattrak-1.1.0-windows-x64-setup.exe";
    packaged["assets"]=nlohmann::json::array({{{"name","sattrak-1.1.0-windows-x64-setup.exe"},{"state","uploaded"},{"size",uint64_t(100)},{"digest","sha256:"+std::string(64,'a')},{"browser_download_url",assetUrl}}});
    check(bool(windowsPackage(packaged,"v1.1.0")),"Accept verified installer metadata");
    packaged["assets"][0]["browser_download_url"]="https://example.com/update.exe";
    check(!windowsPackage(packaged,"v1.1.0"),"Reject installer from another host");
    packaged["assets"][0]["browser_download_url"]=assetUrl;
    packaged["assets"][0]["digest"]=nullptr;
    check(!windowsPackage(packaged,"v1.1.0"),"Missing digest requires manual update");
    packaged["assets"][0]["digest"]="sha256:"+std::string(64,'a');
    packaged["assets"][0]["size"]=uint64_t(3ull*1024*1024*1024);
    check(!windowsPackage(packaged,"v1.1.0"),"Reject oversized package");
    auto legacyPreview=entry("v1.0.0-preview.2",true);
    legacyPreview["assets"]=nlohmann::json::array({{{"name","sattrak-1.0.0-windows-x64-setup.exe"},
        {"state","uploaded"},{"size",uint64_t(100)},{"digest","sha256:"+std::string(64,'a')},
        {"browser_download_url","https://github.com/ExplodingWater/sattrak/releases/download/v1.0.0-preview.2/sattrak-1.0.0-windows-x64-setup.exe"}}});
    check(bool(windowsPackage(legacyPreview,"v1.0.0-preview.2")),"Preserve existing Windows preview installer naming");
    for(const auto* platform:{"macos-arm64","macos-x64","debian-amd64","debian-arm64"}) {
        auto native=entry("v1.1.0");
        const auto name=std::string("sattrak-1.1.0-")+platform+".tar.gz";
        const auto url="https://github.com/ExplodingWater/sattrak/releases/download/v1.1.0/"+name;
        native["assets"]=nlohmann::json::array({{{"name",name},{"state","uploaded"},{"size",uint64_t(100)},
            {"digest","sha256:"+std::string(64,'b')},{"browser_download_url",url}}});
        auto selected=selectRelease(nlohmann::json::array({native}).dump(),"1.0.0",platform);
        check(selected && selected->package && selected->package->url==url,"Select matching native architecture");
        check(!packageFor(native,"v1.1.0","windows-x64"),"Never select native package for Windows");
        check(!packageFor(native,"v1.1.0","unsupported"),"Unsupported platform is manual only");
        native["assets"][0]["digest"]=nullptr;
        check(!packageFor(native,"v1.1.0",platform),"Native digest is mandatory");
        native["assets"][0]["digest"]="sha256:"+std::string(64,'b');
        native["assets"][0]["name"]="sattrak-1.0.0-"+std::string(platform)+".tar.gz";
        check(!packageFor(native,"v1.1.0",platform),"Package name must match selected release version");
    }
    auto unpublished=entry("v5.0.0");unpublished["published_at"]=nullptr;
    check(!selectRelease(nlohmann::json::array({unpublished,entry("preview-2026-09-15-17233d5")}).dump(),"1.0.0"),"Skip unpublished and legacy tags");
    for(const auto& body:{std::string("not json"),std::string("{}"),std::string(2*1024*1024+1,' ')}) {
        bool rejected=false;try{selectRelease(body,"1.0.0");}catch(...){rejected=true;}
        check(rejected,"Reject invalid or oversized response");
    }
    return failures?1:0;
}
