// Native desktop launcher: seed an owned installation, then exec the active
// release in this process. Finder therefore tracks the actual GUI process.
#include "gui/native_update.hpp"
#include "gui/update_release.hpp"
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
namespace {
namespace fs=std::filesystem;
using gui::updates::Version;
std::string read(const fs::path& path) {
    std::ifstream file(path);std::string value;file>>value;
    if(!file)throw std::runtime_error("Cannot read "+path.string());
    return value;
}
std::string payloadVersion(const fs::path& payload) {
    const auto value=read(payload/"release-version.txt");
    if(!gui::updates::version(value) || read(payload/"platform.txt")!=gui::nativeupdate::platform() ||
        !fs::is_regular_file(payload/"ag") || !fs::is_regular_file(payload/"native_updater.py") || !fs::is_directory(payload/"assets"))
        throw std::runtime_error("The installed application payload is incomplete or incompatible");
    return value;
}
fs::path userRoot() {
    if(const auto* overrideRoot=std::getenv("SATTRAK_USER_INSTALL_ROOT")) {
        if(!fs::path(overrideRoot).is_absolute())throw std::runtime_error("SATTRAK_USER_INSTALL_ROOT must be absolute");
        return overrideRoot;
    }
    const auto* home=std::getenv("HOME");
    if(!home || !fs::path(home).is_absolute())throw std::runtime_error("User home directory is unavailable");
#ifdef __APPLE__
    return fs::path(home)/"Library/Application Support/sattrak/installation";
#else
    const auto* xdg=std::getenv("XDG_DATA_HOME");
    return (xdg && fs::path(xdg).is_absolute()?fs::path(xdg):fs::path(home)/".local/share")/"sattrak/installation";
#endif
}
struct Lock {
    int fd=-1;
    explicit Lock(const fs::path& root) {
        fd=open((root/"session.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
        if(fd>=0 && fd<3){int copy=fcntl(fd,F_DUPFD_CLOEXEC,3);close(fd);fd=copy;}
        if(fd<0)throw std::runtime_error("Cannot lock the user installation");
    }
    ~Lock(){if(fd>=0)close(fd);}
    void shared(){if(flock(fd,LOCK_SH)!=0)throw std::runtime_error("Cannot lock the user installation");}
    void exclusive(){if(flock(fd,LOCK_EX)!=0)throw std::runtime_error("Cannot lock the user installation");}
};
std::optional<std::string> active(const fs::path& root) {
    if(!fs::exists(root/"install.json"))return {};
    const auto marker=nlohmann::json::parse(std::ifstream(root/"install.json"));
    if(marker.value("schema",0)!=1 || marker.value("platform","")!=gui::nativeupdate::platform())
        throw std::runtime_error("The user installation belongs to another platform");
    if(!fs::exists(root/"current"))return {};
    const auto current=fs::canonical(root/"current");
    if(current.parent_path()!=root/"releases")throw std::runtime_error("Invalid active release path");
    return payloadVersion(current);
}
void copyPayload(const fs::path& source,const fs::path& stage) {
    fs::create_directory(stage);
    for(const auto& item:fs::recursive_directory_iterator(source)) {
        const auto destination=stage/item.path().lexically_relative(source);
        const auto status=item.symlink_status();
        if(fs::is_directory(status))fs::create_directory(destination);
        else if(fs::is_regular_file(status)) {
            fs::copy_file(item.path(),destination);
            fs::permissions(destination,item.path().filename()=="ag"?fs::perms::owner_all|fs::perms::group_read|fs::perms::group_exec|fs::perms::others_read|fs::perms::others_exec:
                fs::perms::owner_read|fs::perms::owner_write|fs::perms::group_read|fs::perms::others_read);
        }else throw std::runtime_error("Application payload contains a link or special file");
    }
}
void seed(const fs::path& source,const fs::path& root,const std::string& version) {
    fs::create_directories(root/"releases");fs::create_directories(root/"user-data/updates");
    const auto unique=std::to_string(getpid())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto stage=root/"releases"/(".bootstrap-"+unique);
    const auto destination=root/"releases"/("bootstrap-"+version);
    const auto pointer=root/(".current-"+unique);
    const auto markerTemporary=root/(".install-"+unique+".json");
    try {
        // Create the marker before the pointer, allowing an interrupted first
        // installation to resume. Existing preferences are never replaced.
        if(!fs::exists(root/"install.json")) {
            std::ofstream marker(markerTemporary);marker<<nlohmann::json{{"schema",1},{"platform",gui::nativeupdate::platform()}};
            marker.close();if(!marker)throw std::runtime_error("Cannot save installation marker");
            fs::rename(markerTemporary,root/"install.json");
        }
        if(!fs::exists(destination)) {
            copyPayload(source,stage);payloadVersion(stage);fs::rename(stage,destination);
        }else if(payloadVersion(destination)!=version)throw std::runtime_error("Invalid previous bootstrap release");
        for(const char* name:{"assets","earth_texture.jpg","LICENSE","TERMS.md"}) {
            const auto link=root/"user-data"/name,expected=fs::path("../current")/name;
            if(fs::is_symlink(link)) {
                if(fs::read_symlink(link)!=expected)throw std::runtime_error("Unexpected user-data asset link");
            } else if(fs::exists(link))throw std::runtime_error("User-data asset path already exists");
            else fs::create_symlink(expected,link);
        }
        fs::create_symlink(fs::path("releases")/destination.filename(),pointer);
        fs::rename(pointer,root/"current");
    }catch(...) {
        std::error_code ignored;fs::remove_all(stage,ignored);fs::remove(pointer,ignored);fs::remove(markerTemporary,ignored);throw;
    }
}
}
int main(int argc,char** argv) {
    try {
        if(geteuid()==0)throw std::runtime_error("Start sattrak as your normal user, without sudo");
        const auto exe=gui::nativeupdate::executable();
#ifdef __APPLE__
        const auto payload=fs::canonical(exe.parent_path()/"../Resources/payload");
#else
        const auto payload=fs::canonical(exe.parent_path()/"../lib/sattrak/payload");
#endif
        const auto packaged=payloadVersion(payload);
        auto root=userRoot();
        const bool fresh=!fs::exists(root) || fs::is_empty(root);
        if(fs::is_symlink(root))throw std::runtime_error("User installation root must not be a symlink");
        fs::create_directories(root);root=fs::canonical(root);
        struct stat info{};
        if(lstat(root.c_str(),&info)!=0 || info.st_uid!=geteuid() || (info.st_mode&0022))
            throw std::runtime_error("User installation must be owned and writable only by your account");
        if(!fresh && !fs::exists(root/"install.json"))throw std::runtime_error("Refusing to replace an unmanaged user folder");
        fs::permissions(root,fs::perms::owner_all);
        Lock lock(root);lock.shared();
        auto current=active(root);
        if(!current || gui::updates::newer(*gui::updates::version(packaged),*gui::updates::version(*current))) {
            bool exclusive=false;
            if(!current){lock.exclusive();exclusive=true;}
            else if(flock(lock.fd,LOCK_EX|LOCK_NB)==0)exclusive=true;
            else lock.shared();
            // An existing session can postpone seeding a newer installer. It is
            // safe to launch the complete active release until that session ends.
            current=active(root);
            if(exclusive && (!current || gui::updates::newer(*gui::updates::version(packaged),*gui::updates::version(*current))))seed(payload,root,packaged);
            lock.shared();
        }
        if(!active(root))throw std::runtime_error("The user installation could not be prepared");
        const auto program=fs::canonical(root/"current/ag");
        fs::current_path(root/"user-data");
        // The game marks this inherited lock close-on-exec before spawning its
        // update helper, so the helper cannot accidentally inherit a live lock.
        if(fcntl(lock.fd,F_SETFD,0)<0 || setenv("SATTRAK_NATIVE_SESSION_FD",std::to_string(lock.fd).c_str(),1)!=0)
            throw std::runtime_error("Cannot hand off the user session");
        std::vector<char*> arguments{const_cast<char*>(program.c_str())};
        for(int i=1;i<argc;++i)arguments.push_back(argv[i]);arguments.push_back(nullptr);
        execv(program.c_str(),arguments.data());
        throw std::runtime_error(std::string("Cannot start sattrak: ")+std::strerror(errno));
    }catch(const std::exception& error) {
        std::cerr<<"sattrak: "<<error.what()<<'\n';
#ifdef __APPLE__
        if(!std::getenv("SATTRAK_SUPPRESS_LAUNCH_ALERTS")) {
            auto message=CFStringCreateWithCString(nullptr,error.what(),kCFStringEncodingUTF8);
            CFUserNotificationDisplayAlert(0,kCFUserNotificationStopAlertLevel,nullptr,nullptr,nullptr,
                CFSTR("sattrak could not start"),message,CFSTR("OK"),nullptr,nullptr,nullptr);
            if(message)CFRelease(message);
        }
#endif
        return 1;
    }
}
