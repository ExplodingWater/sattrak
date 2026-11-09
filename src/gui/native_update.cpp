#include "gui/native_update.hpp"
#ifndef _WIN32
#include "gui/update_release.hpp"
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <spawn.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char** environ;
namespace gui::nativeupdate {
namespace fs=std::filesystem;
const char* platform(){return updates::buildPlatform();}
fs::path executable() {
    std::error_code error;
#ifdef __APPLE__
    uint32_t size=0;_NSGetExecutablePath(nullptr,&size);std::string buffer(size,'\0');
    if(_NSGetExecutablePath(buffer.data(),&size)!=0)return {};
    return fs::canonical(buffer.c_str(),error);
#else
    return fs::canonical("/proc/self/exe",error);
#endif
}
fs::path installedFolder() {
    if(geteuid()==0 || !platform()[0])return {};
    const auto exe=executable(),root=exe.parent_path().parent_path().parent_path();
    if(exe.parent_path().parent_path().filename()!="releases")return {};
    std::error_code error;
    if(exe.empty() || !fs::is_regular_file(root/"install.json",error))return {};
    struct stat info{};
    if(lstat(root.c_str(),&info)!=0 || !S_ISDIR(info.st_mode) || info.st_uid!=geteuid() || (info.st_mode&0022))return {};
    try {
        auto marker=nlohmann::json::parse(std::ifstream(root/"install.json"));
        if(marker.value("schema",0)!=1 || marker.value("platform","")!=platform())return {};
        if(!fs::equivalent(exe,root/"current/ag",error) || error ||
            !fs::is_regular_file(root/"current/native_updater.py",error) ||
            !fs::is_regular_file(root/"current/release-version.txt",error))return {};
        return root;
    }catch(...){return {};}
}
fs::path cacheFolder() {
    auto root=installedFolder();
    if(!root.empty())return root/"user-data/updates";
    const auto* home=std::getenv("HOME");if(!home || !*home)throw std::runtime_error("User folder unavailable");
#ifdef __APPLE__
    return fs::path(home)/"Library/Caches/sattrak/updates";
#else
    const auto* cache=std::getenv("XDG_CACHE_HOME");
    return cache && fs::path(cache).is_absolute()?fs::path(cache)/"sattrak/updates":fs::path(home)/".cache/sattrak/updates";
#endif
}
void configureRuntime() {
    const auto root=installedFolder();
    if(root.empty()) {
        const auto exe=executable(),candidate=exe.parent_path().parent_path().parent_path();
        std::error_code error;
        if(exe.parent_path().parent_path().filename()=="releases" && fs::is_regular_file(candidate/"install.json",error))
            throw std::runtime_error("Inactive or invalid managed release; start sattrak using its launcher");
        return;
    }
    static int session=-1;
    if(session<0) {
        session=open((root/"session.lock").c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
        if(session<0 || flock(session,LOCK_SH)!=0)throw std::runtime_error("Cannot lock native installation");
        // Python's launcher locks before resolving current. Direct ag launches
        // must recheck after waiting, in case an update just switched versions.
        if(installedFolder()!=root)throw std::runtime_error("Installation updated; start sattrak using its launcher");
    }
    fs::current_path(root/"user-data");
}
namespace {
int helper(const std::vector<std::string>& arguments,bool wait,const fs::path& log={}) {
    auto script=executable().parent_path()/"native_updater.py";
    if(!fs::is_regular_file(script))throw std::runtime_error("Native updater helper is missing");
    std::vector<std::string> values{"python3",script.string()};
    values.insert(values.end(),arguments.begin(),arguments.end());
    std::vector<char*> argv;for(auto& value:values)argv.push_back(value.data());argv.push_back(nullptr);
    posix_spawn_file_actions_t files;posix_spawn_file_actions_init(&files);
    const auto output=log.empty()?fs::path("/dev/null"):log;
    const int outFlags=log.empty()?O_WRONLY:O_WRONLY|O_CREAT|O_APPEND;
    posix_spawn_file_actions_addopen(&files,STDOUT_FILENO,output.c_str(),outFlags,0600);
    posix_spawn_file_actions_adddup2(&files,STDOUT_FILENO,STDERR_FILENO);
    pid_t pid=0;const int result=posix_spawnp(&pid,"python3",&files,nullptr,argv.data(),environ);
    posix_spawn_file_actions_destroy(&files);
    if(result!=0)throw std::runtime_error("Python 3 could not start the native updater");
    if(!wait)return 0;
    int status=0;while(waitpid(pid,&status,0)<0)if(errno!=EINTR)throw std::runtime_error("Updater wait failed");
    return WIFEXITED(status)?WEXITSTATUS(status):1;
}
}
bool verify(const fs::path& file,const std::string& digest,uint64_t size) {
    return helper({"--verify",file.string(),"--sha256",digest,"--size",std::to_string(size)},true)==0;
}
void launchOnExit(const fs::path& root,const fs::path& archive,const std::string& digest,uint64_t size,const std::string& tag) {
    if(installedFolder()!=root)throw std::runtime_error("Installation changed");
    const auto cache=cacheFolder();fs::create_directories(cache);
    helper({"--apply",archive.string(),"--root",root.string(),"--sha256",digest,"--size",std::to_string(size),
            "--tag",tag,"--parent",std::to_string(getpid())},false,cache/"update-install.log");
}
}
#endif
