#pragma once

// ShadowPass (Hito 16 Bloque 2, refactor F2H60): renderiza la escena
// desde la perspectiva de la luz direccional al shadow map array,
// produciendo N cascadas sampleables desde `lit.frag` con
// `sampler2DArrayShadow`.
//
// Pre-F2H60 el pase usaba un single shadow map 2D + bounding sphere
// global de la escena. F2H60 reemplaza por **Cascade Shadow Maps (CSM)**
// con 1-4 cascadas (1 = comportamiento legacy single-map). Cada cascada
// cubre un sub-frustum de la camara, dando texeles mas finos cerca y
// mas gruesos lejos.
//
// Pipeline:
//   1. `recordCsm(scene, assets, renderer, lightDir, cameraView,
//                  cameraProj, cascadeCount, lambda)`:
//      - Computa los split distances con `computeCsmSplits`.
//      - Para cada cascada i:
//        - Computa lightSpace via `computeCascadeShadowMatrices`.
//        - Bindea el layer i del shadow map array como target.
//        - Itera entidades con MeshRenderer y dibuja con `shadow_depth`.
//      - Almacena `m_lightSpaces[i]` y `m_cascadeSplits[i+1]` para que
//        el caller los suba como uniforms al lit shader.
//   2. El caller recupera `lightSpaceMatrix(i)` y `cascadeSplit(i+1)`
//      por cascada + el array texture id via `bindShadowMap(unit)`.
//   3. Lit shader samplea con `sampler2DArrayShadow` y elige la cascada
//      por depth view-space del fragment.

#include "core/Types.h"
#include "engine/render/pipeline/ShadowMath.h"  // kMaxCsmCascades

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <memory>

namespace Mood {

class IShader;
class IRenderer;
class Scene;
class AssetManager;
class OpenGLShadowMapArray;

class ShadowPass {
public:
    /// @brief Crea el shadow map array (depth FBO `size x size x N`) +
    ///        compila el shader `shadow_depth.{vert,frag}`. Por default
    ///        `cascadeCount=kMaxCsmCascades` (4); puede recrearse con
    ///        `recreate()` si cambia el setting per-scene.
    /// @throws std::runtime_error si el FBO queda incompleto o el shader
    ///         no compila.
    explicit ShadowPass(u32 shadowMapSize = 2048,
                          u32 cascadeCount = kMaxCsmCascades);
    ~ShadowPass();

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    /// @brief Recrea el shadow map array con un nuevo cascadeCount o size
    ///        (no-op si los parametros son iguales). Util cuando el dev
    ///        cambia el setting csmCascadeCount en el Inspector en
    ///        runtime. El shader no se recompila (la misma textura
    ///        sampler array soporta N <= kMaxCsmCascades).
    void recreate(u32 shadowMapSize, u32 cascadeCount);

    /// @brief F2H60: graba el shadow map para CSM con `cascadeCount`
    ///        cascadas. `lightDir` apunta DESDE la luz HACIA la escena
    ///        (igual que el campo `direction` de `LightComponent`).
    ///        `cameraView`/`cameraProj` son las matrices de la camara
    ///        principal (perspectiva), usadas para particionar el
    ///        frustum. `lambda` controla el split scheme (0=lineal,
    ///        1=log, 0.5=hybrid practico).
    void recordCsm(Scene& scene,
                    AssetManager& assets,
                    IRenderer& renderer,
                    const glm::vec3& lightDir,
                    const glm::mat4& cameraView,
                    const glm::mat4& cameraProj,
                    u32 cascadeCount,
                    f32 lambda);

    /// @brief Matriz `proj * view` de la cascada `i` (0..cascadeCount-1).
    ///        Subir como `uLightSpaces[i]` al lit shader.
    const glm::mat4& lightSpaceMatrix(u32 cascadeIdx) const;

    /// @brief Distancia view-space (positiva) del split `i` (0..N).
    ///        Hay `cascadeCount + 1` splits: `[near, s1, ..., sN, far]`.
    ///        Usado por el lit shader para elegir cascada del fragment.
    f32 cascadeSplit(u32 splitIdx) const;

    /// @brief Cantidad de cascadas configuradas actualmente.
    u32 cascadeCount() const { return m_cascadeCount; }

    /// @brief Bindea el shadow map array a un texture unit para que el
    ///        lit shader lo samplee como `sampler2DArrayShadow`.
    void bindShadowMap(u32 textureUnit) const;

    /// @brief Tamano del shadow map (cuadrado).
    u32 shadowMapSize() const;

private:
    std::unique_ptr<OpenGLShadowMapArray> m_shadowMapArray;
    std::unique_ptr<IShader> m_shader;
    u32 m_cascadeCount = 1;
    std::array<glm::mat4, kMaxCsmCascades>     m_lightSpaces{};
    std::array<f32, kMaxCsmCascades + 1>       m_cascadeSplits{};

    // F2H60 polish iter3: cache del ultimo conteo de casters logueado.
    // Loguea solo al cambiar (evita spam por frame).
    u32 m_lastLoggedCasterTotal    = 0;
    u32 m_lastLoggedBrushesNoCache = 0;
    // F2H60 polish iter4: cache del ultimo (cascadas, lambda) logueados
    // para diagnostico de tiempo real cuando el dev mueve sliders.
    u32 m_lastLoggedCascadeCount   = 0;
    f32 m_lastLoggedLambda         = -1.0f;
};

} // namespace Mood
