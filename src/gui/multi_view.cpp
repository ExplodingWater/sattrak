#include "gui/adcs_view.hpp"
#include "gui/numeric_widgets.hpp"
#include "gui/accessibility.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace gui {
namespace {
const char* cameraNames[]={"Free flight","Satellite orbit","Earth orbit","Chase","Telescope","Nadir","Solar wing","Ground station","Orbital plane","Sun-Earth"};
}
ADCSView::ViewCamera ADCSView::captureCamera() const {
    return {m_camPreset,m_flycamPos,m_flyForward,m_flyUp,m_arcballCenter,
        m_satOrbitYaw,m_satOrbitPitch,m_satOrbitDistance,m_arcballYaw,m_arcballPitch,m_arcballDist};
}
void ADCSView::applyCamera(const ViewCamera& c) {
    m_camPreset=c.preset;m_flycamPos=c.flyPos;m_flyForward=c.flyForward;m_flyUp=c.flyUp;
    m_arcballCenter=c.arcCenter;m_satOrbitYaw=c.satYaw;m_satOrbitPitch=c.satPitch;
    m_satOrbitDistance=c.satDistance;m_arcballYaw=c.arcYaw;m_arcballPitch=c.arcPitch;m_arcballDist=c.arcDistance;
}
void ADCSView::enableMultiView(bool enabled) {
    if(enabled==m_multiView)return;
    if(enabled) {
        if(!m_viewsInitialized) {
            for(int i=0;i<8;++i) {
                m_views[i].camera=captureCamera();
                if(i)m_views[i].camera.preset=CameraPreset((i+1)%10);
            }
            m_viewsInitialized=true;
        }
        m_views[0].camera=captureCamera();
    } else applyCamera(m_views[0].camera);
    m_multiView=enabled;
}
void ADCSView::drawMultiViewControls() {
    ImGui::SameLine();bool enabled=m_multiView;
    if(ImGui::Checkbox("Multi View",&enabled))enableMultiView(enabled);
    ImGui::BeginDisabled(!m_multiView);
    if(ImGui::Checkbox("Separate desktop windows",&m_separateViews)) {
        for(auto& view:m_views)view.closed=false;
    }
    if(m_multiView) {
        sliderIntExact("Views",&m_viewCount,1,8);
        for(int i=0;i<m_viewCount;++i) {
            ImGui::PushID(600+i);
            int preset=int(m_views[i].camera.preset);
            char label[24];snprintf(label,sizeof(label),"View %d",i+1);
            ImGui::SetNextItemWidth(170);
            if(ImGui::Combo(label,&preset,cameraNames,10)) {
                m_views[i].camera.preset=CameraPreset(preset);m_views[i].closed=false;
            }
            ImGui::PopID();
        }
        if(ImGui::Button("Reopen closed views"))for(auto& view:m_views)view.closed=false;
        ImGui::TextWrapped("Each view has its own camera. Drag or use WASD over a view to control it. Disabling Multi View keeps View 1.");
    }
    ImGui::EndDisabled();
}
void ADCSView::renderViewTexture(MultiView& v,int width,int height) {
    width=std::clamp(width,32,2560);height=std::clamp(height,32,1600);
    if(!v.framebuffer) {
        glGenFramebuffers(1,&v.framebuffer);glGenTextures(1,&v.texture);glGenRenderbuffers(1,&v.depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,v.framebuffer);
    if(width!=v.width || height!=v.height) {
        v.width=width;v.height=height;
        glBindTexture(GL_TEXTURE_2D,v.texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,v.texture,0);
        glBindRenderbuffer(GL_RENDERBUFFER,v.depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,v.depth);
    }
    glDisable(GL_SCISSOR_TEST);glClearColor(.005f,.01f,.025f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    applyCamera(v.camera);
    render3D(width,height);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
void ADCSView::moveViewCamera(MultiView& v,double dx,double dy,bool right,double wheel,GLFWwindow* window,float dt,bool keyboard) {
    applyCamera(v.camera);
    if(right) {if(m_camPreset!=CameraPreset::FreeFlycam)enterFlycam();rotateFlycam(dx,dy);}
    else if(m_camPreset==CameraPreset::SatelliteOrbit)rotateSatelliteOrbit(float(dx),float(dy));
    else if(m_camPreset==CameraPreset::FreeOrbitArcball) {
        m_arcballYaw+=float(dx)*m_sensitivity;m_arcballPitch+=float(dy)*m_sensitivity;
        m_arcballPitch=std::clamp(m_arcballPitch,-1.5f,1.5f);
    }
    const double scroll=wheel*m_scrollSensitivity;
    if(m_camPreset==CameraPreset::SatelliteOrbit)m_satOrbitDistance=std::clamp(m_satOrbitDistance*float(std::exp(-scroll*.12)),.25f,200000.0f);
    if(m_camPreset==CameraPreset::FreeOrbitArcball)m_arcballDist=std::clamp(m_arcballDist*float(std::exp(-scroll*.12)),12.02f,100000.0f);
    if(m_camPreset==CameraPreset::FreeFlycam)m_flycamPos+=m_flyForward*scroll*(m_realScale?m_userSpeedKmS*(12.0/6378.137)*.1:2.5);
    if(keyboard && accessibility::keyboardControlsAllowed(true) && !m_actions.capturing()) {
        auto key=[&](int code){return glfwGetKey(window,code)==GLFW_PRESS?1.0:0.0;};
        const double f=key(GLFW_KEY_W)-key(GLFW_KEY_S),r=key(GLFW_KEY_D)-key(GLFW_KEY_A),u=key(GLFW_KEY_E)-key(GLFW_KEY_Q);
        if(!ImGui::GetIO().WantTextInput&&!ImGui::GetIO().KeyCtrl&&!ImGui::GetIO().KeyAlt&&!m_actions.capturing()&&!ImGui::IsAnyItemActive()&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))
            lookWithArrows(key(GLFW_KEY_RIGHT)-key(GLFW_KEY_LEFT),key(GLFW_KEY_UP)-key(GLFW_KEY_DOWN),dt);
        if(f||r||u)moveFlycam(f,r,u,key(GLFW_KEY_LEFT_SHIFT)!=0,dt);
    }
    v.camera=captureCamera();
}
void ADCSView::closeDesktopViews() {
    for(auto& v:m_views)if(v.window) {
        glfwMakeContextCurrent(v.window);
        if(v.desktopFramebuffer)glDeleteFramebuffers(1,&v.desktopFramebuffer);
        v.desktopFramebuffer=0;glfwDestroyWindow(v.window);v.window=nullptr;v.dragging=false;
    }
    if(m_mainWindow)glfwMakeContextCurrent(m_mainWindow);
}
void ADCSView::cleanupViews() {
    closeDesktopViews();
    for(auto& v:m_views) {
        if(v.framebuffer)glDeleteFramebuffers(1,&v.framebuffer);
        if(v.texture)glDeleteTextures(1,&v.texture);
        if(v.depth)glDeleteRenderbuffers(1,&v.depth);
        v.framebuffer=v.texture=v.depth=0;
    }
    m_mainWindow=nullptr;
}
void ADCSView::renderViews(GLFWwindow* mainWindow,float dt) {
    m_mainWindow=mainWindow;
    int width,height;glfwGetFramebufferSize(mainWindow,&width,&height);
    if(!m_multiView) {
        closeDesktopViews();handleInput(mainWindow,dt);render3D(width,height);drawFlightOverlay();return;
    }
    if(!m_separateViews)closeDesktopViews();
    m_renderingMulti=true;
    auto firstEye=m_cameraEye,firstForward=m_cameraForward,firstUp=m_cameraUp;
    auto& io=ImGui::GetIO();
    for(int i=0;i<8;++i) {
        auto& v=m_views[i];
        if(i>=m_viewCount || v.closed) {
            if(v.window){glfwMakeContextCurrent(v.window);if(v.desktopFramebuffer)glDeleteFramebuffers(1,&v.desktopFramebuffer);v.desktopFramebuffer=0;glfwDestroyWindow(v.window);v.window=nullptr;glfwMakeContextCurrent(mainWindow);}
            continue;
        }
        char label[100];snprintf(label,sizeof(label),"View %d | %s",i+1,cameraNames[int(v.camera.preset)]);
        if(m_separateViews) {
            if(!v.window) {
                glfwWindowHint(GLFW_VISIBLE,glfwGetWindowAttrib(mainWindow,GLFW_VISIBLE));
                v.window=glfwCreateWindow(720,480,label,nullptr,mainWindow);
                if(!v.window){m_separateViews=false;break;}
                glfwSetWindowPos(v.window,60+i*35,80+i*35);
                glfwSetWindowUserPointer(v.window,&v);
                glfwSetScrollCallback(v.window,[](GLFWwindow* w,double,double y){static_cast<MultiView*>(glfwGetWindowUserPointer(w))->scroll+=y;});
            }
            if(glfwWindowShouldClose(v.window)){v.closed=true;continue;}
            glfwSetWindowTitle(v.window,label);
            int w,h;glfwGetFramebufferSize(v.window,&w,&h);if(w<=0||h<=0)continue;
            renderViewTexture(v,w,h);
            const bool focus=glfwGetWindowAttrib(v.window,GLFW_FOCUSED)!=0;
            double x,y;glfwGetCursorPos(v.window,&x,&y);
            const bool right=glfwGetMouseButton(v.window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS;
            const bool drag=right||glfwGetMouseButton(v.window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS;
            if(focus)moveViewCamera(v,drag&&v.dragging?x-v.mouseX:0,drag&&v.dragging?y-v.mouseY:0,right,v.scroll,v.window,dt,true);
            v.scroll=0;v.dragging=drag&&focus;v.mouseX=x;v.mouseY=y;
            glFlush();glfwMakeContextCurrent(v.window);glfwSwapInterval(0);
            if(!v.desktopFramebuffer)glGenFramebuffers(1,&v.desktopFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,v.desktopFramebuffer);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,v.texture,0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
            glDisable(GL_SCISSOR_TEST);glBlitFramebuffer(0,0,v.width,v.height,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
            glfwSwapBuffers(v.window);glfwMakeContextCurrent(mainWindow);
        } else {
            const int cols=m_viewCount<=2?m_viewCount:m_viewCount<=4?2:3;
            const int rows=(m_viewCount+cols-1)/cols;
            const float left=io.DisplaySize.x>1100?380:10;
            const float available=std::max(240.0f,io.DisplaySize.x-2*left);
            ImGui::SetNextWindowPos(ImVec2(left+(i%cols)*available/cols,45+(i/cols)*(io.DisplaySize.y-55)/rows),ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(available/cols-5,(io.DisplaySize.y-55)/rows-5),ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_Border,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,4);
            char windowLabel[120];snprintf(windowLabel,sizeof(windowLabel),"%s###MultiView%d",label,i);
            if(ImGui::Begin(windowLabel,nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextWrapped("3D spacecraft view: %s. F10 returns keyboard control to View 1.", cameraNames[int(v.camera.preset)]);
                auto size=ImGui::GetContentRegionAvail();
                renderViewTexture(v,int(size.x*io.DisplayFramebufferScale.x),int(size.y*io.DisplayFramebufferScale.y));
                ImGui::Image((ImTextureID)(intptr_t)v.texture,size,ImVec2(0,1),ImVec2(1,0));
                const bool hovered=ImGui::IsItemHovered();
                const bool dragging=hovered&&(ImGui::IsMouseDown(0)||ImGui::IsMouseDown(1));
                const bool keyboardTarget = accessibility::flightKeyboardFocus ? i==0 : hovered;
                if(hovered || keyboardTarget)moveViewCamera(v,dragging?io.MouseDelta.x:0,dragging?io.MouseDelta.y:0,ImGui::IsMouseDown(1),hovered?io.MouseWheel:0,mainWindow,dt,keyboardTarget);
            }
            ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
        }
        if(i==0){firstEye=m_cameraEye;firstForward=m_cameraForward;firstUp=m_cameraUp;}
    }
    m_cameraEye=firstEye;m_cameraForward=firstForward;m_cameraUp=firstUp;
    m_renderingMulti=false;applyCamera(m_views[0].camera);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);
}
}
