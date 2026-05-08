// F2H28 Bloque D: render path ortografico wireframe del workspace
// "Editor de mapas". 3 FBOs (Top/Front/Side) con un shader dedicado
// (`shaders/wireframe_ortho.{vert,frag}`) que pinta color flat sin
// lighting / IBL / fog / postFX. El path ignora el RHI helper
// (m_renderer->beginFrame/endFrame) y usa GL puro porque corre
// DESPUES del frame perspectivo principal — el RHI ya hizo su
// endFrame y lo bypaseamos para 3 mini-pases independientes.
//
// Filosofia: este path NO regenera meshes. Reusa los `meshCache`
// que el `brushPass` del frame perspectivo hidrato — por eso el
// caller (`EditorRenderPass.cpp`) nos llama DESPUES de
// `m_sceneRenderer->endFrame()`.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"  // F2H29 Bloque C: flush del preview AABB.
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glad/gl.h>

namespace Mood {

namespace {

// Paleta F2H28 opcion A + ajuste por pedido del dev (fondo negro):
//   - Wireframe regular: celeste GMod #6CC1E5 (108, 193, 229).
//   - Brush seleccionado: naranja saturado #FF9933 (255, 153, 51).
//   - Fondo: NEGRO (0, 0, 0). Cambio sobre el plan original — el
//     dev pidio negro porque resalta mejor el wireframe celeste.
//   - Grid menor (cada uSnapStep): gris oscuro (40, 40, 40) para que
//     sea visible sobre negro pero no compita con el wireframe.
//   - Grid mayor (cada uSnapStep * 8): naranja Valve #F58220
//     (245, 130, 32) — convencion Hammer para destacar la grilla
//     gruesa que el dev usa como referencia de escala.
constexpr glm::vec3 k_wireframeColor{108.0f / 255.0f,
                                      193.0f / 255.0f,
                                      229.0f / 255.0f};
constexpr glm::vec3 k_selectedColor {255.0f / 255.0f,
                                      153.0f / 255.0f,
                                       51.0f / 255.0f};
// F2H28 Bloque G: snap se recibe como parametro de renderOrthoView
// (state vive en EditorApplication::m_hammerSnapStep, cycleable con
// Ctrl++/Ctrl+-).
constexpr glm::vec3 k_gridBgColor   {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 k_gridMinorColor{40.0f / 255.0f,
                                      40.0f / 255.0f,
                                      40.0f / 255.0f};
constexpr glm::vec3 k_gridMajorColor{245.0f / 255.0f,
                                      130.0f / 255.0f,
                                       32.0f / 255.0f};

bool entityIsSelected(Entity e, const std::vector<Entity>& selected) {
    if (!e) return false;
    for (const Entity& s : selected) {
        if (s && s.handle() == e.handle()) return true;
    }
    return false;
}

} // namespace

IFramebuffer* SceneRenderer::orthoFb(usize idx) {
    if (idx >= m_orthoFbs.size()) return nullptr;
    return m_orthoFbs[idx].get();
}

void SceneRenderer::renderOrthoView(Scene& scene,
                                     AssetManager& assets,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     glm::vec2 panOffset,
                                     f32 worldHeight,
                                     f32 snapStep,
                                     u32 panelWidth,
                                     u32 panelHeight,
                                     usize idx,
                                     const std::vector<Entity>& selectedEntities) {
    MOOD_PROFILE_FUNCTION();
    (void)assets;  // sin uso por ahora — placeholder para futuras text.

    if (idx >= m_orthoFbs.size()) return;
    if (!m_wireframeOrthoShader) return;
    OpenGLFramebuffer* fb = m_orthoFbs[idx].get();
    if (!fb) return;
    if (panelWidth == 0 || panelHeight == 0) return;

    // Resize FBO si el panel cambio de tamano.
    if (fb->width() != panelWidth || fb->height() != panelHeight) {
        fb->resize(panelWidth, panelHeight);
    }

    fb->bind();

    // Guardar state GL que vamos a tocar para restaurarlo. El RHI
    // perspectivo ya cerro su endFrame() asi que el state global esta
    // "limpio" en el sentido de IRenderer, pero tocamos GL directamente.
    GLint prevViewport[4]; glGetIntegerv(GL_VIEWPORT, prevViewport);
    glViewport(0, 0, static_cast<GLint>(panelWidth),
                     static_cast<GLint>(panelHeight));

    // Clear negro (cambio del dev sobre el plan original gris).
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // F2H28 Bloque E: grid 2D background. Fullscreen triangle con
    // depth write OFF para que las lineas del wireframe (que vienen
    // despues con depth test ON sobre depth buffer limpio) ganen el
    // depth fight y se dibujen encima.
    if (m_grid2dShader) {
        MOOD_PROFILE_SCOPE("OrthoView::grid");
        const f32 aspect = static_cast<f32>(panelWidth) /
                            static_cast<f32>(panelHeight);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        m_grid2dShader->bind();
        m_grid2dShader->setVec2 ("uPanOffset",   panOffset);
        m_grid2dShader->setFloat("uWorldHeight", worldHeight);
        m_grid2dShader->setFloat("uAspect",      aspect);
        m_grid2dShader->setFloat("uSnapStep",    snapStep);
        m_grid2dShader->setVec3 ("uBgColor",     k_gridBgColor);
        m_grid2dShader->setVec3 ("uMinorColor",  k_gridMinorColor);
        m_grid2dShader->setVec3 ("uMajorColor",  k_gridMajorColor);
        glBindVertexArray(m_grid2dVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    // Modo wireframe: lineas en lugar de triangulos. Disable cull para
    // que las caras se vean desde ambos lados. Depth test ON para que
    // las lineas se ordenen correctamente entre brushes apilados (no
    // todo aparece en un mismo plano).
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    m_wireframeOrthoShader->bind();
    m_wireframeOrthoShader->setMat4("uView", view);
    m_wireframeOrthoShader->setMat4("uProjection", projection);

    // 1) Brushes — reusa meshCache hidratado por el brushPass del frame
    //    perspectivo (que se ejecuto antes en este mismo frame).
    {
        MOOD_PROFILE_SCOPE("OrthoView::brushes");
        scene.forEach<TransformComponent, BrushComponent>(
            [&](Entity e, TransformComponent& t, BrushComponent& bc) {
                if (bc.meshCache.empty()) return;
                const glm::vec3 col = entityIsSelected(e, selectedEntities)
                    ? k_selectedColor : k_wireframeColor;
                m_wireframeOrthoShader->setMat4("uModel", t.worldMatrix());
                m_wireframeOrthoShader->setVec3("uColor", col);
                for (auto& mesh : bc.meshCache) {
                    if (!mesh) continue;
                    m_renderer->drawMesh(*mesh, *m_wireframeOrthoShader);
                }
            });
    }

    // 2) Mesh estaticos (MeshRendererComponent). Skinned y particles
    //    omitidos (no se modelan en orto Hammer-style — son props
    //    runtime). Itera 1 draw por submesh.
    {
        MOOD_PROFILE_SCOPE("OrthoView::meshes");
        scene.forEach<TransformComponent, MeshRendererComponent>(
            [&](Entity e, TransformComponent& t, MeshRendererComponent& mr) {
                MeshAsset* asset = assets.getMesh(mr.mesh);
                if (asset == nullptr) return;
                const glm::vec3 col = entityIsSelected(e, selectedEntities)
                    ? k_selectedColor : k_wireframeColor;
                m_wireframeOrthoShader->setMat4("uModel", t.worldMatrix());
                m_wireframeOrthoShader->setVec3("uColor", col);
                for (const auto& sub : asset->submeshes) {
                    if (sub.mesh == nullptr) continue;
                    m_renderer->drawMesh(*sub.mesh, *m_wireframeOrthoShader);
                }
            });
    }

    // F2H29 Bloque C: flush del debugRenderer al FBO orto. Si el caller
    // (EditorRenderPass) encolo lineas/AABBs antes del renderOrthoView,
    // se rinden ahora con view+proj orto. Si el queue esta vacio, es
    // no-op. Reusa el shader + VAO del debug renderer (mismo que la
    // perspectiva), solo que el flush aca apunta al FBO actual y a las
    // matrices orto. Cada renderOrthoView consume su queue (flush
    // limpia), asi cada orto ve su propio set queue-eado por el caller.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (m_debugRenderer) {
        MOOD_PROFILE_SCOPE("OrthoView::debugFlush");
        m_debugRenderer->flush(view, projection);
    }

    // Restaurar state GL global.
    glEnable(GL_CULL_FACE);
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);

    fb->unbind();
}

} // namespace Mood
