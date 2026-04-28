#include "engine/ui/RmlRenderer.h"

#include "core/Log.h"
#include "engine/render/IShader.h"
#include "engine/render/opengl/OpenGLShader.h"

#include <stb_image.h>

#include <cstring>
#include <vector>

namespace Mood {

RmlRenderer::RmlRenderer() {
    try {
        m_shader = std::make_unique<OpenGLShader>(
            "shaders/ui.vert", "shaders/ui.frag");
    } catch (const std::exception& e) {
        Log::editor()->warn("RmlRenderer: shader UI no disponible: {}", e.what());
        m_shader.reset();
    }
}

RmlRenderer::~RmlRenderer() {
    // Borrar todos los recursos GL pendientes. RmlUi normalmente
    // llama Release{Geometry,Texture} antes de morir, pero un crash
    // o shutdown abrupto puede dejarnos handles huerfanos.
    for (auto& [_, mesh] : m_meshes) {
        if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
    }
    for (auto& [_, tex] : m_textures) {
        if (tex) glDeleteTextures(1, &tex);
    }
}

void RmlRenderer::beginFrame(u32 viewportWidth, u32 viewportHeight) {
    m_viewportWidth  = viewportWidth;
    m_viewportHeight = viewportHeight;
    m_inFrame = true;
    m_drawCallsThisFrame = 0;
    if (!m_shader) return;

    // Estado para UI 2D: alpha blending premultiplicado (RmlUi entrega
    // colores con alpha pre-multiplicado) en RGB; el alpha del FB se
    // FUERZA a 1 (ONE, ONE) para que el resultado quede totalmente
    // opaco. Sin esto, ImGui interpretaria el alpha < 1 del FB como
    // transparencia y mezclaria la UI con el panel oscuro detras.
    // Depth test off, cull off (UI 2D siempre encima).
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,  // RGB
                        GL_ONE, GL_ONE);                  // ALPHA
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    m_shader->bind();
    m_shader->setVec2("uViewportSize",
        glm::vec2(static_cast<f32>(viewportWidth),
                  static_cast<f32>(viewportHeight)));
    m_shader->setInt("uTex", 0);
}

void RmlRenderer::endFrame() {
    m_inFrame = false;
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Diagnostico: log del primer frame con draws para verificar que
    // RmlUi esta invocando RenderGeometry. Despues de eso, silencio
    // (el contador se resetea cada frame y el log condicional evita
    // spam).
    if (m_drawCallsThisFrame > 0 && !m_loggedFirstDraw) {
        Log::editor()->info(
            "RmlRenderer: primer frame con {} draw calls (viewport {}x{}).",
            m_drawCallsThisFrame, m_viewportWidth, m_viewportHeight);
        m_loggedFirstDraw = true;
    }
}

Rml::CompiledGeometryHandle RmlRenderer::CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices) {
    CompiledMesh mesh{};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Rml::Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(int)),
                 indices.data(), GL_STATIC_DRAW);
    mesh.indexCount = static_cast<GLsizei>(indices.size());

    // Layout `Rml::Vertex`: vec2 position + 4×u8 colour + vec2 uv.
    // stride = 20 bytes.
    constexpr GLsizei kStride = sizeof(Rml::Vertex);
    constexpr GLintptr kOffPos = 0;
    constexpr GLintptr kOffCol = sizeof(Rml::Vector2f);
    constexpr GLintptr kOffUv  = kOffCol + 4; // 4 bytes Colourb

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, kStride,
                           reinterpret_cast<const void*>(kOffPos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, kStride,
                           reinterpret_cast<const void*>(kOffCol));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride,
                           reinterpret_cast<const void*>(kOffUv));

    glBindVertexArray(0);

    const Rml::CompiledGeometryHandle handle = m_nextGeometryHandle++;
    m_meshes.emplace(handle, mesh);
    return handle;
}

void RmlRenderer::RenderGeometry(Rml::CompiledGeometryHandle geometry,
                                   Rml::Vector2f translation,
                                   Rml::TextureHandle texture) {
    if (!m_inFrame || !m_shader) return;
    auto it = m_meshes.find(geometry);
    if (it == m_meshes.end()) return;
    const CompiledMesh& mesh = it->second;

    m_shader->setVec2("uTranslate", glm::vec2(translation.x, translation.y));

    if (texture != 0) {
        auto tIt = m_textures.find(texture);
        if (tIt != m_textures.end()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tIt->second);
            m_shader->setInt("uHasTex", 1);
        } else {
            m_shader->setInt("uHasTex", 0);
        }
    } else {
        m_shader->setInt("uHasTex", 0);
    }

    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    ++m_drawCallsThisFrame;
}

void RmlRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    auto it = m_meshes.find(geometry);
    if (it == m_meshes.end()) return;
    if (it->second.ebo) glDeleteBuffers(1, &it->second.ebo);
    if (it->second.vbo) glDeleteBuffers(1, &it->second.vbo);
    if (it->second.vao) glDeleteVertexArrays(1, &it->second.vao);
    m_meshes.erase(it);
}

Rml::TextureHandle RmlRenderer::LoadTexture(
        Rml::Vector2i& texture_dimensions,
        const Rml::String& source) {
    // RmlUi pasa paths relativos a la "base directory" del documento. En
    // este motor el cwd es la raiz del repo y los assets viven en
    // `assets/`. Si el path no resuelve (logging), devolvemos 0 — RmlUi
    // dibuja un cuadrado magenta por defecto.
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(false); // RmlUi usa origen top-left
    unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels,
                                     STBI_rgb_alpha);
    stbi_set_flip_vertically_on_load(true);
    if (data == nullptr) {
        Log::editor()->warn("RmlRenderer: no se pudo cargar textura UI '{}'",
                             source);
        return 0;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    texture_dimensions = {w, h};
    const Rml::TextureHandle handle = m_nextTextureHandle++;
    m_textures.emplace(handle, id);
    return handle;
}

Rml::TextureHandle RmlRenderer::GenerateTexture(
        Rml::Span<const Rml::byte> source,
        Rml::Vector2i source_dimensions) {
    if (source.empty() || source_dimensions.x <= 0 || source_dimensions.y <= 0) {
        return 0;
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 source_dimensions.x, source_dimensions.y, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, source.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    const Rml::TextureHandle handle = m_nextTextureHandle++;
    m_textures.emplace(handle, id);
    return handle;
}

void RmlRenderer::ReleaseTexture(Rml::TextureHandle texture) {
    auto it = m_textures.find(texture);
    if (it == m_textures.end()) return;
    if (it->second) glDeleteTextures(1, &it->second);
    m_textures.erase(it);
}

void RmlRenderer::EnableScissorRegion(bool enable) {
    if (enable) glEnable(GL_SCISSOR_TEST);
    else        glDisable(GL_SCISSOR_TEST);
}

void RmlRenderer::SetScissorRegion(Rml::Rectanglei region) {
    // RmlUi pasa rect en pixel-space top-left. GL espera bottom-left.
    // Convertir flippeando Y: gl_y = vh - (top + height).
    const GLint x = region.Left();
    const GLint y = static_cast<GLint>(m_viewportHeight)
                  - region.Top() - region.Height();
    glScissor(x, y, region.Width(), region.Height());
}

} // namespace Mood
