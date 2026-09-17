#include "gui/music_player.hpp"
#include "gui/alarm_audio.hpp"
#include "gui/radio_commands.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc,char** argv) {
    if(argc!=2)return 1;
    gui::RadioCommand command;std::string message;
    if(!gui::parseRadioCommand("radio.play \"C:\\audio folder\\clip.mp3\" --volume 0.5 --endat 02:30",command,message)||command.file!="C:\\audio folder\\clip.mp3"||command.volume!=.5f||command.endAt!=150)return 15;
    if(!gui::parseRadioCommand("radio.play a.mp3 --endat invalid --volume 2",command,message)||command.endAt!=-1||command.volume!=2)return 16;
    if(!gui::parseRadioCommand("radio.play a.mp3 --endat --volume 0",command,message)||command.endAt!=-1||command.volume!=0)return 17;
    if(gui::parseRadioCommand("radio.play a.mp3 --volume nan",command,message)||gui::parseRadioCommand("radio.play a.mp3 --volume 2.1",command,message)||gui::parseRadioCommand("radio.play \"unclosed",command,message))return 18;
    if(!gui::parseRadioCommand("radio.play a.mp3 --endat 1:60",command,message)||command.endAt!=-1)return 19;
    gui::MusicPlayer player;
    if(!player.initRadio(argv[1]))return 2;
    if(player.secondsUntilRadio()<900||player.secondsUntilRadio()>1800)return 3;
    // Advance the scheduler without waiting in real time; decode each installed clip.
    for(int i=0;i<6;++i){double now=double(i)*20;
        player.update(now);player.playRadioNow();
        if(!player.radioPlaying()||player.radioGain()!=0)return 4;
        player.update(now+2.5);if(std::abs(player.radioGain()-.05f)>1e-6)return 5;
        player.update(now+6);player.setVolumePercent(200);
        if(std::abs(player.radioGain()-.1f)>1e-6)return 6;
        player.setRadioMuted(true);if(!player.radioMuted()||player.radioGain()!=0||!player.radioPlaying())return 10;
        player.update(now+7);if(player.radioGain()!=0)return 11;
        player.setRadioMuted(false);if(player.radioMuted()||std::abs(player.radioGain()-.1f)>1e-6)return 12;
        std::cout<<player.radioStatus()<<" | fades to 10%\n";
        // The audio engine runs asynchronously; allow streaming startup before changing files.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        player.stopRadio();if(player.radioPlaying()||player.secondsUntilRadio()<900||player.secondsUntilRadio()>1800)return 7;
    }
    player.setRadioEnabled(false);player.update(1e6);if(player.radioPlaying())return 8;
    player.setRadioEnabled(true);if(player.secondsUntilRadio()<900||player.secondsUntilRadio()>1800)return 9;
    gui::AlarmAudio alarm;if(!alarm.setActive(true)||alarm.failed())return 13;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if(!alarm.setActive(false)||!alarm.setActive(true))return 14;
    alarm.setActive(false);
    auto clip=std::filesystem::path("05 - QO-100 - Amateur satellite conversation.wav");
    player.setRadioEnabled(false);
    if(!player.playRadioFile(clip,.3f,.1,message))return 20;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));player.update(1e6+1);
    if(player.radioPlaying())return 21;
    if(!player.playRadioFile(clip,.3f,1e9,message))return 22;
    player.update(1e6+7);if(!player.radioPlaying()||std::abs(player.radioGain()-.3f)>1e-6)return 23;
    player.setRadioMuted(true);if(player.radioGain()!=0)return 24;
    player.stopRadio();player.setRadioMuted(false);
    player.setRadioMuted(true);
    for(int i=1;i<=6;++i){auto number="0"+std::to_string(i);if(!player.playRadioFile(number,.1f,-1,message)||!player.radioStatus().starts_with(number+" - "))return 25;}
    for(const auto* bad:{"../outside.wav","..\\outside.wav","C:\\outside.wav","/tmp/outside.wav","assets/radio/05 - QO-100 - Amateur satellite conversation.wav","99"})if(player.playRadioFile(bad,.1f,-1,message))return 26;
    player.stopRadio();player.setRadioMuted(false);
    player.setRadioEnabled(true);
    player.update(2e6); // Pass the scheduled deadline: this must start automatically.
    if(!player.radioPlaying())return 27;
    player.update(2e6+6);
    player.setRadioVolumePercent(40);player.setMasterVolumePercent(50);
    if(std::abs(player.radioGain()-.2f)>1e-6)return 28;
    player.setMasterVolumePercent(0);if(player.radioGain()!=0)return 29;
    player.stopRadio();if(player.radioPlaying()||player.secondsUntilRadio()<900||player.secondsUntilRadio()>1800)return 30;
    player.setMasterVolumePercent(100);player.setRadioVolumePercent(10);
    std::cout<<"Radio scheduling, fades, fixed gain and disable passed.\n";
}
