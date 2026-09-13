#pragma once
#include "gui/gl_util.hpp"
#include <array>

namespace gui {
struct ModelPart {
    Mesh mesh;
    std::array<float,4> color{};
    float roughness = 1;
    unsigned int texture = 0;
};
struct AeroFacet { aero::math::Vec3 center, normal; double area; };
class SatelliteModel {
public:
    std::vector<ModelPart> parts;
    float radius = 1;
    std::vector<AeroFacet> facets;
    // Loads our versioned, pre-baked mesh format. Does not parse untrusted GLB at runtime.
    bool load(const std::string& path);
    void draw(const ShaderProgram& shader) const;
    void cleanup();
};
}
