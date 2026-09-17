#pragma once
#include "gui/music_player.hpp"
#include <string>
#include <vector>
namespace gui {
class DeveloperConsole {
public:
    void draw(MusicPlayer& player);
private:
    bool open=false,focus=false;
    float viewportWidth=0,viewportHeight=0;
    char input[2048]{};
    std::vector<std::string> lines{"Type help for commands. radio.play accepts a track number or filename from assets/radio only. F1 opens/closes this console."};
    void execute(MusicPlayer& player,const std::string& text);
};
}
