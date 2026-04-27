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
#include "engine/render/LightGrid.h"
#include "engine/render/MaterialAsset.h"
#include "engine/render/MeshAsset.h"
#include "engine/render/opengl/OpenGLCubemapTexture.h"
#include "engine/render/opengl/OpenGLDebugRenderer.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLSSBO.h"
#include "engine/render/opengl/OpenGLShader.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ScenePick.h"
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
    m_fog          = FogParams{};
    m_exposure     = 0.0f;
    m_tonemap      = TonemapMode::ACES;
    m_iblIntensity = 1.0f;

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
            m_iblIntensity    = env.iblIntensity;
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

        m_pbrShader->bind();
        m_pbrShader->setMat4("uView", view);
        m_pbrShader->setMat4("uProjection", projection);
        // Texture units fijos:
        //   0 = albedo, 1 = shadowMap, 2 = metallicRoughness, 3 = ao
        //   4 = irradiance cubemap (IBL diffuse)
        //   5 = prefilter cubemap  (IBL specular)
        //   6 = BRDF LUT 2D        (split-sum)
        // Se setean una sola vez por frame (los samplers no cambian).
        m_pbrShader->setInt("uAlbedoMap",         0);
        m_pbrShader->setInt("uShadowMap",         1);
        m_pbrShader->setInt("uMetallicRoughness", 2);
        m_pbrShader->setInt("uAoMap",             3);
        m_pbrShader->setInt("uIrradianceMap",     4);
        m_pbrShader->setInt("uPrefilterMap",      5);
        m_pbrShader->setInt("uBrdfLut",           6);
        m_lightSystem->bindUniforms(*m_pbrShader, lights, cameraPos);

        // Hito 18: Forward+ light grid. Recompute desde cero en CPU,
        // sube las point lights + tiles + indices via SSBOs (bindings
        // 2/3/4) y deja al shader leer SOLO las luces de su tile.
        const u32 fbW = m_sceneFb->width();
        const u32 fbH = m_sceneFb->height();
        m_lightGrid->compute(lights, view, projection, fbW, fbH);

        // Log diagnostico one-shot: cuando cambia la cantidad de point
        // lights, reportamos cuantas hay, cuantos tiles tiene el grid,
        // cuantas asignaciones totales hizo y el promedio por tile no-vacio.
        // Util para verificar que el culling funciona (asignaciones <<
        // luces*tiles si las luces son chicas).
        const u32 pcount = static_cast<u32>(lights.pointLights.size());
        if (pcount != m_lastLoggedPointLightCount) {
            const u32 totalAssign = m_lightGrid->totalAssignments();
            u32 nonEmpty = 0;
            for (const auto& t : m_lightGrid->tileData()) if (t.count > 0) ++nonEmpty;
            const f32 avgPerNonEmpty = nonEmpty > 0
                ? static_cast<f32>(totalAssign) / static_cast<f32>(nonEmpty)
                : 0.0f;
            Log::render()->info(
                "LightGrid: {} point lights -> {} tiles ({}x{}), {} no-vacios, "
                "{} asignaciones (avg {:.2f}/tile)",
                pcount, m_lightGrid->tileCount(),
                m_lightGrid->tilesX(), m_lightGrid->tilesY(),
                nonEmpty, totalAssign, avgPerNonEmpty);
            m_lastLoggedPointLightCount = pcount;
        }

        // SSBO 2: point light data (std430). Layout en C++ debe matchear
        // el del shader: vec3 + pad + vec3 + float + float + 3*pad.
        // Construimos un buffer flat de floats (12 floats = 48 bytes
        // por luz) para evitar mismatches de alignment con el struct
        // `PointLightData` que tiene padding implicito distinto.
        struct PointLightStd430 {
            f32 posX, posY, posZ; f32 _pad0;
            f32 colR, colG, colB; f32 intensity;
            f32 radius; f32 _pad1, _pad2, _pad3;
        };
        static_assert(sizeof(PointLightStd430) == 48,
                      "PointLight std430 layout mismatch con el shader");
        std::vector<PointLightStd430> packed;
        packed.reserve(lights.pointLights.size());
        for (const auto& p : lights.pointLights) {
            PointLightStd430 e{};
            e.posX = p.position.x; e.posY = p.position.y; e.posZ = p.position.z;
            e.colR = p.color.x;    e.colG = p.color.y;    e.colB = p.color.z;
            e.intensity = p.intensity;
            e.radius    = p.radius;
            packed.push_back(e);
        }
        // SSBOs no soportan size 0; subimos al menos un slot dummy para
        // que el `glBufferData` no falle. El shader nunca lo lee porque
        // `tile.count == 0` cuando no hay luces.
        if (packed.empty()) packed.push_back(PointLightStd430{});
        m_pointLightsSsbo->upload(packed.data(),
            static_cast<GLsizeiptr>(packed.size() * sizeof(PointLightStd430)));
        m_pointLightsSsbo->bind(2);

        // SSBO 3: tile data (uvec2 = 8 bytes per tile).
        const auto& tileData = m_lightGrid->tileData();
        const auto totalTiles = tileData.empty() ? 1u
                              : static_cast<u32>(tileData.size());
        if (tileData.empty()) {
            // Defensivo: si el grid es 0x0 (FB sin tamano), subir un tile
            // sentinela vacio.
            LightTileData zero{0u, 0u};
            m_lightTilesSsbo->upload(&zero, sizeof(LightTileData));
        } else {
            m_lightTilesSsbo->upload(tileData.data(),
                static_cast<GLsizeiptr>(tileData.size() * sizeof(LightTileData)));
        }
        m_lightTilesSsbo->bind(3);

        // SSBO 4: light indices (flat array de uint).
        const auto& indices = m_lightGrid->lightIndices();
        if (indices.empty()) {
            const u32 zero = 0u;
            m_lightIndicesSsbo->upload(&zero, sizeof(u32));
        } else {
            m_lightIndicesSsbo->upload(indices.data(),
                static_cast<GLsizeiptr>(indices.size() * sizeof(u32)));
        }
        m_lightIndicesSsbo->bind(4);

        // Uniforms del grid para que el shader haga la division correcta.
        m_pbrShader->setInt("uTileSize",        static_cast<int>(k_LightGridTileSize));
        m_pbrShader->setInt("uTilesX",          static_cast<int>(m_lightGrid->tilesX()));
        m_pbrShader->setInt("uTilesY",          static_cast<int>(m_lightGrid->tilesY()));

        // IBL bind. Si los tres assets estan disponibles, los bindeamos a
        // sus units y activamos `uIblEnabled`. Sino, el shader usa
        // `uAmbient` escalar (bindUniforms del LightSystem ya lo subio).
        const bool iblOk = (m_iblIrradiance && m_iblPrefilter && m_iblBrdfLut);
        m_pbrShader->setInt("uIblEnabled", iblOk ? 1 : 0);
        if (iblOk) {
            m_iblIrradiance->bind(4);
            m_iblPrefilter->bind(5);
            // ITexture::bind activa la unit y bindea GL_TEXTURE_2D.
            m_iblBrdfLut->bind(6);
            m_pbrShader->setFloat("uPrefilterMaxLod",
                static_cast<f32>(m_iblPrefilter->mipLevels() - 1));
        } else {
            m_pbrShader->setFloat("uPrefilterMaxLod", 0.0f);
        }
        m_pbrShader->setFloat("uIblIntensity", m_iblIntensity);

        // Hito 16: shadow uniforms.
        m_pbrShader->setInt("uShadowEnabled", shadowEnabled ? 1 : 0);
        m_pbrShader->setFloat("uShadowBias", 0.005f);
        if (shadowEnabled && m_shadowPass) {
            m_pbrShader->setMat4("uLightSpace",
                                  m_shadowPass->lightSpaceMatrix());
            m_shadowPass->bindShadowMap(1);
        } else {
            m_pbrShader->setMat4("uLightSpace", glm::mat4(1.0f));
        }

        // Fog (Hito 15) — mismas formulas que lit, copiadas a pbr.frag.
        m_pbrShader->setInt  ("uFogMode",    static_cast<int>(m_fog.mode));
        m_pbrShader->setVec3 ("uFogColor",   m_fog.color);
        m_pbrShader->setFloat("uFogDensity", m_fog.density);
        m_pbrShader->setFloat("uFogStart",   m_fog.linearStart);
        m_pbrShader->setFloat("uFogEnd",     m_fog.linearEnd);

        // Texturas dummy para los slots no usados: el missing del
        // AssetManager (slot 0) sirve como fallback inocuo. Bindeamos
        // SIEMPRE las 3 unidades extra (2, 3) para evitar warnings de
        // "sampler bound to texture not written" del GL debug callback.
        ITexture* dummyTex = m_assetManager->getTexture(
            m_assetManager->missingTextureId());

        m_scene->forEach<TransformComponent, MeshRendererComponent>(
            [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
                MeshAsset* asset = m_assetManager->getMesh(mr.mesh);
                if (asset == nullptr) return;
                m_pbrShader->setMat4("uModel", t.worldMatrix());
                for (usize i = 0; i < asset->submeshes.size(); ++i) {
                    const auto& sub = asset->submeshes[i];
                    if (sub.mesh == nullptr) continue;

                    const MaterialAssetId matId =
                        mr.materialOrMissing(sub.materialIndex);
                    const MaterialAsset* mat = m_assetManager->getMaterial(matId);

                    // Albedo (unit 0). Si el material no tiene mapa, bindeamos
                    // el missing y desactivamos el sample con uHasAlbedoMap=0
                    // para que el shader use solo `uAlbedoTint`.
                    const bool hasAlbedo = (mat != nullptr && mat->albedo != 0);
                    glActiveTexture(GL_TEXTURE0);
                    m_assetManager->getTexture(hasAlbedo ? mat->albedo : 0)
                        ->bind(0);
                    m_pbrShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

                    // Metallic-Roughness (unit 2).
                    const bool hasMR =
                        (mat != nullptr && mat->metallicRoughness != 0);
                    glActiveTexture(GL_TEXTURE2);
                    if (hasMR) {
                        m_assetManager->getTexture(mat->metallicRoughness)
                            ->bind(2);
                    } else {
                        dummyTex->bind(2);
                    }
                    m_pbrShader->setInt("uHasMetallicRoughness",
                                         hasMR ? 1 : 0);

                    // AO (unit 3).
                    const bool hasAo = (mat != nullptr && mat->ao != 0);
                    glActiveTexture(GL_TEXTURE3);
                    if (hasAo) {
                        m_assetManager->getTexture(mat->ao)->bind(3);
                    } else {
                        dummyTex->bind(3);
                    }
                    m_pbrShader->setInt("uHasAoMap", hasAo ? 1 : 0);

                    // Multiplicadores escalares.
                    if (mat != nullptr) {
                        m_pbrShader->setVec3 ("uAlbedoTint",   mat->albedoTint);
                        m_pbrShader->setFloat("uMetallicMult", mat->metallicMult);
                        m_pbrShader->setFloat("uRoughnessMult",mat->roughnessMult);
                        m_pbrShader->setFloat("uAoMult",       mat->aoMult);
                    } else {
                        m_pbrShader->setVec3 ("uAlbedoTint",   glm::vec3(1.0f));
                        m_pbrShader->setFloat("uMetallicMult", 0.0f);
                        m_pbrShader->setFloat("uRoughnessMult",0.5f);
                        m_pbrShader->setFloat("uAoMult",       1.0f);
                    }

                    // Volver a unit 0 antes del draw (consistente con la
                    // convencion de los renderers anteriores).
                    glActiveTexture(GL_TEXTURE0);
                    m_renderer->drawMesh(*sub.mesh, *m_pbrShader);
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
    // Highlights de drag-and-drop. Distinguen segun el tipo de payload:
    //   - Texture / Mesh / Prefab apuntan a un TILE -> cubo cyan sobre
    //     el tile bajo el cursor (raycast vs grid).
    //   - Material apunta a una ENTIDAD -> OBB amarillo sobre el mesh
    //     bajo el cursor (raycast vs Scene). El cubo cyan no aplica
    //     porque el material puede asignarse a entidades fuera del
    //     grid (ej. esferas demo flotantes).
    using DragKind = ViewportPanel::AssetDragKind;
    const DragKind dragKind = m_ui.viewport().assetDragKind();
    const bool dragIsTile =
        dragKind == DragKind::Texture
        || dragKind == DragKind::Mesh
        || dragKind == DragKind::Prefab;
    if (hovered.hit && dragIsTile) {
        const glm::vec3 hoverColor(0.2f, 0.9f, 1.0f); // cyan
        const AABB local = m_map.aabbOfTile(hovered.tileX, hovered.tileY);
        const AABB world{origin + local.min, origin + local.max};
        m_debugRenderer->drawAabb(world, hoverColor);
        hasDebugGeometry = true;
    }
    if (dragKind == DragKind::Material && m_scene
        && m_mode == EditorMode::Editor
        && m_ui.viewport().imageHovered()) {
        // Pick por entidad bajo el cursor (mismo flow que click-select).
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                             m_ui.viewport().mouseNdcY());
        ScenePickResult ehit = pickEntity(*m_scene, view, projection, ndc);
        if (ehit && ehit.entity.hasComponent<TransformComponent>()
                  && ehit.entity.hasComponent<MeshRendererComponent>()) {
            const auto& tf = ehit.entity.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();
            constexpr glm::vec3 kCorners[8] = {
                {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
                { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
                {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
                { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(kCorners[i], 1.0f);
                w[i] = glm::vec3(p);
            }
            constexpr int kEdges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}};
            // Color amarillo brillante para el highlight de drop. Distinto
            // del naranja del outline de seleccion para que el dev sepa
            // "estoy sobre algo dropeable" vs "esto esta seleccionado".
            const glm::vec3 dropColor(1.0f, 0.95f, 0.15f);
            for (const auto& e : kEdges) {
                m_debugRenderer->drawLine(w[e[0]], w[e[1]], dropColor);
            }
            hasDebugGeometry = true;
        }
    }

    // Outline 3D de la entidad seleccionada (estilo Blender/Unity): 12
    // aristas de un OBB (oriented bounding box) que sigue rotacion + escala
    // del Transform. Para entidades con MeshRenderer asumimos que el mesh
    // ocupa el cubo unitario [-0.5, 0.5]^3 en local space (cierto para los
    // primitivos del Asset Manager y para los .obj importados ahora — habra
    // que pasar el AABB del MeshAsset cuando los modelos no esten centrados).
    // Para entidades sin mesh (Light/Audio) saltamos: el icono 2D ya muestra
    // la seleccion con halo cyan en el overlay.
    if (m_scene && m_mode == EditorMode::Editor) {
        Entity sel = m_ui.selectedEntity();
        if (sel && sel.hasComponent<TransformComponent>() &&
            sel.hasComponent<MeshRendererComponent>()) {
            const auto& tf = sel.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();
            // 8 esquinas del cubo unitario centrado en el origen.
            constexpr glm::vec3 kCorners[8] = {
                {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
                { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
                {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
                { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(kCorners[i], 1.0f);
                w[i] = glm::vec3(p);
            }
            // 12 aristas: bottom (0-1,1-2,2-3,3-0), top (4-5,5-6,6-7,7-4),
            // verticales (0-4,1-5,2-6,3-7).
            constexpr int kEdges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}};
            const glm::vec3 selColor(1.0f, 0.55f, 0.05f); // naranja Blender
            for (const auto& e : kEdges) {
                m_debugRenderer->drawLine(w[e[0]], w[e[1]], selColor);
            }
            hasDebugGeometry = true;
        }
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
