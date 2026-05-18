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
#include "engine/render/rhi/IFramebuffer.h"  // F2H28: orthoFb devuelve IFramebuffer*.
#include "engine/scene/core/Entity.h"  // F2H28: lista de selected en orto.

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>  // F2H28 Bloque E: panOffset en renderOrthoView.
#include <glm/vec3.hpp>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
class OpenGLOitFramebuffer;  // F2H64
class OpenGLInstanceBuffer;
class OpenGLParticleRenderer;
class OpenGLSSBO;
class BloomPass;
class ColorGradingPass;
class SSRPass;  // F2H61
class PostProcessPass;
class SSAOPass;
class Scene;
class ShaderGraphCache;  // F2H62 Bloque E
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

    /// @brief F2H28: render wireframe ortografico al FBO indexado por
    ///        `idx` (0=Top, 1=Front, 2=Side; convencion arbitraria del
    ///        caller). Pinta brushes + meshes estaticos con color flat
    ///        celeste (paleta GMod) sobre fondo gris claro; entidades
    ///        en `selectedEntities` se redibujan en naranja Valve para
    ///        que la seleccion sea visible. Sin shadows / IBL /
    ///        particles / postFX — el costo extra por viewport es
    ///        solo la geometria.
    ///
    ///        Llamar DESPUES de `endFrame()` del render perspectivo
    ///        del frame, asi los `BrushComponent::meshCache` ya estan
    ///        hidratados y se pueden reusar (no se regenera la mesh
    ///        en este path).
    void renderOrthoView(Scene& scene,
                         AssetManager& assets,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         glm::vec2 panOffset,
                         f32 worldHeight,
                         f32 snapStep,
                         u32 panelWidth,
                         u32 panelHeight,
                         usize idx,
                         const std::vector<Entity>& selectedEntities = {});

    /// @brief Acceso al FBO de la vista orto `idx` para que el panel
    ///        `OrthoViewportPanel` lo muestre con `ImGui::Image`. La
    ///        firma usa `IFramebuffer*` para que el panel (capa
    ///        editor/) no necesite conocer el tipo concreto OpenGL.
    ///        Devuelve nullptr si idx fuera de [0, 2].
    IFramebuffer* orthoFb(usize idx);

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

    // F2H55: bloom params (leidos por el panel Environment + tests).
    bool bloomEnabled()   const { return m_bloomEnabled; }
    f32  bloomThreshold() const { return m_bloomThreshold; }
    f32  bloomIntensity() const { return m_bloomIntensity; }
    f32  bloomRadius()    const { return m_bloomRadius; }

    // F2H56: SSAO params.
    bool ssaoEnabled()   const { return m_ssaoEnabled; }
    f32  ssaoRadius()    const { return m_ssaoRadius; }
    f32  ssaoIntensity() const { return m_ssaoIntensity; }

    // F2H61: SSR params (leidos por el panel Environment + tests).
    bool ssrEnabled()    const { return m_ssrEnabled; }
    u32  ssrMaxSteps()   const { return m_ssrMaxSteps; }
    f32  ssrThickness()  const { return m_ssrThickness; }
    f32  ssrStepSize()   const { return m_ssrStepSize; }
    f32  ssrIntensity()  const { return m_ssrIntensity; }

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

    /// @brief F2H21: handles del IBL del SceneRenderer para que el
    ///        `MaterialPreviewRenderer` use el mismo prefilter /
    ///        irradiance / brdfLut sin duplicar la carga de disco.
    ///        Pueden devolver nullptr si la carga del IBL fallo en el
    ///        ctor (try/catch silencioso) — en ese caso el preview cae
    ///        a ambient escalar.
    OpenGLCubemapTexture* iblIrradiance() const { return m_iblIrradiance.get(); }
    OpenGLCubemapTexture* iblPrefilter() const  { return m_iblPrefilter.get(); }
    ITexture*             iblBrdfLut() const    { return m_iblBrdfLut.get(); }

