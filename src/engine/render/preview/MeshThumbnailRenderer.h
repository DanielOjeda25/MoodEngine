#pragma once

// Renderer de miniaturas 3D de meshes para el editor (F2H80). Renderiza el
// modelo real (todos sus submeshes con sus materiales) a un FBO LDR pequeño y
// CACHEA la textura por mesh — se renderiza una sola vez (lazy) y se reusa.
//
// Reusa el shader PBR + IBL del motor, igual que MaterialPreviewRenderer, pero:
//   - dibuja un mesh arbitrario, no la esfera de preview de materiales;
//   - encuadra la cámara al AABB del mesh (bounding sphere + ángulo 3/4);
//   - cachea una textura persistente por meshId (grilla de cards SFM-style);
//   - NO anima (una grilla de N miniaturas rotando distrae + es cara).
//
// Uso desde el editor (hay contexto GL en el frame):
//   m_thumbs = std::make_unique<MeshThumbnailRenderer>();
//   m_thumbs->setIblTextures(irr, pre, brdf);
//   GLuint tex = m_thumbs->thumbnailFor(meshId, *assetManager);
//   ImGui::ImageButton("##t", (ImTextureID)(uintptr_t)tex, {96, 96});

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <unordered_map>

namespace Mood {

class AssetManager;
class IMesh;
class IShader;
class ITexture;
struct MaterialAsset;
class OpenGLCubemapTexture;
class OpenGLFramebuffer;
class OpenGLSSBO;

class MeshThumbnailRenderer {
public:
    /// @brief F2H80: primitivas CSG que se previsualizan en el modal "+ Crear
    ///        Entidad". El renderer construye el brush + su mesh una sola vez
    ///        y cachea el thumbnail (igual que los meshes del proyecto).
    enum class PrimitiveKind {
        Plane, Quad, Box, Cylinder, Sphere, Cone,
        Capsule, Pyramid, Wedge, PrismTri, PrismHex
    };

    /// @brief Crea shaders + SSBOs vacíos (Forward+ bindings 2/3/4). Los FBO
    ///        se crean lazy, uno por mesh, en `thumbnailFor`.
    explicit MeshThumbnailRenderer(u32 size = 128);
    ~MeshThumbnailRenderer();

    MeshThumbnailRenderer(const MeshThumbnailRenderer&) = delete;
    MeshThumbnailRenderer& operator=(const MeshThumbnailRenderer&) = delete;

    /// @brief Inyecta el IBL compartido del SceneRenderer (non-owning). Si
    ///        alguno es null cae al ambient escalar (uIblEnabled=0).
    void setIblTextures(OpenGLCubemapTexture* irradiance,
                        OpenGLCubemapTexture* prefilter,
                        ITexture* brdfLut);

    /// @brief Textura (color attachment) de la miniatura del mesh. La
    ///        renderiza la primera vez y la cachea; devuelve 0 si no se pudo
    ///        (mesh inválido / sin submeshes / FBO inválido).
    GLuint thumbnailFor(u32 meshId, AssetManager& assets);  // meshId = MeshAssetId

    /// @brief Miniatura de una primitiva CSG (construye el brush + mesh una vez
    ///        y cachea). Devuelve 0 si no se pudo.
    GLuint thumbnailForPrimitive(PrimitiveKind kind, AssetManager& assets);

    /// @brief Invalida toda la cache (ej. al re-importar assets) o un mesh.
    void clear();
    void invalidate(u32 meshId);

    u32 size() const { return m_size; }

private:
    // Setup PBR común (compartido por el path de mesh y el de primitiva).
    void clearAndSetGlState();
    void setCamera(const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                   const glm::mat4& model);
    void bindInvariantState();
    void bindMaterial(const MaterialAsset* mat, AssetManager& assets);

    void renderMeshToBoundFbo(u32 meshId, AssetManager& assets);
    void renderRawMeshToBoundFbo(IMesh* mesh, const glm::vec3& aabbMin,
                                 const glm::vec3& aabbMax, AssetManager& assets);

    u32 m_size = 0;

    std::unique_ptr<IShader>    m_pbrShader;
    std::unique_ptr<OpenGLSSBO> m_pointLightsSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightTilesSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightIndicesSsbo;

    // IBL inyectado (no owned).
    OpenGLCubemapTexture* m_iblIrradiance = nullptr;
    OpenGLCubemapTexture* m_iblPrefilter  = nullptr;
    ITexture*             m_iblBrdfLut     = nullptr;

    // Cache: meshId -> FBO con la miniatura ya renderizada (textura persistente).
    std::unordered_map<u32, std::unique_ptr<OpenGLFramebuffer>> m_cache;
    // Cache de primitivas, keyed por PrimitiveKind (int).
    std::unordered_map<int, std::unique_ptr<OpenGLFramebuffer>> m_primCache;
};

} // namespace Mood
