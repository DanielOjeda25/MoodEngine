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

// Paleta F2H28 opcion A confirmada por el dev:
//   - Wireframe regular: celeste GMod #6CC1E5 (108, 193, 229).
//   - Brush seleccionado: naranja saturado #FF9933 (255, 153, 51).
//   - Fondo: gris claro #C8C8C8 (200, 200, 200).
constexpr glm::vec3 k_wireframeColor{108.0f / 255.0f,
                                      193.0f / 255.0f,
                                      229.0f / 255.0f};
constexpr glm::vec3 k_selectedColor {255.0f / 255.0f,
                                      153.0f / 255.0f,
                                       51.0f / 255.0f};
constexpr float k_clearGray = 200.0f / 255.0f;

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

    // Clear gris claro (fondo Hammer-style).
    glClearColor(k_clearGray, k_clearGray, k_clearGray, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    // Restaurar state GL global.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);

    fb->unbind();
}

} // namespace Mood
