#include "gui/sky_renderer.hpp"
#include "stb_easy_font.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace gui {
namespace {
struct SkyVertex { float x,y,z,r,g,b,mag; };
SkyVertex vertex(aero::math::Vec3 d, float r, float g, float b, float mag=0) {
    return {float(d.x),float(d.y),float(d.z),r,g,b,mag};
}
const char* skyVertex=R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in float magnitude;
uniform mat4 uMVP;
uniform int uMode;
uniform float uLimit, uBrightness;
out vec3 tint;
out float opacity;
void main() {
    gl_Position = uMVP * vec4(position,1.0);
    gl_Position.z = gl_Position.w * 0.999999;
    tint = color;
    opacity = 1.0;
    gl_PointSize = 1.0;
    if (uMode == 0) {
        // Logarithmic display response keeps naked-eye stars legible.
        float flux = pow(10.0,-0.4*magnitude);
        gl_PointSize = clamp(1.4 + 1.8*pow(flux,0.25),1.4,7.0);
        opacity = magnitude > uLimit ? 0.0 : clamp(uBrightness*pow(flux,0.22),0.0,1.0);
    }
}
)";
const char* skyFragment=R"(#version 330 core
in vec3 tint;
in float opacity;
uniform int uMode;
out vec4 fragColor;
void main() {
    float alpha=opacity;
    if (uMode==0) {
        float r=length(gl_PointCoord*2.0-1.0);
        if(r>1.0 || alpha==0.0) discard;
        alpha*=1.0-smoothstep(0.1,1.0,r);
    }
    fragColor=vec4(tint,alpha);
}
)";
}
bool SkyRenderer::init(const std::string& folder) {
    cleanup();
    if (!m_catalog.load(folder,m_error)) { std::cerr<<m_error<<'\n';return false; }
    if (!m_shader.loadFromSource(skyVertex,skyFragment)) { m_error="Sky shader compilation failed";return false; }
    glGenVertexArrays(3,m_vao);glGenBuffers(3,m_vbo);
    for (int i=0;i<3;++i) {
        glBindVertexArray(m_vao[i]);glBindBuffer(GL_ARRAY_BUFFER,m_vbo[i]);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(SkyVertex),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(SkyVertex),reinterpret_cast<void*>(offsetof(SkyVertex,r)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(SkyVertex),reinterpret_cast<void*>(offsetof(SkyVertex,mag)));
    }
    std::vector<SkyVertex> stars, lines;
    for (const auto& star:m_catalog.stars) {
        // B-V color index: cool blue through solar white to warm orange.
        const float t=std::clamp((star.bv+.3f)/2.1f,0.f,1.f);
        const float r=t<.45f?.68f+.32f*t/.45f:1.f;
        const float g=t<.45f?.80f+.15f*t/.45f:.95f-.37f*(t-.45f)/.55f;
        const float b=t<.45f?1.f:1.f-.62f*(t-.45f)/.55f;
        stars.push_back(vertex(star.direction,r,g,b,star.magnitude));
    }
    for (const auto& segment:m_catalog.segments) {
        const double angle=std::acos(std::clamp(segment.a.dot(segment.b),-1.0,1.0));
        const int count=std::max(1,int(std::ceil(angle/.035))); // <=2 degree great-circle pieces
        auto direction=[&](int n) {
            const double t=double(n)/count;
            return (segment.a*(1-t)+segment.b*t).normalized();
        };
        for(int i=0;i<count;++i) {
            lines.push_back(vertex(direction(i),.16f,.29f,.39f));
            lines.push_back(vertex(direction(i+1),.16f,.29f,.39f));
        }
    }
    m_starCount=int(stars.size());m_lineCount=int(lines.size());
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo[0]);glBufferData(GL_ARRAY_BUFFER,stars.size()*sizeof(SkyVertex),stars.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo[1]);glBufferData(GL_ARRAY_BUFFER,lines.size()*sizeof(SkyVertex),lines.data(),GL_STATIC_DRAW);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    return true;
}
void SkyRenderer::draw(const Mat4& vp,int width,int height,const SkySettings& settings) {
    if (!m_shader.id || width<=0 || height<=0 || (!settings.stars&&!settings.lines&&!settings.labels)) return;
    // Preserve state for the scene renderer, including independent alpha blending.
    const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),point=glIsEnabled(GL_PROGRAM_POINT_SIZE),cull=glIsEnabled(GL_CULL_FACE);
    GLboolean writeDepth;glGetBooleanv(GL_DEPTH_WRITEMASK,&writeDepth);
    GLint program,vao,buffer,srcRGB,dstRGB,srcAlpha,dstAlpha,eqRGB,eqAlpha;
    glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    m_shader.use();m_shader.setMat4("uMVP",vp);
    m_shader.setFloat("uLimit",settings.magnitudeLimit);m_shader.setFloat("uBrightness",settings.brightness);
    if(settings.lines) {
        m_shader.setInt("uMode",1);glBindVertexArray(m_vao[1]);glDrawArrays(GL_LINES,0,m_lineCount);
    }
    if(settings.stars) {
        m_shader.setInt("uMode",0);glBindVertexArray(m_vao[0]);glDrawArrays(GL_POINTS,0,m_starCount);
    }
    if(settings.labels) {
        struct FontVertex { float x,y,z; unsigned char color[4]; };
        std::array<FontVertex,4096> glyphs{};
        std::vector<SkyVertex> text;
        std::vector<std::array<float,4>> occupied;
        const float scale=std::clamp(height/850.f,1.f,2.f);
        for (const auto& label:m_catalog.labels) {
            const auto& d=label.direction;
            auto clip=[&](int row){return vp.m[row]*d.x+vp.m[4+row]*d.y+vp.m[8+row]*d.z;};
            const double w=clip(3);if(w<=0)continue;
            const float x=float((clip(0)/w+1)*width*.5),y=float((1-clip(1)/w)*height*.5);
            std::string name=label.name;
            const float tw=stb_easy_font_width(name.data())*scale,th=12*scale;
            const std::array<float,4> box{x-tw*.5f,y-th*.5f,x+tw*.5f,y+th*.5f};
            if(box[0]<8 || box[1]<8 || box[2]>width-8 || box[3]>height-8)continue;
            bool overlaps=false;
            for(const auto& old:occupied)if(box[0]<old[2]+8 && box[2]>old[0]-8 && box[1]<old[3]+8 && box[3]>old[1]-8){overlaps=true;break;}
            if(overlaps)continue;
            occupied.push_back(box);
            const int quads=stb_easy_font_print(0,0,name.data(),nullptr,glyphs.data(),int(sizeof(glyphs)));
            for(int q=0;q<quads;++q)for(int i:{0,1,2,0,2,3}) {
                const auto& g=glyphs[q*4+i];
                text.push_back(vertex({2*(box[0]+g.x*scale)/width-1,1-2*(box[1]+g.y*scale)/height,0},.43f,.58f,.68f));
            }
        }
        m_shader.setInt("uMode",2);m_shader.setMat4("uMVP",Mat4::identity());
        glBindVertexArray(m_vao[2]);glBindBuffer(GL_ARRAY_BUFFER,m_vbo[2]);
        glBufferData(GL_ARRAY_BUFFER,text.size()*sizeof(SkyVertex),text.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(text.size()));
    }
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glUseProgram(program);
    glDepthMask(writeDepth);if(depth)glEnable(GL_DEPTH_TEST);
    if(cull)glEnable(GL_CULL_FACE);
    if(!blend)glDisable(GL_BLEND);if(!point)glDisable(GL_PROGRAM_POINT_SIZE);
    glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);glBlendEquationSeparate(eqRGB,eqAlpha);
}
void SkyRenderer::cleanup() {
    if(m_vao[0])glDeleteVertexArrays(3,m_vao);
    if(m_vbo[0])glDeleteBuffers(3,m_vbo);
    for(int i=0;i<3;++i)m_vao[i]=m_vbo[i]=0;
    m_shader.cleanup();m_starCount=m_lineCount=0;
}
} // namespace gui
