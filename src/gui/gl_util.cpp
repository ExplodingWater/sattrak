#include "gui/gl_util.hpp"
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace gui {

void Mesh::init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    indexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Positions (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);

    // Normals (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(1);

    // UVs (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (vao != 0 && indexCount > 0) {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Mesh::cleanup() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

bool ShaderProgram::loadFromSource(const char* vertexSrc, const char* fragmentSrc) {
    int success;
    char infoLog[512];

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return false;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return false;
    }

    id = glCreateProgram();
    glAttachShader(id, vertexShader);
    glAttachShader(id, fragmentShader);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(id, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void ShaderProgram::use() const {
    glUseProgram(id);
}

void ShaderProgram::setMat4(const char* name, const Mat4& mat) const {
    int loc = glGetUniformLocation(id, name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, mat.data());
    }
}

void ShaderProgram::setVec3(const char* name, float x, float y, float z) const {
    int loc = glGetUniformLocation(id, name);
    if (loc >= 0) {
        glUniform3f(loc, x, y, z);
    }
}

void ShaderProgram::setVec4(const char* name, float x, float y, float z, float w) const {
    int loc = glGetUniformLocation(id, name);
    if (loc >= 0) {
        glUniform4f(loc, x, y, z, w);
    }
}

void ShaderProgram::setInt(const char* name, int val) const {
    int loc = glGetUniformLocation(id, name);
    if (loc >= 0) {
        glUniform1i(loc, val);
    }
}

void ShaderProgram::setFloat(const char* name, float val) const {
    int loc = glGetUniformLocation(id, name);
    if (loc >= 0) {
        glUniform1f(loc, val);
    }
}

void ShaderProgram::cleanup() {
    if (id) glDeleteProgram(id);
    id = 0;
}

Mesh createSphereMesh(float radius, int rings, int sectors) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float PI = 3.14159265358979323846f;
    for (int r = 0; r <= rings; ++r) {
        float phi = float(r) / float(rings) * PI; // [0, PI]
        float v = float(r) / float(rings); // stbi loads the north-up image without flipping

        for (int s = 0; s <= sectors; ++s) {
            float theta = float(s) / float(sectors) * 2.0f * PI; // [0, 2PI]
            float u = float(s) / float(sectors);

            // Three.js spherical mapping convention
            float x = -radius * std::sin(phi) * std::cos(theta);
            float y = radius * std::cos(phi);
            float z = radius * std::sin(phi) * std::sin(theta);

            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;

            vertices.push_back({x, y, z, nx, ny, nz, u, v});
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            unsigned int first = (r * (sectors + 1)) + s;
            unsigned int second = first + sectors + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    Mesh m;
    m.init(vertices, indices);
    return m;
}

Mesh createBoxMesh(float width, float height, float depth) {
    float w = width * 0.5f;
    float h = height * 0.5f;
    float d = depth * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face (+Z)
        {-w, -h,  d,  0, 0, 1,  0, 0},
        { w, -h,  d,  0, 0, 1,  1, 0},
        { w,  h,  d,  0, 0, 1,  1, 1},
        {-w,  h,  d,  0, 0, 1,  0, 1},
        // Back face (-Z)
        { w, -h, -d,  0, 0, -1, 0, 0},
        {-w, -h, -d,  0, 0, -1, 1, 0},
        {-w,  h, -d,  0, 0, -1, 1, 1},
        { w,  h, -d,  0, 0, -1, 0, 1},
        // Left face (-X)
        {-w, -h, -d, -1, 0, 0,  0, 0},
        {-w, -h,  d, -1, 0, 0,  1, 0},
        {-w,  h,  d, -1, 0, 0,  1, 1},
        {-w,  h, -d, -1, 0, 0,  0, 1},
        // Right face (+X)
        { w, -h,  d,  1, 0, 0,  0, 0},
        { w, -h, -d,  1, 0, 0,  1, 0},
        { w,  h, -d,  1, 0, 0,  1, 1},
        { w,  h,  d,  1, 0, 0,  0, 1},
        // Top face (+Y)
        {-w,  h,  d,  0, 1, 0,  0, 0},
        { w,  h,  d,  0, 1, 0,  1, 0},
        { w,  h, -d,  0, 1, 0,  1, 1},
        {-w,  h, -d,  0, 1, 0,  0, 1},
        // Bottom face (-Y)
        {-w, -h, -d,  0, -1, 0, 0, 0},
        { w, -h, -d,  0, -1, 0, 1, 0},
        { w, -h,  d,  0, -1, 0, 1, 1},
        {-w, -h,  d,  0, -1, 0, 0, 1}
    };

    std::vector<unsigned int> indices = {
         0,  1,  2,   0,  2,  3,
         4,  5,  6,   4,  6,  7,
         8,  9, 10,   8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23
    };

    Mesh m;
    m.init(vertices, indices);
    return m;
}

Mesh createCylinderMesh(float radiusBottom, float radiusTop, float height, int slices) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float PI = 3.14159265358979323846f;
    float halfH = height * 0.5f;

    for (int i = 0; i <= slices; ++i) {
        float theta = float(i) / float(slices) * 2.0f * PI;
        float cosT = std::cos(theta);
        float sinT = std::sin(theta);
        float u = float(i) / float(slices);

        // Top ring
        vertices.push_back({radiusTop * cosT, halfH, radiusTop * sinT, cosT, 0, sinT, u, 1.0f});
        // Bottom ring
        vertices.push_back({radiusBottom * cosT, -halfH, radiusBottom * sinT, cosT, 0, sinT, u, 0.0f});
    }

    for (int i = 0; i < slices; ++i) {
        unsigned int top1 = i * 2;
        unsigned int bot1 = i * 2 + 1;
        unsigned int top2 = (i + 1) * 2;
        unsigned int bot2 = (i + 1) * 2 + 1;

        indices.push_back(top1);
        indices.push_back(bot1);
        indices.push_back(top2);

        indices.push_back(top2);
        indices.push_back(bot1);
        indices.push_back(bot2);
    }

    Mesh m;
    m.init(vertices, indices);
    return m;
}

