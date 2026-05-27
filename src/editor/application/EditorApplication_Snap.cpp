// F2H31 Bloque C: snap helper del editor de mapas. Decide entre snap a
// vertex del scene (si m_snapToVertexEnabled + algun vertex cae dentro
// del threshold screen-space) o snap al grid del workspace
// (m_hammerSnapStep). Aislado en un partial para no inflar
// EditorApplication_Run.cpp ya sobre el cap LOC.

#include "editor/application/EditorApplication.h"

#include "engine/assets/manager/AssetManager.h"          // F3H20: mesh AABB lookup
#include "engine/render/resources/MeshAsset.h"           // F3H20: aabbMin/Max
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"   // brushAabbWorld + meshAabbWorld
#include "editor/panels/scene/OrthoCamera.h"
#include "engine/world/csg/Brush.h"            // enumerateBrushVertices

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>

namespace Mood {

glm::vec3 EditorApplication::snapToVertexOrGrid(const glm::vec3& worldPt,
                                                 const OrthoCamera& cam,
                                                 f32 oAspect) {
    const f32 snap = static_cast<f32>(m_hammerSnapStep);
    auto gridSnap = [snap](glm::vec3 v) {
        if (snap > 0.0f) {
            v.x = std::round(v.x / snap) * snap;
            v.y = std::round(v.y / snap) * snap;
            v.z = std::round(v.z / snap) * snap;
        }
        return v;
    };

    // F3H20: el toggle vive en .moodproj > settings.snap (single source
    // of truth). Sin proyecto, no hay snap-to-vertex (no hay donde
    // persistir el flag). El miembro `m_snapToVertexEnabled` queda como
    // mirror runtime para la tecla V / UI request — se sincroniza con
    // ProjectSettings en `pumpUiRequests`.
    const SnapSettings k_snapCfg = m_project
        ? m_project->settings.snap : SnapSettings{};
    if (!k_snapCfg.snapToVertexEnabled || !m_scene) {
        return gridSnap(worldPt);
    }
    const f32 kThresholdNdc = k_snapCfg.snapToVertexThresholdNdc;
    // Broadphase world: solo brushes cuyo AABB world expandido por
    // threshold contiene worldPt. Sin esto, en escenas con cientos de
    // brushes enumerariamos vertices de todos cada frame del drag.
    // F3H6: 16.0f default + leido del proyecto (snapBroadphaseMinWorld).
    const f32 thresholdWorld = std::max(snap * 2.0f, k_snapCfg.snapBroadphaseMinWorld);

    const glm::mat4 mView = cam.viewMatrix();
    const glm::mat4 mProj = cam.projMatrix(oAspect);
    const glm::mat4 mVP = mProj * mView;

    // Cursor en ndc del orto.
    f32 ndcCursorX = 0.0f, ndcCursorY = 0.0f;
    {
        const glm::vec4 c = mVP * glm::vec4(worldPt, 1.0f);
        if (std::abs(c.w) > 1e-6f) {
            ndcCursorX = c.x / c.w;
            ndcCursorY = c.y / c.w;
        }
    }

    glm::vec3 bestVertex(0.0f);
    f32  bestDistNdc = kThresholdNdc;
    bool found = false;

    m_scene->forEach<TransformComponent, BrushComponent>(
        [&](Entity, TransformComponent& t, BrushComponent& bc) {
            const AABB worldAabb = brushAabbWorld(t, bc);
            if (worldPt.x < worldAabb.min.x - thresholdWorld ||
                worldPt.x > worldAabb.max.x + thresholdWorld ||
                worldPt.y < worldAabb.min.y - thresholdWorld ||
                worldPt.y > worldAabb.max.y + thresholdWorld ||
                worldPt.z < worldAabb.min.z - thresholdWorld ||
                worldPt.z > worldAabb.max.z + thresholdWorld) {
                return;
            }
            const std::vector<Csg::BrushVertex> verts =
                Csg::enumerateBrushVertices(bc.brush);
            const glm::mat4 worldM = t.worldMatrix();
            for (const auto& bv : verts) {
                const glm::vec4 vw4 = worldM * glm::vec4(bv.localPos, 1.0f);
                const glm::vec3 vWorld(vw4);
                const glm::vec4 clip = mVP * glm::vec4(vWorld, 1.0f);
                if (std::abs(clip.w) < 1e-6f) continue;
                const f32 ndcX = clip.x / clip.w;
                const f32 ndcY = clip.y / clip.w;
                const f32 dx = ndcX - ndcCursorX;
                const f32 dy = ndcY - ndcCursorY;
                const f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist < bestDistNdc) {
                    bestDistNdc = dist;
                    bestVertex = vWorld;
                    found = true;
                }
            }
        });

    return found ? bestVertex : gridSnap(worldPt);
}

// F3H20 iter 4: las funciones de vertex snap perspectivo (snapToVertexInScene,
// closestVertexOnEntityToWorld, findSnapTargetForGizmo) se removieron en
// favor de grid snap Hammer-style. El experimento de vertex snap con
// source-point + marcadores yellow resultó confuso de usar — los corners
// que se alineaban no eran intuitivos y los marcadores agregaban ruido
// visual. El gizmo perspectivo ahora usa solo grid snap (cuantiza delta
// a multiplos de SnapSettings::snapGridStep), pattern Hammer/SFM-style.
// Ver EditorOverlay_Gizmo.cpp::drawEditorOverlayGizmo y
// EditorOverlay_Modal.cpp::updateModalShortcut. El path orto del workspace
// "Editor de mapas" sigue usando vertex snap (`snapToVertexOrGrid` arriba) —
// ese flujo era hammer-correct: vertex snap actua como override del grid
// snap orto cuando hay un brush vertex dentro del threshold pixel.

} // namespace Mood
