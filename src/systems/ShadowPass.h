#pragma once

// ShadowPass (Hito 16 Bloque 2): renderiza la escena desde la perspectiva
// de la luz direccional al depth-only framebuffer (`OpenGLShadowMap`),
// produciendo un shadow map sampleable desde `lit.frag`.
//
// Pipeline:
//   1. `record(scene, lightDir, sceneCenter, sceneRadius)`:
//      - Computa lightView (camara mirando al sceneCenter desde
//        `sceneCenter - lightDir * sceneRadius`).
//      - Computa lightProj ortografico que cubre el bounding sphere.
//      - Bindea el shadow map FBO + viewport.
//      - Itera entidades con MeshRenderer y dibuja con `shadow_depth`
//        shader, sin texturas ni iluminacion.
//      - Front-face culling durante el pass para reducir peter-panning
//        (la cara que mira a la luz no es la que escribe depth de
//        sombra; eso ayuda a evitar shadow acne).
//   2. El caller recupera la matriz `lightSpace = lightProj * lightView`
//      via `lightSpaceMatrix()` y la sube como uniform al lit shader.
//   3. El depth texture (`shadowMap.glDepthTextureId()`) se bindea como
//      sampler2DShadow al lit shader.

#include "core/Types.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <memory>

namespace Mood {

class IShader;
class IRenderer;
class Scene;
class AssetManager;
class OpenGLShadowMap;

class ShadowPass {
public:
    /// @brief Crea el shadow map (depth FBO `size x size`) + compila el
    ///        shader `shadow_depth.{vert,frag}`.
    /// @throws std::runtime_error si el FBO queda incompleto o el shader
    ///         no compila.
    explicit ShadowPass(u32 shadowMapSize = 2048);
    ~ShadowPass();

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    /// @brief Renderiza la escena al shadow map desde la perspectiva de la
    ///        luz direccional. `lightDir` apunta DESDE la luz HACIA la escena
    ///        (igual que el campo `direction` de `LightComponent`).
    /// @param sceneCenter Centro del bounding sphere de la escena (world space).
    /// @param sceneRadius Radio del bounding sphere; define el tamano del
    ///        frustum ortografico.
    void record(Scene& scene,
                AssetManager& assets,
                IRenderer& renderer,
                const glm::vec3& lightDir,
                const glm::vec3& sceneCenter,
                f32 sceneRadius);

    /// @brief `lightProj * lightView` del ultimo `record()`. Subir como
    ///        `uLightSpace` al lit shader para muestrear el shadow map.
    const glm::mat4& lightSpaceMatrix() const { return m_lightSpace; }

    /// @brief Bindea el shadow map a un texture unit para que el lit
    ///        shader lo samplee.
    void bindShadowMap(u32 textureUnit) const;

    /// @brief Tamano del shadow map (cuadrado). Util para PCF dx/dy.
    u32 shadowMapSize() const;

private:
    std::unique_ptr<OpenGLShadowMap> m_shadowMap;
    std::unique_ptr<IShader> m_shader;
    glm::mat4 m_lightSpace{1.0f};
};

} // namespace Mood