Mesh createQuadMesh() {
    std::vector<Vertex> vertices = {
        {-1.0f, -1.0f, 0.0f,  0, 0, 1,  0.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f,  0, 0, 1,  1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f,  0, 0, 1,  1.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f,  0, 0, 1,  0.0f, 1.0f}
    };
    std::vector<unsigned int> indices = {
        0, 1, 2,  0, 2, 3
    };
    Mesh m;
    m.init(vertices, indices);
    return m;
}

unsigned int loadTextureFromFile(const std::string& path) {
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = GL_RGB;
    if (nrChannels == 1) format = GL_RED;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 4) format = GL_RGBA;

    GLint alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

unsigned int createDynamicRGBATexture(int width, int height) {
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void updateDynamicRGBATexture(unsigned int texId, int width, int height, const unsigned char* data) {
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Turbo Colormap sample (Google's high-accuracy perceptual colormap)
ColorRGB sampleTurboColormap(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    const float kRedVec4[4]   = {0.13572138f, 4.61539260f, -42.66032258f, 132.13108234f};
    const float kGreenVec4[4] = {0.09140261f, 2.19418839f, 4.84296658f, -14.18503333f};
    const float kBlueVec4[4]  = {0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f};
    const float kRedVec2[2]   = {-152.94239396f, 59.28637943f};
    const float kGreenVec2[2] = {4.27729857f, 2.82956604f};
    const float kBlueVec2[2]  = {-89.90310912f, 27.34824973f};

    float v4 = t * t * t * t;
    float v3 = t * t * t;
    float v2 = t * t;
    float v1 = t;
    float v5 = v4 * t;

    float r = kRedVec4[0] + kRedVec4[1]*v1 + kRedVec4[2]*v2 + kRedVec4[3]*v3 + kRedVec2[0]*v4 + kRedVec2[1]*v5;
    float g = kGreenVec4[0] + kGreenVec4[1]*v1 + kGreenVec4[2]*v2 + kGreenVec4[3]*v3 + kGreenVec2[0]*v4 + kGreenVec2[1]*v5;
    float b = kBlueVec4[0] + kBlueVec4[1]*v1 + kBlueVec4[2]*v2 + kBlueVec4[3]*v3 + kBlueVec2[0]*v4 + kBlueVec2[1]*v5;

    r = std::max(0.0f, std::min(1.0f, r));
    g = std::max(0.0f, std::min(1.0f, g));
    b = std::max(0.0f, std::min(1.0f, b));

    return { (unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f), (unsigned char)(b * 255.0f) };
}

ColorRGB sampleInfernoColormap(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    float r = std::min(1.0f, std::max(0.0f, 1.35f * t - 0.1f));
    float g = std::min(1.0f, std::max(0.0f, 1.25f * (t * t)));
    float b = std::min(1.0f, std::max(0.0f, 0.6f * std::sin(t * 3.14159f) + 0.3f * t));
    return { (unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f), (unsigned char)(b * 255.0f) };
}

ColorRGB sampleCoolWarmColormap(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    float r = t;
    float g = 1.0f - std::abs(t - 0.5f) * 2.0f;
    float b = 1.0f - t;
    return { (unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f), (unsigned char)(b * 255.0f) };
}

} // namespace gui
