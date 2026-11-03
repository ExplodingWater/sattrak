#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace gui {
inline bool tutorialRequested = false;
inline bool tutorialRunning = false;
// Per-user state, independent of the installation folder and working directory.
inline std::filesystem::path tutorialStatePath() {
#ifdef _WIN32
    wchar_t* root=nullptr;
    size_t length=0;
    if (_wdupenv_s(&root,&length,L"LOCALAPPDATA")==0 && root) {
        const auto path=std::filesystem::path(root) / L"sattrak" / L"tutorial-v1.txt";
        std::free(root);
        return path;
    }
#else
    if (const char* root = std::getenv("XDG_STATE_HOME"))
        return std::filesystem::path(root) / "sattrak/tutorial-v1.txt";
    if (const char* root = std::getenv("HOME"))
        return std::filesystem::path(root) / ".local/state/sattrak/tutorial-v1.txt";
#endif
    return {};
}
inline bool tutorialSeen(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string status;
    return (file >> status) && (status == "completed" || status == "skipped");
}
inline bool saveTutorialState(const std::filesystem::path& path, bool completed) {
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path);
    file << (completed ? "completed\n" : "skipped\n");
    file.close();
    return bool(file);
}
}
