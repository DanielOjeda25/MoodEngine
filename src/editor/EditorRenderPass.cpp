// Implementacion del pase de render de la escena a viewport (Hito 16
// refactor — extraido de EditorApplication.cpp).
//
// Aca viven:
//   - `EditorApplication::renderSceneToViewport(dt)`: pipeline completo de
//     render por frame del editor (skybox -> lit scene-driven -> debug ->
//     post-process pass).
//   - `EditorApplication::applyEnvironmentFromScene()`: helper que resetea
//     m_fog/m_exposure/m_tonemap y los sobreescribe con el primer
//     EnvironmentComponent encontrado.
//
// Ambas son metodos de `EditorApplication` (declarados en el .h); aca solo
// se mueven las definiciones para que el .cpp principal no acumule la
// pipeline completa.

#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/render/IRenderer.h"
#include "engine/render/ITexture.h"
#include "engine/render/MeshAsset.h"
#include "engine/render/opengl/OpenGLDebugRenderer.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLShader.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ViewportPick.h"
#include "systems/LightSystem.h"
#include "systems/PostProcessPass.h"
#include "systems/ShadowPass.h"
#include "systems/SkyboxRenderer.h"

#include <glad/gl.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mood {

void EditorApplication::applyEnvironmentFromScene() {
    // Reset a defaults primero. Sin este reset, abrir un proyecto sin
    // Environment hereda los valores del proyecto anterior.
    m_fog      = FogParams{};
    m_exposure = 0.0f;
    m_tonemap  = TonemapMode::ACES;

    if (!m_scene) return;

    bool envFound = false;
    m_scene->forEach<EnvironmentComponent>(
        [&](Entity, EnvironmentComponent& env) {
            if (envFound) return; // primer Environment gana
            envFound = true;
            m_fog.mode        = static_cast<FogMode>(env.fogMode);
            m_fog.color       = env.fogColor;
            m_fog.density     = env.fogDensity;
            m_fog.linearStart = env.fogLinearStart;
            m_fog.linearEnd   = env.fogLinearEnd;
            m_exposure        = env.exposure;
            m_tonemap         = static_cast<TonemapMode>(env.tonemapMode);
        });
}

