#pragma once
namespace gui {
inline constexpr const char* solarVertex=R"(#version 330 core
out vec2 screen;
void main() {
    screen=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;
    gl_Position=vec4(screen,1.0,1.0);
}
)";
inline constexpr const char* solarFragment=R"(#version 330 core
in vec2 screen;
out vec4 color;
uniform vec3 uForward,uRight,uUp,uSunDir,uEye;
uniform float uTanHalfFov,uAspect,uRadius,uGlare;
void main() {
    vec3 ray=normalize(uForward+(uRight*screen.x*uAspect+uUp*screen.y)*uTanHalfFov);
    float angle=atan(length(cross(ray,uSunDir)),dot(ray,uSunDir));
    float pixel=max(fwidth(angle),0.00001);
    float disc=1.0-smoothstep(uRadius-pixel,uRadius+pixel,angle);
    float limb=sqrt(max(0.0,1.0-pow(angle/uRadius,2.0)));
    float glare=exp(-angle/(uRadius*5.0))*.20+exp(-angle/(uRadius*1.4))*.48;
    // Earth's limb hides the disc and suppresses glare during an eclipse
    float earthRadius=asin(clamp(12.0/max(length(uEye),12.00001),0.0,1.0));
    float separation=acos(clamp(dot(normalize(-uEye),uSunDir),-1.0,1.0));
    float visible=smoothstep(earthRadius-uRadius,earthRadius+uRadius,separation);
    float radiance=disc*(.72+.28*limb)+glare*uGlare*visible;
    color=vec4(vec3(1.0,.965,.90)*radiance,1.0);
}
)";
}
