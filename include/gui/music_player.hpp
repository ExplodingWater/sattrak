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
    bool initRadio(const std::filesystem::path& folder);
    void setRadioEnabled(bool enabled);
    bool radioEnabled() const;
    bool radioPlaying() const;
    float radioGain() const;
    double secondsUntilRadio() const;
    const std::string& radioStatus() const;
    void playRadioNow();
    void stopRadio();
    void update(double wallTimeSeconds);
    void previous();
    void togglePause();
    void next();
    void setVolumePercent(int percent);
    double durationSeconds() const;
    double elapsedSeconds() const;
    void seekSeconds(double seconds);

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
