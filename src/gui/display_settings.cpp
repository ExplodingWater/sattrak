#include "gui/display_settings.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <utility>

namespace gui {
DisplaySettings::DisplaySettings(GLFWwindow* window,bool remember):window(window),remember(remember) {
    baseStyle=ImGui::GetStyle();
    if(remember){std::ifstream scale("local/ui_scale.txt");int value;if(scale>>value)uiScale=std::clamp(value,75,200);}
    refreshMonitors();syncWindow();
    int best=0;
    for(int i=0;i<int(monitors.size());++i) {
        int x,y;glfwGetMonitorPos(monitors[i].handle,&x,&y);
        int overlap=std::max(0,std::min(x+monitors[i].width,current.x+current.width)-std::max(x,current.x))*
                    std::max(0,std::min(y+monitors[i].height,current.y+current.height)-std::max(y,current.y));
        if(overlap>best){best=overlap;selected=i;}
    }
    width=current.width;height=current.height;
    if(remember) {
        std::ifstream file("local/display_settings.txt");int savedMode,savedWidth,savedHeight,savedMonitor;
        if(file>>savedMode>>savedWidth>>savedHeight>>savedMonitor) {
            mode=savedMode;width=savedWidth;height=savedHeight;
            selected=std::clamp(savedMonitor,0,std::max(0,int(monitors.size())-1));
            std::string reason;
            if(valid(reason)){queued=true;restoringSaved=true;}
            else {mode=2;width=current.width;height=current.height;message="Saved display settings do not fit this monitor. Using a window.";}
        }
    }
}
void DisplaySettings::syncWindow() {
    if(current.mode!=2)return;
    glfwGetWindowPos(window,&current.x,&current.y);
    glfwGetWindowSize(window,&current.width,&current.height);
    current.maximized=glfwGetWindowAttrib(window,GLFW_MAXIMIZED)==GLFW_TRUE;
    int best=0;
    for(const auto& monitor:monitors) {
        int x,y;glfwGetMonitorPos(monitor.handle,&x,&y);
        int area=std::max(0,std::min(x+monitor.width,current.x+current.width)-std::max(x,current.x))*
                 std::max(0,std::min(y+monitor.height,current.y+current.height)-std::max(y,current.y));
        if(area>best){best=area;current.monitor=monitor.handle;}
    }
}
void DisplaySettings::refreshMonitors() {
    int count=0;auto** handles=glfwGetMonitors(&count);
    std::vector<Monitor> refreshed;
    for(int i=0;i<count;++i) {
        auto old=std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.handle==handles[i];});
        // Keep the desktop mode captured before entering exclusive fullscreen.
        if(old!=monitors.end())refreshed.push_back(*old);
        else if(auto* video=glfwGetVideoMode(handles[i])) {
            int maxWidth=video->width,maxHeight=video->height,modeCount=0;
            auto* modes=glfwGetVideoModes(handles[i],&modeCount);
            for(int j=0;j<modeCount;++j)if(modes[j].width*modes[j].height>maxWidth*maxHeight){maxWidth=modes[j].width;maxHeight=modes[j].height;}
            refreshed.push_back({handles[i],video->width,video->height,video->refreshRate,glfwGetMonitorName(handles[i]),maxWidth,maxHeight});
        }
    }
    monitors=std::move(refreshed);
    selected=std::clamp(selected,0,std::max(0,int(monitors.size())-1));
}
bool DisplaySettings::valid(std::string& reason) const {
    if(monitors.empty()||selected<0||selected>=int(monitors.size())){reason="No monitor is available.";return false;}
    if(mode<0||mode>2){reason="Choose a display mode.";return false;}
    const auto& monitor=monitors[selected];
    if(mode==1)return true;
    if(width<640||height<360||width>monitor.maxWidth||height>monitor.maxHeight) {
        reason="Enter at least 640 x 360, no larger than the selected monitor's maximum reported resolution.";return false;
    }
    if(mode==0) {
        int count;auto* modes=glfwGetVideoModes(monitor.handle,&count);
        for(int i=0;i<count;++i)if(modes[i].width==width&&modes[i].height==height)return true;
        reason="This fullscreen resolution is not supported by the monitor. Choose a preset or use Windowed.";return false;
    }
    return true;
}
bool DisplaySettings::apply(const State& state) {
    auto found=std::find_if(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.handle==state.monitor;});
    if(found==monitors.end())return false;
    const auto& monitor=*found;
    glfwGetError(nullptr); // Clear an earlier, unrelated GLFW error.
    glfwRestoreWindow(window);
    glfwSetWindowAttrib(window,GLFW_DECORATED,state.mode==2?GLFW_TRUE:GLFW_FALSE);
    glfwSetWindowAttrib(window,GLFW_RESIZABLE,state.mode==2?GLFW_TRUE:GLFW_FALSE);
    if(state.mode==0) {
        glfwSetWindowMonitor(window,monitor.handle,0,0,state.width,state.height,monitor.refresh);
    } else if(state.mode==1) {
        int x,y;glfwGetMonitorPos(monitor.handle,&x,&y);
        glfwSetWindowMonitor(window,nullptr,x,y,monitor.width,monitor.height,GLFW_DONT_CARE);
    } else {
        int x,y,w,h,left,top,right,bottom;
        glfwGetMonitorWorkarea(monitor.handle,&x,&y,&w,&h);
        glfwGetWindowFrameSize(window,&left,&top,&right,&bottom);
        int actualWidth=std::min(state.width,std::max(1,w-left-right));
        int actualHeight=std::min(state.height,std::max(1,h-top-bottom));
        int px=std::clamp(state.x,x+left,std::max(x+left,x+w-actualWidth-right));
        int py=std::clamp(state.y,y+top,std::max(y+top,y+h-actualHeight-bottom));
        glfwSetWindowMonitor(window,nullptr,px,py,actualWidth,actualHeight,GLFW_DONT_CARE);
        if(state.maximized)glfwMaximizeWindow(window);
    }
    const char* error=nullptr;
    if(glfwGetError(&error)!=GLFW_NO_ERROR){message=error?error:"Display change failed.";return false;}
    current=state;
    glfwGetWindowSize(window,&current.width,&current.height);
    glfwSwapInterval(1);
    return true;
}
void DisplaySettings::update() {
    if(uiScale!=appliedScale) {
        uiScale=std::clamp(uiScale,75,200);appliedScale=uiScale;
        ImGui::GetStyle()=baseStyle;
        ImGui::GetStyle().ScaleAllSizes(uiScale/100.0f);
        ImGui::GetStyle().FontScaleMain=uiScale/100.0f;
        if(remember){std::error_code ec;std::filesystem::create_directories("local",ec);std::ofstream("local/ui_scale.txt")<<uiScale;}
    }
    refreshMonitors();
    if(monitors.empty())return;
    if(current.monitor&&std::none_of(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.handle==current.monitor;})) {
        previous=current;previous.mode=2;previous.monitor=monitors.front().handle;reverting=true;
    }
    if(confirming&&(glfwGetTime()>=deadline||glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS))reverting=true;
    if(reverting) {
        if(std::none_of(monitors.begin(),monitors.end(),[&](const Monitor& m){return m.handle==previous.monitor;})) {
            previous.monitor=monitors.front().handle;previous.mode=2;
        }
        apply(previous);mode=current.mode;width=current.width;height=current.height;
        confirming=false;reverting=false;queued=false;restoringSaved=false;message="Previous display settings restored.";
    } else if(queued) {
        queued=false;std::string reason;
        if(!valid(reason)){message=reason;return;}
        syncWindow();previous=current;
        if(!previous.monitor)previous.monitor=monitors[selected].handle;
        auto next=current;next.mode=mode;next.width=width;next.height=height;
        next.monitor=monitors[selected].handle;next.maximized=false;
        if(!apply(next)){reverting=true;return;}
        confirming=!restoringSaved;restoringSaved=false;deadline=glfwGetTime()+15;message.clear();
    }
}
bool DisplaySettings::save() const {
    if(!remember)return true;
    std::error_code error;std::filesystem::create_directories("local",error);
    std::ofstream file("local/display_settings.txt");
    file<<current.mode<<' '<<current.width<<' '<<current.height<<' '<<selected<<'\n';
    file.flush();return bool(file);
}
void DisplaySettings::draw() {
    ImGui::SeparatorText("Interface");
    ImGui::SliderInt("UI scale",&uiScale,75,200,"%d%%",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(90*ImGui::GetStyle().FontScaleMain);
    if(ImGui::InputInt("Scale percent",&uiScale,0,0))uiScale=std::clamp(uiScale,75,200);
    if(ImGui::Button("Reset UI scale"))uiScale=100;
    ImGui::SeparatorText("Display");
    ImGui::BeginDisabled(confirming||queued||monitors.empty());
    const char* monitorName=monitors.empty()?"No monitor":monitors[selected].name.c_str();
    if(ImGui::BeginCombo("Monitor",monitorName)) {
        for(int i=0;i<int(monitors.size());++i){ImGui::PushID(i);if(ImGui::Selectable(monitors[i].name.c_str(),selected==i))selected=i;ImGui::PopID();}
        ImGui::EndCombo();
    }
    const char* names[]={"Fullscreen","Fullscreen Windowed","Windowed"};
    ImGui::Combo("Window mode",&mode,names,3);
    if(!monitors.empty()) {
        const auto& monitor=monitors[selected];
        ImGui::Text("Desktop: %d x %d",monitor.width,monitor.height);
        ImGui::TextDisabled("Maximum reported: %d x %d",monitor.maxWidth,monitor.maxHeight);
        ImGui::BeginDisabled(mode==1);
        std::set<std::pair<int,int>> presets;
        const std::pair<int,int> common[]={{640,360},{800,600},{1024,768},{1280,720},{1280,800},{1366,768},{1600,900},{1920,1080},{1920,1200},{2560,1440},{2560,1600},{3440,1440},{3840,2160}};
        int count;auto* modes=glfwGetVideoModes(monitor.handle,&count);
        auto add=[&](int w,int h){if(w>=640&&h>=360&&w<=monitor.maxWidth&&h<=monitor.maxHeight)presets.emplace(w,h);};
        for(int i=0;i<count;++i)add(modes[i].width,modes[i].height);
        if(mode!=0){for(auto [w,h]:common)add(w,h);add(monitor.width,monitor.height);}
        std::string preview=std::to_string(mode==1?monitor.width:width)+" x "+std::to_string(mode==1?monitor.height:height);
        if(ImGui::BeginCombo("Resolution preset",preview.c_str())) {
            for(auto [w,h]:presets){auto label=std::to_string(w)+" x "+std::to_string(h);if(ImGui::Selectable(label.c_str(),w==width&&h==height)){width=w;height=h;}}
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(90);ImGui::InputInt("##display_width",&width,0,0);
        ImGui::SameLine();ImGui::TextUnformatted("x");ImGui::SameLine();
        ImGui::SetNextItemWidth(90);ImGui::InputInt("##display_height",&height,0,0);
        ImGui::SameLine();ImGui::TextUnformatted("Resolution");
        ImGui::EndDisabled();
        if(mode==1)ImGui::TextWrapped("Fullscreen Windowed fills the monitor at its desktop resolution without changing the display mode.");
        if(mode==2)ImGui::TextWrapped("Windowed sizes are fitted inside the desktop work area, leaving room for the title bar and taskbar.");
        int fw,fh;glfwGetFramebufferSize(window,&fw,&fh);
        ImGui::TextDisabled("Drawable framebuffer: %d x %d",fw,fh);
        ImGui::TextWrapped("Window sizes use OS screen coordinates. High-DPI displays may render more pixels.");
    }
    std::string reason;bool okay=valid(reason);
    ImGui::BeginDisabled(!okay);
    if(ImGui::Button("Apply display settings"))queued=true;
    ImGui::EndDisabled();
    if(!okay)ImGui::TextWrapped("%s",reason.c_str());
    ImGui::EndDisabled();
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
}
void DisplaySettings::drawConfirmation() {
    if(confirming&&!confirmationOpened){ImGui::OpenPopup("Confirm display changes");confirmationOpened=true;}
    auto size=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(size.x*.5f,size.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(size.x-24,480*ImGui::GetStyle().FontScaleMain),0));
    if(ImGui::BeginPopupModal("Confirm display changes",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove)) {
        if(!confirming){ImGui::CloseCurrentPopup();confirmationOpened=false;}
        else {
            ImGui::TextWrapped("Keep the new display settings?");
            ImGui::TextColored(ImVec4(1,.72f,.15f,1),"Reverting in %d seconds",std::max(0,int(std::ceil(deadline-glfwGetTime()))));
            ImGui::Separator();
            if(ImGui::Button("Accept changes")){confirming=false;confirmationOpened=false;message=save()?"Display settings saved.":"Display applied; preferences could not be saved.";ImGui::CloseCurrentPopup();}
            ImGui::SameLine();
            if(ImGui::Button("Revert")){reverting=true;confirmationOpened=false;ImGui::CloseCurrentPopup();}
            ImGui::TextWrapped("Press Escape to revert. No response restores your previous settings automatically.");
        }
        ImGui::EndPopup();
    }
    if(!confirming)confirmationOpened=false;
}
}
