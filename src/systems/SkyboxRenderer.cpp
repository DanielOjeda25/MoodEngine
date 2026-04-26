#include "systems/SkyboxRenderer.h"

#include "core/Log.h"
#include "engine/render/IShader.h"
#include "engine/render/opengl/OpenGLCubemapTexture.h"
#include "engine/render/opengl/OpenGLShader.h"

#include <glm/mat3x3.hpp>

#include <array>
#include <filesystem>

namespace Mood {

namespace {

// Cubo unitario [-1,+1]^3 con SOLO posiciones (sin uv/normal/color).
// 36 vertices (6 caras x 2 tris x 3 verts), winding CW visto desde
// adentro — con back-face culling el cubo se ve solo desde dentro,
// que es lo que queremos para el sky (la camara siempre esta dentro).
//
// Nota: en realidad para el skybox conviene desactivar culling antes de
// dibujar y reactivarlo despues. Aca dejamos los winding en CCW visto
// desde fuera (igual que el cubo de geometria normal); el draw del
// SkyboxRenderer hace `glDisable(GL_CULL_FACE)` durante el draw para
// evitar tener que pensar en orientaciones.
constexpr float k_cubeVerts[] = {
    // -X
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    // +X
     1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
    // -Z
    -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    // +Z
     1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
    // +Y (top)
    -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    // -Y (bottom)
    -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
};

} // namespace

SkyboxRenderer::SkyboxRenderer(const std::string& cubemapDirFs) {
    const std::filesystem::path dir(cubemapDirFs);
    const std::array<std::string, 6> paths = {
        (dir / "px.png").string(),
        (dir / "nx.png").string(),
        (dir / "py.png").string(),
        (dir / "ny.png").string(),
        (dir / "pz.png").string(),
        (dir / "nz.png").string(),
    };
    m_cubemap = std::make_unique<OpenGLCubemapTexture>(paths);

    m_shader = std::make_unique<OpenGLShader>(
        "shaders/skybox.vert", "shaders/skybox.frag");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_cubeVerts), k_cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);

    m_vertexCount = static_cast<u32>(sizeof(k_cubeVerts) / (3 * sizeof(float)));

    Log::render()->info("SkyboxRenderer inicializado (cubemap='{}', {} verts)",
                         cubemapDirFs, m_vertexCount);
}

SkyboxRenderer::~SkyboxRenderer() {
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

void SkyboxRenderer::draw(const glm::mat4& view, const glm::mat4& projection) {
    // Anular la translacion de la matriz view para que el sky no se mueva
    // con la camara, solo rote.
    const glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));

    m_shader->bind();
    m_shader->setMat4("uViewNoTranslation", viewNoTrans);
    m_shader->setMat4("uProjection",         projection);
    m_shader->setInt("uSkybox", 0);
    m_cubemap->bind(0);

    // Depth test debe estar LEQUAL (no LESS) para que el sky con depth=1
    // no se descarte vs el clear de depth=1. El render pipeline del
    // motor lo deja en LEQUAL en `OpenGLRenderer::init`.
    GLint prevDepthFunc = 0;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
    glDepthFunc(GL_LEQUAL);

    // Sin face culling: el cubo se ve desde dentro y no quiero pensar en
    // winding orders.
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    if (cullEnabled) glDisable(GL_CULL_FACE);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertexCount));
    glBindVertexArray(0);

    if (cullEnabled) glEnable(GL_CULL_FACE);
    glDepthFunc(prevDepthFunc);
}

} // namespace Mood
