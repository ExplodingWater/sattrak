#include "gui/satellite_model.hpp"
#include <fstream>
#include <cstring>
#include <cstdint>
#include <algorithm>

namespace gui {
bool SatelliteModel::load(const std::string& path) {
    cleanup();
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() > 64*1024*1024) return false;
    in.seekg(0);
    auto read = [&](auto& value) { return bool(in.read(reinterpret_cast<char*>(&value), sizeof(value))); };
    char magic[8]; uint32_t count = 0;
    if (!in.read(magic,8) || std::memcmp(magic,"AEROMDL1",8) || !read(count) || !read(radius)
        || count == 0 || count > 128 || !std::isfinite(radius) || radius <= 0 || radius > 10000) return false;
    struct Pending { ModelPart part; std::vector<Vertex> vertices; std::vector<unsigned int> indices; };
    std::vector<Pending> pending(count);
    size_t total = 0;
    static_assert(sizeof(Vertex) == 8*sizeof(float));
    static_assert(sizeof(unsigned int) == sizeof(uint32_t));
    for (auto& p : pending) {
        uint32_t nv = 0, ni = 0;
        if (!read(p.part.color) || !read(p.part.roughness) || !read(nv) || !read(ni)) return false;
        if (nv == 0 || nv > 1000000 || ni == 0 || ni > 3000000 || ni % 3) return false;
        total += size_t(nv)*sizeof(Vertex) + size_t(ni)*sizeof(unsigned int);
        if (total > 64*1024*1024) return false;
        for (float c : p.part.color) if (!std::isfinite(c) || c < 0 || c > 1) return false;
        if (!std::isfinite(p.part.roughness) || p.part.roughness < 0 || p.part.roughness > 1) return false;
        p.vertices.resize(nv); p.indices.resize(ni);
        if (!in.read(reinterpret_cast<char*>(p.vertices.data()), nv*sizeof(Vertex))
            || !in.read(reinterpret_cast<char*>(p.indices.data()), ni*sizeof(unsigned int))) return false;
        for (auto index : p.indices) if (index >= nv) return false;
        for (const auto& v : p.vertices) {
            const float values[] = {v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v};
            for (float f : values) if (!std::isfinite(f)) return false;
            if (std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z) > radius+0.01f) return false;
        }
    }
    if (in.peek() != std::char_traits<char>::eof()) return false;
    // Validate the complete file before allocating any GPU resources.
    for (auto& p : pending) {
        for (size_t i=0; i<p.indices.size(); i+=3) {
            auto point = [&](unsigned j) { const auto& v=p.vertices[j]; return aero::math::Vec3(v.x,v.y,v.z); };
            auto a=point(p.indices[i]), b=point(p.indices[i+1]), c=point(p.indices[i+2]);
            auto n=(b-a).cross(c-a); double area=n.norm()*0.5;
            if(area>1e-10) facets.push_back({(a+b+c)/3.0,n.normalized(),area});
        }
        p.part.mesh.init(p.vertices,p.indices);
        parts.push_back(p.part);
    }
    return true;
}
void SatelliteModel::draw(const ShaderProgram& shader) const {
    shader.setInt("uUseTexture",0);
    for (const auto& p : parts) {
        shader.setVec4("uColor",p.color[0],p.color[1],p.color[2],p.color[3]);
        shader.setFloat("uRoughness",p.roughness);
        p.mesh.draw();
    }
    shader.setFloat("uRoughness",0.5f);
}
void SatelliteModel::cleanup() {
    for (auto& p : parts) p.mesh.cleanup();
    parts.clear(); facets.clear(); radius = 1;
}
}
