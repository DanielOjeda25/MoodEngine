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

#include <filesystem>
#include <memory>
#include <unordered_map>

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

    /// @brief F3H15: directorio donde persistir las miniaturas de
    ///        materiales. Tipicamente `<projectRoot>/.cache/thumbs/`.
    ///        Path vacio = cache disco off (solo memoria, comportamiento
    ///        F2H81). El renderer crea el directorio al primer store si
    ///        no existe. Llamar al cargar/cerrar proyecto.
    void setDiskCacheRoot(std::filesystem::path cacheRoot);

    /// @brief Renderiza la esfera con el material indicado al FBO
    ///        interno. Restaura el FBO y viewport originales antes
    ///        de devolver. El caller obtiene la textura color con
    ///        `outputTextureId()`.
    void renderPreview(const MaterialAsset& mat, AssetManager& assets);

    /// @brief GLuint del color attachment. `(ImTextureID)(uintptr_t)id`
    ///        para usar con `ImGui::Image`.
    GLuint outputTextureId() const;

    /// @brief F2H81: miniatura ESTÁTICA de un material (esfera a ángulo fijo) en
    ///        un FBO cacheado por materialId (render-once). Para el grid del
    ///        Asset Browser (tab Materiales). Devuelve 0 si falla.
    GLuint thumbnail(u32 materialId, AssetManager& assets);  // materialId = MaterialAssetId

    /// @brief F3H16: miniatura grande (`kLargePreviewSize` px) para el
    ///        tooltip ampliado del Asset Browser. Cache memoria + disco
    ///        separados del thumb normal — filename incluye el size.
    GLuint thumbnailLarge(u32 materialId, AssetManager& assets);

    /// @brief F3H16: size de la miniatura grande. Hardcoded por ahora.
    static constexpr u32 kLargePreviewSize = 384u;

    /// @brief Invalida la cache de miniaturas (llamar al recargar/editar
    ///        materiales — si no, las miniaturas quedan stale).
    void clearThumbnailCache();

    u32 width() const { return m_width; }
    u32 height() const { return m_height; }

private:
    /// Renderiza la esfera con `mat` al FBO YA bindeado (viewport seteado),
    /// rotada `angleRad` sobre Y. No toca el FBO (lo maneja el caller).
    void renderSphereToBoundFbo(const MaterialAsset& mat, AssetManager& assets,
                                f32 angleRad);

    u32 m_width = 0;
    u32 m_height = 0;

    std::unique_ptr<OpenGLFramebuffer> m_fb;
    std::unique_ptr<IShader>           m_pbrShader;
    std::unique_ptr<IShader>           m_bgShader;     // F3H15: fondo gradient
    GLuint                              m_dummyVao = 0; // F3H15: VAO empty para fullscreen draw

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

    // F2H81: cache de miniaturas por materialId (FBO con textura persistente).
    std::unordered_map<u32, std::unique_ptr<OpenGLFramebuffer>> m_thumbCache;
    // F3H16: cache paralela para los thumbs grandes (kLargePreviewSize).
    std::unordered_map<u32, std::unique_ptr<OpenGLFramebuffer>> m_largeCache;

    // F3H15: cache disco. Vacio = off. Cuando seteado, `thumbnail`
    // intenta `tryLoad` antes de rendear y `store` despues.
    std::filesystem::path m_diskCacheRoot;

    /// @brief F3H16: helper interno load-or-render. Reusable por
    ///        thumbnail (m_width/m_height del FBO principal) y
    ///        thumbnailLarge (kLargePreviewSize).
    GLuint loadOrRenderMatThumb(u32 materialId, AssetManager& assets,
                                  u32 size,
                                  std::unordered_map<u32,
                                      std::unique_ptr<OpenGLFramebuffer>>& cache);
};

} // namespace Mood