void EditorApplication::renderSceneToViewport(f32 dt) {
    const u32 desiredW = m_ui.viewport().desiredWidth();
    const u32 desiredH = m_ui.viewport().desiredHeight();
    if (desiredW > 0 && desiredH > 0) {
        // Hito 15 Bloque 3: ambos framebuffers comparten el tamano del panel.
        // El HDR es el target del render real; el LDR es el que ImGui muestra.
        m_sceneFb->resize(desiredW, desiredH);
        m_viewportFb->resize(desiredW, desiredH);
    }

    // Hito 16: Shadow pass ANTES de bindear el scene FB. Buscamos la
    // primera DirectionalLight con `enabled && castShadows` y, si existe,
    // renderizamos al shadow map. Las matrices del light-space quedan en
    // `m_shadowPass->lightSpaceMatrix()` para subir como uniform al lit
    // shader despues. Sin directional con shadows: `shadowEnabled = 0` y
    // el lit shader saltea el sample.
    bool shadowEnabled = false;
    glm::vec3 shadowLightDir(0.0f, -1.0f, 0.0f);
    if (m_scene && m_shadowPass) {
        m_scene->forEach<LightComponent>(
            [&](Entity, LightComponent& lc) {
                if (shadowEnabled) return;
                if (!lc.enabled) return;
                if (lc.type != LightComponent::Type::Directional) return;
                if (!lc.castShadows) return;
                shadowEnabled = true;
                shadowLightDir = lc.direction;
            });
    }
    if (shadowEnabled) {
        // Bounding sphere fijo centrado en el origen del mapa, radius
        // grande para cubrir tiles + props. Cuando llegue CSM (Hito futuro)
        // esto se reemplaza por una serie de cascadas calculadas del frustum
        // de la camara.
        const glm::vec3 sceneCenter = mapWorldOrigin() +
            glm::vec3(m_map.width()  * m_map.tileSize() * 0.5f,
                      0.0f,
                      m_map.height() * m_map.tileSize() * 0.5f);
        const f32 sceneRadius = std::max(m_map.width(), m_map.height())
                              * m_map.tileSize() * 0.75f + 5.0f;
        m_shadowPass->record(*m_scene, *m_assetManager, *m_renderer,
                             shadowLightDir, sceneCenter, sceneRadius);
    }
    // Log one-shot del cambio de estado del shadow pass — util para confirmar
    // visualmente que el directional con castShadows fue detectado.
    if (shadowEnabled != m_shadowEnabledLastFrame) {
        if (shadowEnabled) {
            Log::render()->info(
                "ShadowPass ACTIVO (directional con castShadows detectada, "
                "dir=({:.2f},{:.2f},{:.2f}))",
                shadowLightDir.x, shadowLightDir.y, shadowLightDir.z);
        } else {
            Log::render()->info("ShadowPass inactivo (sin directional con castShadows)");
        }
        m_shadowEnabledLastFrame = shadowEnabled;
    }

    // El render de la escena (sky + lit + fog + debug) escribe al HDR.
    // El post-process pass copia + tonemap a `m_viewportFb` al final.
    m_sceneFb->bind();

    ClearValues clear{};
    clear.color = {0.07f, 0.12f, 0.22f, 1.0f};
    m_renderer->beginFrame(clear);

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;

    glm::mat4 view;
    glm::mat4 projection;
    if (m_mode == EditorMode::Play) {
        view = m_playCamera.viewMatrix();
        projection = m_playCamera.projectionMatrix(aspect);
    } else {
        view = m_editorCamera.viewMatrix();
        projection = m_editorCamera.projectionMatrix(aspect);
    }
    // Hito 13: guardar para que el overlay 2D de ViewportPanel (iconos +
    // gizmos) proyecte posiciones al frame siguiente.
    m_lastView = view;
    m_lastProjection = projection;
    m_lastAspect = aspect;

    // Hito 15 Bloque 1: skybox PRIMERO. El cubo del skybox se rendea con
    // depth=1 (truco pos.z=w) y depth test LEQUAL, asi cualquier geometria
    // con depth < 1 escribe encima. Si el SkyboxRenderer no esta disponible
    // (carga del cubemap fallida), el clear color queda visible.
    if (m_skyboxRenderer) {
        m_skyboxRenderer->draw(view, projection);
    }

    // El mapa se dibuja centrado en el origen del mundo (mapWorldOrigin()).
    // Mismo offset que consume PhysicsSystem: single source of truth.
    const glm::vec3 origin = mapWorldOrigin();
    (void)dt; // Ya no rotamos el modelo; el mapa es estatico.

    // Render scene-driven unificado (Hito 10 Bloque 4). Antes habia dos
    // pases: GridRenderer loopeando el GridMap + pase scene-driven filtrando
    // `Tile_`. Ahora `rebuildSceneFromMap` crea las entidades-tile ya con
    // MeshRendererComponent, y este unico loop las dibuja junto con modelos
    // importados / rotador demo / etc.
    // GridMap sigue existiendo como fuente authoritative de la topologia
    // (physics + save/load + rebuild); solo el render dejo de dependir de el.
    //
    // Itera por submeshes para soportar MeshAssets con multiples submeshes
    // (imports assimp). 1 draw call por submesh, textura segun materialIndex.
    if (m_scene) {
        // Hito 15 Bloque 4: reset + apply del Environment. Helper compartido
        // con `tryOpenProjectPath` para que la primera frame tras cargar un
        // proyecto ya muestre los valores guardados sin un flash a defaults.
        applyEnvironmentFromScene();

        // Hito 11: el render scene-driven usa el shader iluminado. Recolectamos
        // las luces de la scene una vez por frame y subimos los uniforms antes
        // de los draws.
        const glm::vec3 cameraPos = (m_mode == EditorMode::Play)
            ? m_playCamera.position()
            : m_editorCamera.position();
        LightFrameData lights = m_lightSystem->buildFrameData(*m_scene);

        m_litShader->bind();
        m_litShader->setMat4("uView", view);
        m_litShader->setMat4("uProjection", projection);
        m_litShader->setInt("uTexture", 0);
        m_lightSystem->bindUniforms(*m_litShader, lights, cameraPos);

        // Hito 16: shadow uniforms. Si hay directional con castShadows el
        // shadow pass ya escribio al depth FB; subimos la lightSpace matrix
        // y bindeamos el shadow map al texture unit 1 (unit 0 = albedo).
        // `uShadowEnabled = 0` cuando no hay shadows: el lit saltea el
        // sample y el sampler2DShadow puede quedar sin bindear sin crashear.
        m_litShader->setInt("uShadowEnabled", shadowEnabled ? 1 : 0);
        m_litShader->setInt("uShadowMap", 1);
        m_litShader->setFloat("uShadowBias", 0.005f);
        if (shadowEnabled && m_shadowPass) {
            m_litShader->setMat4("uLightSpace",
                                  m_shadowPass->lightSpaceMatrix());
            m_shadowPass->bindShadowMap(1);
        } else {
            m_litShader->setMat4("uLightSpace", glm::mat4(1.0f));
        }
        // Volver al texture unit 0 para los albedos del scene-driven loop.
        glActiveTexture(GL_TEXTURE0);

        // Hito 15 Bloque 2: fog. En este bloque los parametros viven en un
        // member del EditorApplication (`m_fog`) inicializado por default
        // a Exp con densidad sutil para que el smoke test sea visible. El
        // Bloque 4 agrega `EnvironmentComponent` y la fuente pasa a la
        // entidad sentinel "Environment" de la scene.
        m_litShader->setInt  ("uFogMode",    static_cast<int>(m_fog.mode));
        m_litShader->setVec3 ("uFogColor",   m_fog.color);
        m_litShader->setFloat("uFogDensity", m_fog.density);
        m_litShader->setFloat("uFogStart",   m_fog.linearStart);
        m_litShader->setFloat("uFogEnd",     m_fog.linearEnd);

        m_scene->forEach<TransformComponent, MeshRendererComponent>(
            [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
                MeshAsset* asset = m_assetManager->getMesh(mr.mesh);
                if (asset == nullptr) return;
                m_litShader->setMat4("uModel", t.worldMatrix());
                for (usize i = 0; i < asset->submeshes.size(); ++i) {
                    const auto& sub = asset->submeshes[i];
                    if (sub.mesh == nullptr) continue;
                    // Fallback slot 0 (missing.png) si el array es mas corto
                    // que el numero de submeshes.
                    const TextureAssetId tex = mr.materialOrMissing(sub.materialIndex);
                    m_assetManager->getTexture(tex)->bind(0);
                    m_renderer->drawMesh(*sub.mesh, *m_litShader);
                }
            });
    }

    // Tile picking: solo en Editor Mode, cuando el cursor esta sobre la
    // imagen del viewport. Resultado se usa abajo para el hover highlight
    // y mas adelante para drag & drop desde el Asset Browser.
    // Suprimido durante el drag del gizmo (Hito 13): el cyan se superpone
    // ruidosamente con las flechas, y es claro que el usuario esta moviendo
    // la entidad, no apuntando a un tile.
    TilePickResult hovered{};
    if (m_mode == EditorMode::Editor && m_ui.viewport().imageHovered() &&
        !m_gizmo.active) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                            m_ui.viewport().mouseNdcY());
        hovered = pickTile(m_map, origin, view, projection, ndc);
    }
    m_hoveredTile = hovered;

    // Debug draw: AABBs de tiles solidos + AABB del jugador en Play Mode
    // (toggle con F1). Hover highlight del tile bajo el cursor (siempre
    // visible en Editor Mode). Todo se acumula y flushea al final.
    bool hasDebugGeometry = false;
    if (m_debugDraw) {
        const glm::vec3 tileColor(1.0f, 0.85f, 0.15f);
        for (u32 ty = 0; ty < m_map.height(); ++ty) {
            for (u32 tx = 0; tx < m_map.width(); ++tx) {
                if (!m_map.isSolid(tx, ty)) continue;
                const AABB local = m_map.aabbOfTile(tx, ty);
                const AABB world{origin + local.min, origin + local.max};
                m_debugRenderer->drawAabb(world, tileColor);
            }
        }
        if (m_mode == EditorMode::Play) {
            const glm::vec3 playerColor(0.2f, 1.0f, 0.4f);
            const glm::vec3 pos = m_playCamera.position();
            m_debugRenderer->drawAabb(
                AABB{pos - k_playerHalfExtents, pos + k_playerHalfExtents},
                playerColor);
        }
        hasDebugGeometry = true;
    }
    if (hovered.hit) {
        const glm::vec3 hoverColor(0.2f, 0.9f, 1.0f); // cyan
        const AABB local = m_map.aabbOfTile(hovered.tileX, hovered.tileY);
        const AABB world{origin + local.min, origin + local.max};
        m_debugRenderer->drawAabb(world, hoverColor);
        hasDebugGeometry = true;
    }
    if (hasDebugGeometry) {
        m_debugRenderer->flush(view, projection);
    }

    m_renderer->endFrame();
    m_sceneFb->unbind();

    // Hito 15 Bloque 3: post-process pass. Lee `m_sceneFb` (HDR), escribe
    // a `m_viewportFb` (LDR) tras `2^exposure` + tonemap + gamma. ImGui
    // muestra `m_viewportFb` en el panel Viewport (`setFramebuffer` lo
    // apunto en el ctor).
    if (m_postProcess && m_sceneFb && m_viewportFb) {
        m_postProcess->apply(*m_sceneFb, *m_viewportFb,
                              m_exposure, m_tonemap);
        m_viewportFb->unbind();
    }
}

} // namespace Mood
