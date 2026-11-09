#pragma once
#include "gui/gl_util.hpp"
#include "gui/sky_catalog.hpp"

namespace gui {
struct SkySettings {
    bool stars = true, lines = false, labels = false;
    float magnitudeLimit = 6, brightness = 1;
};
// Drawn before scene geometry, without writing depth. Earth and spacecraft
// naturally occlude stars, constellation figures and labels in every view.
class SkyRenderer {
public:
    bool init(const std::string& folder);
    void draw(const Mat4& rotationProjection, int width, int height, const SkySettings& settings);
    void cleanup();
    const SkyCatalog& catalog() const { return m_catalog; }
    const std::string& error() const { return m_error; }
private:
    SkyCatalog m_catalog;
    std::string m_error;
    ShaderProgram m_shader;
    unsigned int m_vao[3]{}, m_vbo[3]{};
    int m_starCount=0, m_lineCount=0;
};
} // namespace gui
