// F2H83: helpers F1-debug por feature, extraidos de
// `EditorApplication::drawEditorScene3DOverlay` (en
// `EditorRenderPass_Overlay.cpp`) para mantener el orquestador bajo el
// cap de LOC. Cada helper dibuja su overlay sobre el debug renderer
// recibido; el orquestador los invoca dentro del block
// `if (m_debugDraw && m_scene)`.
//
// Cero cambio de comportamiento vs el inline previo — la extraccion
// es mecanica: cada bloque era un `m_scene->forEach<...>` independiente
// que solo leia `m_scene`, `m_assetManager` y `m_physicsWorld` (todos
// miembros) ademas del `dbg` que llega por parametro.

#include "editor/application/EditorApplication.h"

#include "core/math/AABB.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/ragdoll/RagdollLayout.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <cmath>
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mood {

void EditorApplication::drawTriggersDebugOverlay(OpenGLDebugRenderer& dbg) {
    // Hito 40 B: OBB de cada TriggerComponent. Color verde brillante.
    // Construye los 8 vertices en local space y los proyecta con
    // position + rotation del Transform (ignora scale para mantener
    // `halfExtents` en metros directos — mismo invariante del
    // TriggerSystem). 12 aristas dibujadas como `drawLine`. La rotation
    // hace que se vea como un OBB (no AABB) cuando la entidad esta rotada.
    const glm::vec3 trigColor(0.2f, 1.0f, 0.4f);
    m_scene->forEach<TransformComponent, TriggerComponent>(
        [&](Entity, TransformComponent& tf, TriggerComponent& tr) {
            glm::mat4 m(1.0f);
            m = glm::translate(m, tf.position);
            m = glm::rotate(m, glm::radians(tf.rotationEuler.y),
                             glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(tf.rotationEuler.x),
                             glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(tf.rotationEuler.z),
                             glm::vec3(0, 0, 1));
            const glm::vec3& he = tr.halfExtents;
            glm::vec3 corners[8];
            int idx = 0;
            for (int xi = -1; xi <= 1; xi += 2)
            for (int yi = -1; yi <= 1; yi += 2)
            for (int zi = -1; zi <= 1; zi += 2) {
                const glm::vec4 local(
                    static_cast<f32>(xi) * he.x,
                    static_cast<f32>(yi) * he.y,
                    static_cast<f32>(zi) * he.z,
                    1.0f);
                corners[idx++] = glm::vec3(m * local);
            }
            // 12 aristas del cubo. Indices: bit 0=x, bit 1=y, bit 2=z.
            // Aristas conectan corners que difieren en exactamente 1 bit.
            for (int i = 0; i < 8; ++i) {
                for (int b = 0; b < 3; ++b) {
                    const int j = i ^ (1 << b);
                    if (j > i) {
                        dbg.drawLine(corners[i], corners[j], trigColor);
                    }
                }
            }
        });
}

void EditorApplication::drawJointsDebugOverlay(OpenGLDebugRenderer& dbg) {
    // F2H65: overlay de JointComponents. Para cada joint con
    // targetEntity resuelto, dibujamos: (1) un cubito chico en el
    // pivotWorld del owner (anchor visible), (2) una linea owner pivot
    // -> target position, (3) para Hinge, una flecha corta que indica
    // el eje de rotacion. Color por tipo: Hinge=azul, Distance=verde
    // lima, Point=magenta, Slider=cyan, Fixed=naranja.
    m_scene->forEach<JointComponent, TransformComponent>(
        [&](Entity, JointComponent& joint, TransformComponent& tA) {
            if (joint.targetEntity == kJointNoTarget) return;
            const auto handleB = static_cast<entt::entity>(joint.targetEntity);
            Entity entB = m_scene->entityFromHandle(handleB);
            if (!entB || !entB.hasComponent<TransformComponent>()) return;
            const auto& tB = entB.getComponent<TransformComponent>();

            glm::vec3 color;
            switch (joint.type) {
                case JointComponent::Type::Hinge:
                    color = glm::vec3(0.30f, 0.65f, 1.00f); break;
                case JointComponent::Type::Distance:
                    color = glm::vec3(0.40f, 1.00f, 0.30f); break;
                case JointComponent::Type::Point:
                    color = glm::vec3(1.00f, 0.35f, 0.85f); break;
                case JointComponent::Type::Slider:
                    color = glm::vec3(0.30f, 0.95f, 0.95f); break;
                case JointComponent::Type::Fixed:
                    color = glm::vec3(1.00f, 0.60f, 0.15f); break;
            }

            const glm::mat4 worldA = tA.worldMatrix();
            const glm::vec3 pivotWorld =
                glm::vec3(worldA * glm::vec4(joint.pivotLocal, 1.0f));

            // Anchor chiquito en el pivot.
            constexpr f32 k_pivotMark = 0.08f;
            const glm::vec3 he(k_pivotMark);
            dbg.drawAabb(AABB{pivotWorld - he, pivotWorld + he}, color);

            // Linea pivot -> target (visualiza la asociacion A->B).
            dbg.drawLine(pivotWorld, tB.position, color);

            // Para Hinge, dibujamos una flecha corta sobre el eje
            // (segmento de pivotWorld + axis * len). Asi el dev ve
            // si el eje apunta como espera (Y vertical, etc.).
            if (joint.type == JointComponent::Type::Hinge) {
                const glm::vec3 axisWorld =
                    glm::normalize(glm::vec3(
                        worldA * glm::vec4(joint.axisLocal, 0.0f)));
                constexpr f32 k_axisLen = 0.5f;
                const glm::vec3 axisTip = pivotWorld + axisWorld * k_axisLen;
                dbg.drawLine(pivotWorld, axisTip,
                             glm::vec3(1.0f, 1.0f, 0.2f));
            } else if (joint.type == JointComponent::Type::Slider) {
                // F2H71: dibujamos el riel del slider — un segmento que
                // cubre el rango de travel [min, max] sobre el axis,
                // para que el dev vea hasta donde desliza.
                const glm::vec3 axisWorld =
                    glm::normalize(glm::vec3(
                        worldA * glm::vec4(joint.axisLocal, 0.0f)));
                const glm::vec3 railMin =
                    pivotWorld + axisWorld * joint.sliderLimitMin;
                const glm::vec3 railMax =
                    pivotWorld + axisWorld * joint.sliderLimitMax;
                dbg.drawLine(railMin, railMax,
                             glm::vec3(1.0f, 1.0f, 0.2f));
                // Topes en los extremos del riel.
                constexpr f32 k_stop = 0.05f;
                const glm::vec3 hs(k_stop);
                dbg.drawAabb(AABB{railMin - hs, railMin + hs}, color);
                dbg.drawAabb(AABB{railMax - hs, railMax + hs}, color);
            }
        });
}

void EditorApplication::drawForceFieldsDebugOverlay(OpenGLDebugRenderer& dbg) {
    // F2H72: overlay de ForceFieldComponents. Dibuja la zona (Box como
    // OBB wireframe, Sphere como 3 circulos) + un indicador de modo:
    // flecha amarilla para Directional (la direccion del viento),
    // spokes radiales para Radial. Violeta = activo, gris = enabled=false.
    m_scene->forEach<TransformComponent, ForceFieldComponent>(
        [&](Entity, TransformComponent& tf, ForceFieldComponent& ff) {
            const glm::vec3 color = ff.enabled
                ? glm::vec3(0.70f, 0.40f, 1.00f)
                : glm::vec3(0.45f, 0.45f, 0.45f);
            const glm::vec3 center = tf.position;

            if (ff.shape == ForceFieldComponent::Shape::Box) {
                glm::mat4 m(1.0f);
                m = glm::translate(m, center);
                m = glm::rotate(m, glm::radians(tf.rotationEuler.y), glm::vec3(0, 1, 0));
                m = glm::rotate(m, glm::radians(tf.rotationEuler.x), glm::vec3(1, 0, 0));
                m = glm::rotate(m, glm::radians(tf.rotationEuler.z), glm::vec3(0, 0, 1));
                const glm::vec3& he = ff.halfExtents;
                glm::vec3 corners[8];
                int idx = 0;
                for (int xi = -1; xi <= 1; xi += 2)
                for (int yi = -1; yi <= 1; yi += 2)
                for (int zi = -1; zi <= 1; zi += 2) {
                    corners[idx++] = glm::vec3(m * glm::vec4(
                        static_cast<f32>(xi) * he.x,
                        static_cast<f32>(yi) * he.y,
                        static_cast<f32>(zi) * he.z, 1.0f));
                }
                for (int i = 0; i < 8; ++i)
                    for (int b = 0; b < 3; ++b) {
                        const int jj = i ^ (1 << b);
                        if (jj > i) dbg.drawLine(corners[i], corners[jj], color);
                    }
            } else {
                // Sphere: 3 circulos (planos XY, XZ, YZ) por segmentos.
                constexpr int kSeg = 24;
                const f32 r = ff.radius;
                for (int axis = 0; axis < 3; ++axis) {
                    glm::vec3 prev(0.0f);
                    for (int s = 0; s <= kSeg; ++s) {
                        const f32 a = static_cast<f32>(s) / kSeg * 6.2831853f;
                        const f32 c = std::cos(a) * r;
                        const f32 sn = std::sin(a) * r;
                        glm::vec3 p = (axis == 0) ? center + glm::vec3(c, sn, 0.0f)
                                    : (axis == 1) ? center + glm::vec3(c, 0.0f, sn)
                                                  : center + glm::vec3(0.0f, c, sn);
                        if (s > 0) dbg.drawLine(prev, p, color);
                        prev = p;
                    }
                }
            }

            const glm::vec3 yellow(1.0f, 1.0f, 0.2f);
            if (ff.mode == ForceFieldComponent::Mode::Directional) {
                const f32 len = glm::length(ff.direction);
                const glm::vec3 d = (len > 1e-4f)
                    ? ff.direction / len : glm::vec3(0, 1, 0);
                dbg.drawLine(center, center + d * 1.0f, yellow);
            } else {
                constexpr f32 k = 0.6f;
                dbg.drawLine(center, center + glm::vec3( k, 0, 0), yellow);
                dbg.drawLine(center, center + glm::vec3(-k, 0, 0), yellow);
                dbg.drawLine(center, center + glm::vec3(0,  k, 0), yellow);
                dbg.drawLine(center, center + glm::vec3(0, -k, 0), yellow);
                dbg.drawLine(center, center + glm::vec3(0, 0,  k), yellow);
                dbg.drawLine(center, center + glm::vec3(0, 0, -k), yellow);
            }
        });
}

void EditorApplication::drawRagdollsDebugOverlay(OpenGLDebugRenderer& dbg) {
    // F2H66 Bloque F: overlay de ragdolls. Para cada entidad con
    // RagdollComponent en estado Ragdolling, dibujamos cada body como
    // wireframe capsule (linea central + 2 circulos en los extremos +
    // 4 longitudinales) y lineas amarillas parent->child para
    // visualizar la jerarquia de constraints. Util para diagnosticar
    // pose anatomica (codos al reves, brazos mal limitados) sin tener
    // que loguear matrices.
    if (m_assetManager == nullptr || !m_physicsWorld) return;

    const glm::vec3 capsuleColor(1.0f, 0.55f, 0.10f);
    const glm::vec3 boneConnColor(1.0f, 0.85f, 0.20f);
    std::vector<glm::mat4> partWorlds;
    m_scene->forEach<RagdollComponent, MeshRendererComponent>(
        [&](Entity, RagdollComponent& rag, MeshRendererComponent& mr) {
            if (rag.state != RagdollComponent::State::Ragdolling) return;
            if (rag.ragdollId == 0) return;
            const MeshAsset* mesh = m_assetManager->getMesh(mr.mesh);
            if (mesh == nullptr || !mesh->skeleton.has_value()) return;
            if (!m_physicsWorld->readRagdollPose(rag.ragdollId, partWorlds)) {
                return;
            }
            const auto layout = ragdoll::buildMixamoLayout(
                *mesh->skeleton, rag.totalMass, rag.limbRadius);
            if (layout.bones.size() != partWorlds.size()) return;

            // Helper: dibuja wireframe capsule en world space. axis = Y
            // local del transform (default Jolt capsule).
            auto drawWireCapsule = [&](const glm::mat4& xform,
                                         f32 halfHeight, f32 radius,
                                         const glm::vec3& color) {
                const glm::vec3 center(xform[3]);
                const glm::vec3 axis = glm::normalize(
                    glm::vec3(xform[1]));
                glm::vec3 t1 = glm::cross(axis, glm::vec3(1, 0, 0));
                if (glm::length(t1) < 0.1f) {
                    t1 = glm::cross(axis, glm::vec3(0, 0, 1));
                }
                t1 = glm::normalize(t1);
                const glm::vec3 t2 = glm::cross(axis, t1);
                const glm::vec3 endA = center - axis * halfHeight;
                const glm::vec3 endB = center + axis * halfHeight;
                dbg.drawLine(endA, endB, color);
                constexpr int kSeg = 12;
                for (int e = 0; e < 2; ++e) {
                    const glm::vec3 ec = (e == 0) ? endA : endB;
                    glm::vec3 prev = ec + t1 * radius;
                    for (int i = 1; i <= kSeg; ++i) {
                        const f32 a = static_cast<f32>(i) /
                            static_cast<f32>(kSeg) * 6.28318530718f;
                        const glm::vec3 p = ec
                            + t1 * (radius * std::cos(a))
                            + t2 * (radius * std::sin(a));
                        dbg.drawLine(prev, p, color);
                        prev = p;
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    const f32 a = static_cast<f32>(i) / 4.0f
                        * 6.28318530718f;
                    const glm::vec3 off =
                        t1 * (radius * std::cos(a))
                        + t2 * (radius * std::sin(a));
                    dbg.drawLine(endA + off, endB + off, color);
                }
            };

            for (usize i = 0; i < layout.bones.size(); ++i) {
                const auto& b = layout.bones[i];
                drawWireCapsule(partWorlds[i],
                                 b.capsuleHalfHeight,
                                 b.capsuleRadius, capsuleColor);
                if (b.parentRagdollIndex >= 0 &&
                    b.parentRagdollIndex <
                        static_cast<int>(partWorlds.size())) {
                    const glm::vec3 myC(partWorlds[i][3]);
                    const glm::vec3 parentC(
                        partWorlds[b.parentRagdollIndex][3]);
                    dbg.drawLine(myC, parentC, boneConnColor);
                }
            }
        });
}

void EditorApplication::drawVehiclesDebugOverlay(OpenGLDebugRenderer& dbg) {
    // F2H67 Polish 4: overlay de vehicles. Por cada entity con
    // VehicleComponent + vehicleId valido: leemos el VehicleState del
    // PhysicsWorld y dibujamos:
    //   - OBB azul claro del chassis (transform world).
    //   - 4 puntos verdes en las wheels (sus transforms post-sync).
    //   - Vector amarillo de velocidad (forward * forwardSpeed).
    //   - Lineas grises radiales chassis -> wheels (visualiza la
    //     jerarquia auto-spawned).
    if (!m_physicsWorld) return;

    const glm::vec3 chassisColor(0.40f, 0.70f, 1.00f);
    const glm::vec3 wheelColor  (0.20f, 0.90f, 0.30f);
    const glm::vec3 velColor    (1.00f, 1.00f, 0.20f);
    const glm::vec3 linkColor   (0.70f, 0.70f, 0.70f);
    m_scene->forEach<VehicleComponent, TransformComponent>(
        [&](Entity, VehicleComponent& veh, TransformComponent& tf) {
            (void)tf;
            if (veh.vehicleId == 0) return;
            PhysicsWorld::VehicleState st;
            if (!m_physicsWorld->readVehicleState(veh.vehicleId, st)) return;

            const glm::vec3 chassisCenter(st.chassisWorld[3]);
            // F2H82 Bloque B: leemos los halfExtents reales del
            // .moodvehicle desde el AssetManager (ya cacheado por
            // VehicleSystem) y el `chassisBoxOffset` para que la caja
            // del gizmo coincida visualmente con la caja fisica
            // (RotatedTranslatedShape). Sin esto, el gizmo queda al
            // ras del piso y con tamaño SA por defecto.
            glm::vec3 he(0.9f, 0.5f, 2.0f);
            glm::vec3 boxOffset(0.0f);
            if (m_assetManager != nullptr && !veh.configPath.empty()) {
                const VehicleConfigAssetId cid =
                    m_assetManager->loadVehicleConfig(veh.configPath);
                if (const vehicle::VehicleConfig* cfg =
                        m_assetManager->getVehicleConfig(cid)) {
                    he = cfg->chassisHalfExtents;
                    boxOffset = cfg->chassisBoxOffset;
                }
            }
            const glm::vec3 corners[8] = {
                {boxOffset.x-he.x, boxOffset.y-he.y, boxOffset.z-he.z},
                {boxOffset.x+he.x, boxOffset.y-he.y, boxOffset.z-he.z},
                {boxOffset.x+he.x, boxOffset.y+he.y, boxOffset.z-he.z},
                {boxOffset.x-he.x, boxOffset.y+he.y, boxOffset.z-he.z},
                {boxOffset.x-he.x, boxOffset.y-he.y, boxOffset.z+he.z},
                {boxOffset.x+he.x, boxOffset.y-he.y, boxOffset.z+he.z},
                {boxOffset.x+he.x, boxOffset.y+he.y, boxOffset.z+he.z},
                {boxOffset.x-he.x, boxOffset.y+he.y, boxOffset.z+he.z},
            };
            glm::vec3 worldC[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 w =
                    st.chassisWorld * glm::vec4(corners[i], 1.0f);
                worldC[i] = glm::vec3(w);
            }
            const int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7},
            };
            for (int i = 0; i < 12; ++i) {
                dbg.drawLine(worldC[edges[i][0]],
                              worldC[edges[i][1]], chassisColor);
            }

            const glm::vec3 fwdWorld =
                glm::normalize(glm::vec3(st.chassisWorld[2]));
            const f32 speed = st.forwardSpeed;
            if (std::abs(speed) > 0.1f) {
                dbg.drawLine(chassisCenter,
                              chassisCenter + fwdWorld * speed * 0.2f,
                              velColor);
            }

            for (int i = 0; i < 4; ++i) {
                const glm::vec3 wpos(st.wheelWorlds[i][3]);
                constexpr f32 k = 0.1f;
                dbg.drawLine(wpos - glm::vec3(k,0,0),
                              wpos + glm::vec3(k,0,0), wheelColor);
                dbg.drawLine(wpos - glm::vec3(0,k,0),
                              wpos + glm::vec3(0,k,0), wheelColor);
                dbg.drawLine(wpos - glm::vec3(0,0,k),
                              wpos + glm::vec3(0,0,k), wheelColor);
                dbg.drawLine(chassisCenter, wpos, linkColor);
            }
        });
}

} // namespace Mood
