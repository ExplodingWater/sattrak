#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace gui {

class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    bool init(const std::filesystem::path& musicFolder);
    void update(double wallTimeSeconds);
    void previous();
    void togglePause();
    void next();
    void setVolumePercent(int percent);

    bool hasTracks() const;
    bool isPlaying() const;
    int volumePercent() const;
    double startDelaySeconds() const;
    const std::string& displayText() const;
    const std::string& errorText() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace gui
