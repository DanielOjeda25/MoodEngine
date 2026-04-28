#pragma once

// RmlUi `RenderInterface` (Hito 20). Implementacion contra OpenGL: cada
// `CompiledGeometryHandle` mantiene VAO/VBO/EBO propios; cada
// `TextureHandle` un GLuint. El shader UI vive en
// `shaders/ui.{vert,frag}`. Bind/setup lo hace UiLayer::render antes de
// invocar `Rml::Context::Render`.
//
// Limitaciones conocidas (no-goals del Hito 20):
// - Sin clip masks (RmlUi `EnableClipMask`/`RenderToClipMask`).
// - Sin layers/composite (RmlUi `PushLayer`/`CompositeLayers`).
// - Sin filtros / shaders custom (`CompileFilter`/`CompileShader`).
// El default no-op de la base RenderInterface cubre esos casos.

#include "core/Types.h"

#include <RmlUi/Core/RenderInterface.h>
#include <glad/gl.h>

#include <memory>
#include <unordered_map>

namespace Mood {

class IShader;

class RmlRenderer : public Rml::RenderInterface {
public:
    RmlRenderer();
    ~RmlRenderer() override;

    /// @brief Llamar antes de `Rml::Context::Render()`. Setea el shader,
    ///        viewport size, alpha blending, y deshabilita depth test
    ///        (UI siempre encima). Reseta el GL state que tocamos.
    void beginFrame(u32 viewportWidth, u32 viewportHeight);

    /// @brief Restaura el GL state que `beginFrame` modifico (depth test
    ///        on, blending off, scissor off). Llamar despues de
    ///        `Rml::Context::Render()`.
    void endFrame();

    // --- Geometria ---
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    // --- Texturas ---
    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                    const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                        Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    // --- Scissor ---
    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

private:
    struct CompiledMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
    };

    std::unique_ptr<IShader> m_shader;
    Rml::CompiledGeometryHandle m_nextGeometryHandle = 1;
    Rml::TextureHandle          m_nextTextureHandle  = 1;

    std::unordered_map<Rml::CompiledGeometryHandle, CompiledMesh> m_meshes;
    std::unordered_map<Rml::TextureHandle, GLuint>                m_textures;

    u32 m_viewportWidth  = 1280;
    u32 m_viewportHeight = 720;
    bool m_inFrame = false;
    u32  m_drawCallsThisFrame = 0;
    bool m_loggedFirstDraw = false; // diagnostico Hito 20 Bloque 2
};

} // namespace Mood
