#pragma once

// Renderer de preview de animaciones (F2H81). Renderiza un mesh skinneado (el
// NPC de referencia) **posado por un clip en un tiempo dado** a un FBO LDR, para
// mostrar la animación reproduciéndose en el editor (Asset Browser → tab
// Animations). A diferencia de MeshThumbnailRenderer (miniatura estática
// cacheada), este renderiza **cada frame** (la pose cambia con el tiempo) —
// patrón del MaterialPreviewRenderer (1 FBO, render por frame, no cache).
//
// Reusa el shader skinneado del motor (pbr_skinned.vert + pbr.frag) + IBL.
// La evaluación de pose es pura (AnimationClip + Skeleton headers): bind del
// clip al esqueleto (cacheado por clip), evaluate → local pose → skinning
// matrices → uBoneMatrices[].
//
// Uso:
//   m_animPreview->setIblTextures(irr, pre, brdf);
//   GLuint tex = m_animPreview->renderClip(npcMeshId, clipId, timeSec, assets);
//   ImGui::Image((ImTextureID)(uintptr_t)tex, {256,256}, {0,1}, {1,0});

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Mood {

class AssetManager;
class IShader;
class ITexture;
struct MaterialAsset;
class OpenGLCubemapTexture;
class OpenGLFramebuffer;
class OpenGLSSBO;

class AnimationPreviewRenderer {
public:
    explicit AnimationPreviewRenderer(u32 size = 256);
    ~AnimationPreviewRenderer();

    AnimationPreviewRenderer(const AnimationPreviewRenderer&) = delete;
    AnimationPreviewRenderer& operator=(const AnimationPreviewRenderer&) = delete;

    void setIblTextures(OpenGLCubemapTexture* irradiance,
                        OpenGLCubemapTexture* prefilter,
                        ITexture* brdfLut);

    /// @brief Frame EN VIVO: renderiza `meshId` posado por `clipId` en `timeSec`
    ///        al FBO compartido (se re-renderiza por frame — para la card con el
    ///        mouse encima). Devuelve 0 si falla.
    GLuint renderClip(u32 meshId, u32 clipId, f32 timeSec, AssetManager& assets);

    /// @brief Miniatura ESTÁTICA: renderiza el clip en una pose representativa
    ///        (~33% de su duración) a un FBO cacheado por clip (render-once).
    ///        Para las cards que NO tienen el mouse encima. Devuelve 0 si falla.
    GLuint staticThumbnail(u32 meshId, u32 clipId, AssetManager& assets);

    GLuint outputTextureId() const;
    u32 size() const { return m_size; }

private:
    void bindInvariantState();
    void bindMaterial(const MaterialAsset* mat, AssetManager& assets);
    /// Renderiza la pose al FBO ya bindeado (viewport ya seteado). Limpia el
    /// FBO siempre; devuelve false si el mesh/clip no sirven (queda el fondo).
    bool renderPoseToBoundFbo(u32 meshId, u32 clipId, f32 timeSec, AssetManager& assets);

    u32 m_size = 0;

    std::unique_ptr<OpenGLFramebuffer> m_fb;  // FBO compartido del frame en vivo
    std::unique_ptr<IShader>           m_skinnedShader;
    std::unique_ptr<OpenGLSSBO>        m_pointLightsSsbo;
    std::unique_ptr<OpenGLSSBO>        m_lightTilesSsbo;
    std::unique_ptr<OpenGLSSBO>        m_lightIndicesSsbo;

    OpenGLCubemapTexture* m_iblIrradiance = nullptr;
    OpenGLCubemapTexture* m_iblPrefilter  = nullptr;
    ITexture*             m_iblBrdfLut     = nullptr;

    // Cache del remap clip→esqueleto por clipId (bindClipToSkeleton es O(tracks)).
    std::unordered_map<u32, std::vector<int>> m_remapCache;
    // Cache de miniaturas estáticas por clipId (FBO con textura persistente).
    std::unordered_map<u32, std::unique_ptr<OpenGLFramebuffer>> m_staticCache;
    // Scratch reusados por frame (evita realloc).
    std::vector<glm::mat4> m_localPose;
    std::vector<glm::mat4> m_skinning;
};

} // namespace Mood
