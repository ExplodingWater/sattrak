#pragma once
#include <memory>
namespace gui {
class AlarmAudio {
public:
    AlarmAudio();
    ~AlarmAudio();
    bool setActive(bool active);
    bool failed() const;
    void setGain(float gain);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
