#include "gui/music_player.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc,char** argv) {
    if(argc!=2)return 1;
    gui::MusicPlayer player;
    if(!player.initRadio(argv[1]))return 2;
    if(player.secondsUntilRadio()<2700||player.secondsUntilRadio()>7200)return 3;
    // Advance the scheduler without waiting in real time; decode each installed clip.
    for(int i=0;i<6;++i){double now=double(i)*20;
        player.update(now);player.playRadioNow();
        if(!player.radioPlaying()||player.radioGain()!=0)return 4;
        player.update(now+2.5);if(std::abs(player.radioGain()-.05f)>1e-6)return 5;
        player.update(now+6);player.setVolumePercent(200);
        if(std::abs(player.radioGain()-.1f)>1e-6)return 6;
        std::cout<<player.radioStatus()<<" | fades to 10%\n";
        // The audio engine runs asynchronously; allow streaming startup before changing files.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        player.stopRadio();if(player.radioPlaying()||player.secondsUntilRadio()<2700||player.secondsUntilRadio()>7200)return 7;
    }
    player.setRadioEnabled(false);player.update(1e6);if(player.radioPlaying())return 8;
    player.setRadioEnabled(true);if(player.secondsUntilRadio()<2700)return 9;
    std::cout<<"Radio scheduling, fades, fixed gain and disable passed.\n";
}
