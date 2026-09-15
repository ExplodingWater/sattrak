#include "gui/developer_console.hpp"
#include "gui/radio_commands.hpp"
#include "imgui.h"
#include <algorithm>
namespace gui {
void DeveloperConsole::execute(MusicPlayer& player,const std::string& text){
    RadioCommand command;std::string message;lines.push_back("> "+text);
    if(!parseRadioCommand(text,command,message)){lines.push_back("Error: "+message);return;}
    if(command.name=="help")lines.push_back("radio.play 01 [--volume 0..2] [--endat seconds|MM:SS|HH:MM:SS]\nNumbers (01, 02, ...) or quoted filenames from assets/radio only.\nradio.stop | radio.mute | radio.unmute | radio.status | clear\nDefault volume: 0.1. Invalid/out-of-range end times are ignored.\nPlayback fades in over 5 seconds. Mute takes precedence.");
    else if(command.name=="clear")lines.clear();
    else if(command.name=="radio.play"){
        auto path=std::filesystem::u8path(command.file);player.playRadioFile(path,command.volume,command.endAt,message);lines.push_back(message);
    }else if(command.name=="radio.stop"){player.stopRadio();lines.push_back("Radio stopped. Next automatic clip in 15-30 minutes if enabled.");}
    else if(command.name=="radio.mute"){player.setRadioMuted(true);lines.push_back("Radio communications muted.");}
    else if(command.name=="radio.unmute"){player.setRadioMuted(false);lines.push_back("Radio communications unmuted.");}
    else if(command.name=="radio.status")lines.push_back(player.radioStatus()+(player.radioMuted()?" | MUTED":" | audible if playing"));
}
void DeveloperConsole::draw(MusicPlayer& player){
    if(ImGui::IsKeyPressed(ImGuiKey_F1,false)){open=!open;focus=open;}
    if(!open)return;
    auto size=ImGui::GetIO().DisplaySize;
    const bool resized=size.x!=viewportWidth||size.y!=viewportHeight;
    viewportWidth=size.x;viewportHeight=size.y;
    ImGui::SetNextWindowSize(ImVec2(std::min(760.f,size.x-24),std::min(360.f,size.y-60)),resized?ImGuiCond_Always:ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(size.x*.5f,45),resized?ImGuiCond_Always:ImGuiCond_Appearing,ImVec2(.5f,0));
    ImGui::SetNextWindowBgAlpha(.98f);
    if(focus)ImGui::SetNextWindowFocus();
    if(ImGui::Begin("Console",&open)){
        if(lines.size()>100)lines.erase(lines.begin(),lines.end()-100);
        ImGui::BeginChild("Output",ImVec2(0,-ImGui::GetFrameHeightWithSpacing()),true);
        for(const auto& line:lines)ImGui::TextWrapped("%s",line.c_str());
        if(focus)ImGui::SetScrollHereY(1);ImGui::EndChild();
        if(focus){ImGui::SetKeyboardFocusHere();focus=false;}
        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputText("##Command",input,sizeof(input),ImGuiInputTextFlags_EnterReturnsTrue)){
            execute(player,input);input[0]=0;focus=true;
        }
    }ImGui::End();
}
}
