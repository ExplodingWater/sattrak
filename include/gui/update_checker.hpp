#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include <filesystem>
#include "gui/update_release.hpp"

namespace gui {
class UpdateChecker {
public:
    void start();
    void drawSettings();
    void drawNotice(bool allowed);
    static const char* installedVersion();
    static int commandLineCheck();
    static int downloadTest();
    void installOnExit();
private:
    friend struct UpdateChecks;
    void startWith(std::function<std::string()> fetch);
    enum class Status { Idle,Checking,Current,Available,Unavailable,Downloading,Ready };
    struct State {
        std::mutex mutex;
        Status status=Status::Idle;
        std::string tag,url;
        bool dismissed=false;
        bool automatic=true,configured=false,closing=false;
        std::filesystem::path target,installer;
        std::optional<updates::Package> package;
        std::string detail;
        uint64_t received=0;
    };
    std::shared_ptr<State> state=std::make_shared<State>();
    static void download(const std::shared_ptr<State>& shared,const updates::Package& package);
};
inline UpdateChecker& appUpdates(){static UpdateChecker checker;return checker;}
}
