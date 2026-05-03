#pragma once

// Pipeline de render de escena reusable. Hito 21 Bloque 2: extraido de
// `EditorApplication::renderSceneToViewport` para que tanto el editor
// como `MoodPlayer` lo compartan. Es dueno de los recursos de render
// (framebuffers, shaders, passes, IBL, light grid, SSBOs); el caller
// le pasa la escena + camara + assets como parametros.
//
// Uso por frame:
//
//   m_sceneRenderer->renderScene(scene, assets, view, proj, aspect,
//                                  cameraPos, panelW, panelH);
//   // El caller puede agregar geometria al debug renderer aqui:
//   m_sceneRenderer->debugRenderer().drawAabb(...);
//   m_sceneRenderer->endFrame();  // flush debug + post-process
//
// El framebuffer final (LDR, post-tonemap) se accede con `viewportFb()`
// — es lo que el panel Viewport del editor muestra con `ImGui::Image`,
// y lo que `MoodPlayer` blittea al backbuffer.

#include "core/Types.h"
#include "engine/render/pipeline/Fog.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <memory>

namespace Mood {

class AssetManager;
class IRenderer;
class IShader;
class ITexture;
class LightGrid;
class LightSystem;
class OpenGLCubemapTexture;
class OpenGLDebugRenderer;
class OpenGLFramebuffer;
class OpenGLInstanceBuffer;
class OpenGLParticleRenderer;
class OpenGLSSBO;
class PostProcessPass;
class Scene;
class ShadowPass;
class SkyboxRenderer;
struct FrameStats;
enum class TonemapMode : i32;

class SceneRenderer {
public:
    /// @brief Inicializa todos los recursos de render: RHI, FBs (HDR
    ///        scene + LDR viewport), shaders PBR (estatico + skinneado),
    ///        debug renderer, skybox, IBL, shadow, post-process, light
    ///        grid + SSBOs. Asume que SDL + glad ya estan listos.
    SceneRenderer();
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    /// @brief Pinta la escena al framebuffer HDR (sky -> PBR -> light
    ///        grid -> sombras). Deja el FB todavia bindeado para que el
    ///        caller pueda agregar debug geometry via `debugRenderer()`
    ///        antes de cerrar el frame con `endFrame()`.
    void renderScene(Scene& scene,
                     AssetManager& assets,
                     const glm::mat4& view,
                     const glm::mat4& projection,
                     f32 aspect,
                     const glm::vec3& cameraPos,
                     u32 panelWidth,
                     u32 panelHeight);

    /// @brief Cierra el frame: flushea el debug renderer (si tiene
    ///        geometria pendiente), unbindea el scene FB, y aplica el
    ///        post-process (exposure + tonemap + gamma) al viewport FB.
    void endFrame();

    /// @brief Acceso al debug renderer entre `renderScene` y `endFrame`
    ///        para que el caller agregue lineas / AABBs / OBBs propios
    ///        (selection outline, drop highlights, F1 debug, etc.).
    OpenGLDebugRenderer& debugRenderer() { return *m_debugRenderer; }

    /// @brief El FB LDR final con post-process aplicado. Es lo que el
    ///        panel Viewport del editor muestra con `ImGui::Image`.
    OpenGLFramebuffer& viewportFb() { return *m_viewportFb; }
    OpenGLFramebuffer& sceneFb()    { return *m_sceneFb; }

    /// @brief Las matrices del ultimo frame (escritas por
    ///        `renderScene`). Las lee el overlay 2D del ViewportPanel
    ///        para proyectar posiciones de iconos / gizmos.
    glm::mat4 lastView()       const { return m_lastView; }
    glm::mat4 lastProjection() const { return m_lastProjection; }
    f32       lastAspect()     const { return m_lastAspect; }

    /// @brief Estado de Environment leido de la escena en `renderScene`.
    ///        Lo expone para que el Inspector pueda mostrar valores
    ///        actuales y para tests de round-trip.
    const FogParams& fog() const { return m_fog; }
    f32 exposure()         const { return m_exposure; }
    TonemapMode tonemap()  const { return m_tonemap; }
    f32 iblIntensity()     const { return m_iblIntensity; }

    /// @brief Resetea el cache de Environment y aplica el primer
    ///        EnvironmentComponent encontrado en la escena. Util al
    ///        cargar un proyecto para tener los valores antes del
    ///        primer render.
    void applyEnvironmentFromScene(Scene& scene);

    /// @brief F2H2: contadores per-frame del IRenderer (draw calls + tris).
    ///        Reset cada `beginFrame`. Solo cubre las draws via drawMesh
    ///        (PBR static + skinned + shadow); skybox/particles/debug NO
    ///        se cuentan.
    FrameStats frameStats() const;

private:
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<OpenGLFramebuffer> m_sceneFb;     // HDR RGBA16F
    std::unique_ptr<OpenGLFramebuffer> m_viewportFb;  // LDR RGBA8
    std::unique_ptr<PostProcessPass>   m_postProcess;

    std::unique_ptr<IShader> m_pbrShader;          // PBR estatico (no instanced)
    std::unique_ptr<IShader> m_pbrInstancedShader; // F2H4: variante instanced
    std::unique_ptr<IShader> m_pbrSkinnedShader;   // PBR + LBS skinning

    // F2H4: VBO para subir las matrices model como atributo de instancia
    // (locations 5-8) cada frame antes del draw instanced.
    std::unique_ptr<OpenGLInstanceBuffer> m_instanceBuffer;

    std::unique_ptr<OpenGLDebugRenderer> m_debugRenderer;
    std::unique_ptr<SkyboxRenderer>       m_skyboxRenderer;
    std::unique_ptr<ShadowPass>           m_shadowPass;
    std::unique_ptr<LightSystem>          m_lightSystem;
    std::unique_ptr<OpenGLParticleRenderer> m_particleRenderer;  // Hito 29 Bloque 2

    // Forward+ light grid (Hito 18). 3 SSBOs: point lights, tile data,
    // light indices. Bindings 2/3/4.
    std::unique_ptr<LightGrid>  m_lightGrid;
    std::unique_ptr<OpenGLSSBO> m_pointLightsSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightTilesSsbo;
    std::unique_ptr<OpenGLSSBO> m_lightIndicesSsbo;

    // IBL (Hito 17 Bloque 3): cae a uAmbient escalar si alguno falta.
    std::unique_ptr<OpenGLCubemapTexture> m_iblIrradiance;
    std::unique_ptr<OpenGLCubemapTexture> m_iblPrefilter;
    std::unique_ptr<ITexture>             m_iblBrdfLut;

    // Estado del frame actual (escrito por renderScene, leido por el
    // caller antes/durante endFrame).
    glm::mat4 m_lastView{1.0f};
    glm::mat4 m_lastProjection{1.0f};
    f32 m_lastAspect = 1.0f;

    // Environment cache (reset + override en applyEnvironmentFromScene).
    FogParams m_fog{};
    f32 m_exposure = 0.0f;
    TonemapMode m_tonemap;            // ACES en ctor
    f32 m_iblIntensity = 1.0f;

    // Diagnostico one-shot.
    bool m_shadowEnabledLastFrame = false;
    u32  m_lastLoggedPointLightCount = 9999u;
};

} // namespace Mood
