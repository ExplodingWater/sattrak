// Optional integration check: requires a desktop OpenGL context, outside CTest.
#include "gui/sky_renderer.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc,char** argv) {
    if(argc<2 || argc>3)return 2;
    glfwSetErrorCallback([](int code,const char* message){std::cerr<<code<<": "<<message<<'\n';});
    if(!glfwInit())return 2;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    GLFWwindow* window=glfwCreateWindow(1024,768,"Sky renderer checks",nullptr,nullptr);
    if(!window){glfwTerminate();return 2;}
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))return 2;
    int width,height;glfwGetFramebufferSize(window,&width,&height);
    glViewport(0,0,width,height);
    gui::SkyRenderer sky;
    if(!sky.init(argv[1]))return 1;
    int failures=0;
    auto check=[&](bool value,const char* message){if(!value){std::cerr<<message<<'\n';++failures;}};
    const auto projection=gui::Mat4::perspective(1.4f,float(width)/height,.1f,1000);
    const auto vp=gui::Mat4::multiply(projection,gui::Mat4::lookAt({},gui::skyDirection(83,0),{0,1,0}));
    auto render=[&](const gui::SkySettings& settings) {
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        sky.draw(vp,width,height,settings);
        std::vector<unsigned char> pixels(size_t(width)*height*3);
        glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        check(glGetError()==GL_NO_ERROR,"Sky OpenGL error");
        return pixels;
    };
    auto lit=[](const auto& pixels){size_t count=0;for(size_t i=0;i<pixels.size();i+=3)if(pixels[i]||pixels[i+1]||pixels[i+2])++count;return count;};
    const auto empty=render({false,false,false,6,1});
    check(lit(empty)==0,"All sky layers disabled must draw nothing");
    const auto faint=render({true,false,false,6,1});
    const auto bright=render({true,false,false,2,1});
    check(lit(faint)>lit(bright) && lit(bright)>0,"Limiting magnitude must filter faint stars");
    check(lit(render({false,true,false,6,1}))>100,"Independent constellation lines");
    check(lit(render({false,false,true,6,1}))>100,"Independent constellation names");
    // Blending and depth state must survive a sky pass, including non-default
    // alpha factors; otherwise subsequent Earth/spacecraft rendering can break.
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
    glDisable(GL_PROGRAM_POINT_SIZE);glDepthMask(GL_TRUE);
    glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA);
    glBlendEquationSeparate(GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT);
    sky.draw(vp,width,height,{true,true,true,6,1});
    GLboolean depthWrite;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    GLint alphaSource,equation;glGetIntegerv(GL_BLEND_SRC_ALPHA,&alphaSource);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equation);
    check(glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_BLEND)&&
        !glIsEnabled(GL_PROGRAM_POINT_SIZE)&&depthWrite&&alphaSource==GL_DST_ALPHA&&equation==GL_FUNC_REVERSE_SUBTRACT,"Restore scene OpenGL state");
    const auto capture=render({true,true,true,6,1.3f});
    if(argc==3) {
        std::ofstream out(argv[2],std::ios::binary);
        out<<"P6\n"<<width<<' '<<height<<"\n255\n";
        for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(capture.data()+size_t(y)*width*3),width*3);
        check(bool(out),"Write sky preview");
    }
    std::cout<<"Sky check: "<<sky.catalog().stars.size()<<" stars, "<<lit(faint)<<" lit pixels at magnitude 6, "
             <<lit(bright)<<" at magnitude 2; "<<failures<<" failures.\n";
    sky.cleanup();glfwDestroyWindow(window);glfwTerminate();return failures?1:0;
}
