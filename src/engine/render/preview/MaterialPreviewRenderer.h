#pragma once

// Renderer de preview esferico off-screen para el editor de materiales
// (F2H21). Renderiza una esfera con el material indicado a un FBO LDR
// 256x256 (default), reusando el shader PBR del motor. Sin shadow, sin
// fog, sin point lights — solo 1 directional + IBL opcional.
//
// Uso desde el editor:
//   m_preview = std::make_unique<MaterialPreviewRenderer>();
//   m_preview->setIblTextures(irradiance, prefilter, brdfLut);
//   ...
//   // cada frame que el panel es visible:
//   m_preview->renderPreview(*mat, *m_assetManager);
//   ImGui::Image((ImTextureID)(uintptr_t) m_preview->outputTextureId(),
//                {256, 256});

#include "core/Types.h"

#include <glad/gl.h>

#include <memory>

namespace Mood {

class AssetManager;
class IShader;
class ITexture;
class OpenGLCubemapTexture;
class OpenGLFramebuffer;
class OpenGLSSBO;
struct MaterialAsset;

class MaterialPreviewRenderer {
public:
    /// @brief Crea el FBO + carga shaders + crea SSBOs vacios para
    ///        compatibilidad con el shader PBR (que espera Forward+
    ///        bindings 2/3/4).
    explicit MaterialPreviewRenderer(u32 width = 256, u32 height = 256);
    ~MaterialPreviewRenderer();

    MaterialPreviewRenderer(const MaterialPreviewRenderer&) = delete;
    MaterialPreviewRenderer& operator=(const MaterialPreviewRenderer&) = delete;

    /// @brief Inyecta el IBL compartido del SceneRenderer. Si alguno
    ///        es null, el preview cae al fallback ambient escalar
    ///        (uIblEnabled=0). Non-owning — el caller maneja la vida.
    void setIblTextures(OpenGLCubemapTexture* irradiance,
                         OpenGLCubemapTexture* prefilter,
                         ITexture* brdfLut);

    /// @brief Renderiza la esfera con el material indicado al FBO
    ///        interno. Restaura el FBO y viewport originales antes
    ///        de devolver. El caller obtiene la textura color con
    ///        `outputTextureId()`.
    void renderPreview(const MaterialAsset& mat, AssetManager& assets);

    /// @brief GLuint del color attachment. `(ImTextureID)(uintptr_t)id`
    ///        para usar con `ImGui::Image`.
    GLuint outputTextureId() const;

    u32 width() const { return m_width; }
    u32 height() const { return m_height; }

private:
    u32 m_width = 0;
    u32 m_height = 0;

    std::unique_ptr<OpenGLFramebuffer> m_fb;
    std::unique_ptr<IShader>           m_pbrShader;

    // SSBOs vacios para compatibilidad con el shader PBR (Forward+
    // bindings 2/3/4). Para el preview no hay point lights — un solo
    // tile con count=0.
    std::unique_ptr<OpenGLSSBO> m_pointLightsSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightTilesSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightIndicesSsbo;

    // IBL inyectado (no owned).
    OpenGLCubemapTexture* m_iblIrradiance = nullptr;
    OpenGLCubemapTexture* m_iblPrefilter = nullptr;
    ITexture*             m_iblBrdfLut = nullptr;
};

} // namespace Mood