private:
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<OpenGLFramebuffer> m_sceneFb;     // HDR RGBA16F
    std::unique_ptr<OpenGLFramebuffer> m_ssaoOutFb;   // HDR RGBA16F (F2H56): scene * AO modulation
    std::unique_ptr<OpenGLFramebuffer> m_bloomFb;     // HDR RGBA16F (F2H55): scene + bloom contribution antes del post-process
    std::unique_ptr<OpenGLFramebuffer> m_colorGradingFb;// HDR RGBA16F (F2H58): post bloom + color grading aplicado, antes del post-process
    std::unique_ptr<OpenGLFramebuffer> m_ssrFb;       // HDR RGBA16F (F2H61): scene color + SSR reflection aditivo
    std::unique_ptr<OpenGLFramebuffer> m_backbufferCopyFb; // HDR RGBA16F (F2H63): snapshot del color del FB pre-translucent. Los frag shaders translucent samplean esto via uBackbufferCopy con offset para refraccion screen-space.
    std::unique_ptr<OpenGLOitFramebuffer> m_oitAccumFb; // F2H64: OIT Weighted Blended. Dos color attachments (accumColor RGBA16F + revealage R16F) + depth compartido con m_sceneFb. El composite pass mezcla con el scene color.
    std::unique_ptr<OpenGLFramebuffer> m_viewportFb;  // LDR RGBA8
    std::unique_ptr<SSAOPass>          m_ssaoPass;    // F2H56
    std::unique_ptr<BloomPass>         m_bloomPass;   // F2H55
    std::unique_ptr<ColorGradingPass>  m_colorGradingPass; // F2H58
    std::unique_ptr<SSRPass>           m_ssrPass;     // F2H61
    std::unique_ptr<PostProcessPass>   m_postProcess;

    std::unique_ptr<IShader> m_pbrShader;          // PBR estatico (no instanced)
    std::unique_ptr<IShader> m_pbrInstancedShader; // F2H4: variante instanced
    std::unique_ptr<IShader> m_pbrSkinnedShader;   // PBR + LBS skinning
    std::unique_ptr<IShader> m_wireframeOrthoShader;  // F2H28: vista orto.
    std::unique_ptr<IShader> m_grid2dShader;          // F2H28 Bloque E: grid orto.
    std::unique_ptr<IShader> m_oitCompositeShader;    // F2H64: fullscreen tri que mezcla m_oitAccumFb sobre el scene FB.
    unsigned int            m_oitCompositeVao = 0;   // F2H64: VAO trivial para el composite (gl_VertexID gen).

    // F2H62 Bloque E: cache de programas GL compilados desde .moodshader.
    // Cuando un material tiene shaderGraphPath no vacio, el render pide
    // un IShader* al cache; si hay hit lo usa en lugar del PBR estandar.
    // Fallback transparente al PBR si load/compile falla.
    std::unique_ptr<ShaderGraphCache> m_shaderGraphCache;

    // F2H62 Bloque E: tiempo acumulado desde el primer frame para el
    // uniform uTime de los shader graphs (necesario para shaders
    // animados tipo water/hologram). Actualizado en renderScene.
    f32 m_currentTime = 0.0f;

    // F2H28: 3 FBOs LDR para Top/Front/Side del workspace "Editor de
    // mapas". Resize a demanda en renderOrthoView. Vacios cuando el
    // workspace activo no es el de mapas (no se renderizan).
    std::array<std::unique_ptr<OpenGLFramebuffer>, 3> m_orthoFbs;

    // F2H28 Bloque E: VAO trivial para el grid2d shader (fullscreen
    // triangle sin VBO; el .vert genera posiciones desde gl_VertexID,
    // pero OpenGL Core Profile exige un VAO bound para glDrawArrays).
    // Tipo `unsigned int` para no leakear glad al header publico.
    unsigned int m_grid2dVao = 0;

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

    // F2H55: bloom params (poblados por applyEnvironmentFromScene en Bloque C).
    bool m_bloomEnabled    = true;
    f32  m_bloomThreshold  = 1.0f;
    f32  m_bloomIntensity  = 0.6f;
    f32  m_bloomRadius     = 1.0f;

    // F2H56: SSAO params (poblados por applyEnvironmentFromScene).
    bool m_ssaoEnabled   = true;
    f32  m_ssaoRadius    = 0.5f;
    f32  m_ssaoIntensity = 1.0f;

    // F2H61: SSR params (poblados por applyEnvironmentFromScene).
    // Default OFF -- iter1 F2H60: "todo deberia estar desactivado por defecto".
    bool m_ssrEnabled   = false;
    u32  m_ssrMaxSteps  = 32u;
    f32  m_ssrThickness = 0.5f;
    f32  m_ssrStepSize  = 0.2f;
    f32  m_ssrIntensity = 0.5f;

    // F2H58: Color Grading params (poblados por applyEnvironmentFromScene).
    bool        m_colorGradingEnabled   = false;
    std::string m_colorGradingLutPath   = "";
    f32         m_colorGradingIntensity = 1.0f;
    // F2H60: CSM quality params (poblados por applyEnvironmentFromScene).
    // El "enabled" de las sombras se decide per-light via
    // LightComponent::castShadows -- aca solo viven los knobs de calidad.
    u32  m_csmCascadeCount = 4;
    f32  m_csmSplitLambda  = 0.5f;
    // GL handle de la LUT efectiva del frame actual. Resuelto en
    // renderScene a partir del path (loadTexture via AssetManager) o
    // identity si path vacio. 0 = no listo / fallback a skip del pass.
    u32         m_currentLutTextureId   = 0;
    // Identity LUT sintetizada lazy. 256x16 RGBA8 lineal con lookup(c) == c.
    // Se crea la primera vez que se necesita y se reusa entre frames.
    u32         m_identityLutId         = 0;

    /// @brief Sintetiza la LUT identidad 256x16 RGBA8 lineal. Idempotente:
    ///        si `m_identityLutId != 0` no recrea. Llamada lazy desde
    ///        renderScene cuando el color grading necesita LUT y no hay
    ///        post-process pass o LUT custom.
    void synthesizeIdentityLut();

    /// @brief F2H64: ensure-or-resize del `m_oitAccumFb`. Lo crea si no
    ///        existe, lo resize si cambió el panel size, y re-attachea el
    ///        depth del scene FB (que puede haberse recreado en su resize).
    ///        Idempotente — seguro llamarlo cada frame.
    void ensureOitFb(u32 width, u32 height);

    /// @brief F2H63: blittea el color attachment de `m_sceneFb` a
    ///        `m_backbufferCopyFb`. Llamar justo antes del translucent
    ///        pass para que los shaders translucent puedan samplear el
    ///        color "pre-translucent" via `uBackbufferCopy` (unit 7) y
    ///        hacer refraccion screen-space. Asume que ambos FBOs tienen
    ///        el mismo size — el resize esta sincronizado en renderScene.
    ///        Devuelve `true` si el blit se ejecuto, `false` si los FBOs
    ///        no estan listos.
    bool blitSceneToBackbufferCopy();

    // Diagnostico one-shot.
    bool m_shadowEnabledLastFrame = false;
    u32  m_lastLoggedPointLightCount = 9999u;
    // F2H60 polish iter5: diagnostico de applyEnvironmentFromScene.
    // Loguea solo cuando los valores cambian (slider movido) para evitar
    // spam por frame.
    bool m_lastLoggedEnvFound        = false;
    u32  m_lastLoggedCsmCascadeCount = 0;
    f32  m_lastLoggedCsmLambda       = -1.0f;

    // F2H42 -> F2H60: shadow map cache por hash de escena ELIMINADO con
    // CSM. CSM depende de la matriz de camara que cambia casi siempre,
    // asi que cachear entre frames no aporta. recordCsm corre cada frame.
};

} // namespace Mood
