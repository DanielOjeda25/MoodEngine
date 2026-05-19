// `EditorApplication::drawEditorScene3DOverlay` — overlay 3D del editor
// dibujado por encima del scene FB ya renderizado. Maneja: tile picking
// highlights, drag highlights, OBB de seleccion (entity + brush), F1
// debug AABBs, etc.
//
// Extraido de `EditorRenderPass.cpp` en AUDIT-2 (2026-05-17) para
// mantener `EditorRenderPass.cpp` bajo el hard cap de 800 LOC. El
// archivo original tenia dos funciones grandes (`renderSceneToViewport`
// + `drawEditorScene3DOverlay`); la segunda pasa a sibling file por
// cohesion (overlay del editor es separable del orchestrator del frame).

#include "editor/application/EditorApplication.h"

#include "core/math/AABB.h"
#include "core/math/Ray.h"  // AUDIT-3: pickRayFromNdc
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/ragdoll/RagdollLayout.h"   // F2H66 F: overlay ragdolls
#include "engine/physics/world/PhysicsWorld.h"      // F2H66 F: readRagdollPose
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/world/csg/Brush.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/scene/queries/ViewportPick.h"

#include <cmath>     // F2H66 F: sin/cos para wire capsules
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mood {

void EditorApplication::drawEditorScene3DOverlay(const glm::mat4& view,
                                                   const glm::mat4& projection,
                                                   const glm::vec3& worldOrigin) {
    if (!m_sceneRenderer) return;
    OpenGLDebugRenderer& dbg = m_sceneRenderer->debugRenderer();

    // Tile picking: solo en Editor Mode con cursor sobre la imagen.
    // Suprimido durante drag del gizmo (cyan se superpone con flechas).
    TilePickResult hovered{};
    if (m_mode == EditorMode::Editor && m_ui.viewport().imageHovered() &&
        !m_gizmo.active) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                            m_ui.viewport().mouseNdcY());
        hovered = pickTile(m_map, worldOrigin, view, projection, ndc);
    }
    m_hoveredTile = hovered;

    // F1 debug: AABBs de tiles solidos + AABB del player en Play Mode.
    if (m_debugDraw) {
        const glm::vec3 tileColor(1.0f, 0.85f, 0.15f);
        for (u32 ty = 0; ty < m_map.height(); ++ty) {
            for (u32 tx = 0; tx < m_map.width(); ++tx) {
                if (!m_map.isSolid(tx, ty)) continue;
                const AABB local = m_map.aabbOfTile(tx, ty);
                const AABB world{worldOrigin + local.min,
                                  worldOrigin + local.max};
                dbg.drawAabb(world, tileColor);
            }
        }
        if (m_mode == EditorMode::Play) {
            const glm::vec3 playerColor(0.2f, 1.0f, 0.4f);
            const glm::vec3 pos = m_playCamera.position();
            dbg.drawAabb(
                AABB{pos - k_playerHalfExtents, pos + k_playerHalfExtents},
                playerColor);
        }

        // Hito 23 Bloque 4: path activo de cada NavAgent como
        // polyline cyan. El waypoint actual (`pathIndex`) se marca
        // con un AABB chiquito brillante para distinguirlo.
        if (m_scene) {
            const glm::vec3 pathColor(0.2f, 0.9f, 1.0f);   // cyan
            const glm::vec3 cursorColor(0.0f, 1.0f, 1.0f); // cyan brillante
            const f32 tileSize = m_map.tileSize();
            auto tileToWorld = [&](const glm::ivec2& t) {
                return glm::vec3(
                    worldOrigin.x + (static_cast<f32>(t.x) + 0.5f) * tileSize,
                    worldOrigin.y + 0.5f * tileSize,
                    worldOrigin.z + (static_cast<f32>(t.y) + 0.5f) * tileSize);
            };
            m_scene->forEach<NavAgentComponent>(
                [&](Entity, NavAgentComponent& nav) {
                    if (nav.path.size() < 2) return;
                    for (usize i = 0; i + 1 < nav.path.size(); ++i) {
                        dbg.drawLine(tileToWorld(nav.path[i]),
                                      tileToWorld(nav.path[i + 1]),
                                      pathColor);
                    }
                    if (nav.pathIndex < nav.path.size()) {
                        const glm::vec3 c = tileToWorld(nav.path[nav.pathIndex]);
                        const glm::vec3 he(0.25f);
                        dbg.drawAabb(AABB{c - he, c + he}, cursorColor);
                    }
                });

            // Hito 40 B: OBB de cada TriggerComponent. Color verde
            // brillante. Construye los 8 vertices en local space y los
            // proyecta con position + rotation del Transform (ignora
            // scale para mantener `halfExtents` en metros directos —
            // mismo invariante del TriggerSystem). 12 aristas dibujadas
            // como `drawLine`. La rotation hace que se vea como un OBB
            // (no AABB) cuando la entidad esta rotada.
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
                    // 12 aristas del cubo. Indices: bit 0=x, bit 1=y,
                    // bit 2=z. Aristas conectan corners que difieren en
                    // exactamente 1 bit.
                    for (int i = 0; i < 8; ++i) {
                        for (int b = 0; b < 3; ++b) {
                            const int j = i ^ (1 << b);
                            if (j > i) {
                                dbg.drawLine(corners[i], corners[j], trigColor);
                            }
                        }
                    }
                });

            // F2H65: overlay de JointComponents. Para cada joint con
            // targetEntity resuelto, dibujamos: (1) un cubito chico en el
            // pivotWorld del owner (anchor visible), (2) una linea owner
            // pivot -> target position, (3) para Hinge, una flecha corta
            // que indica el eje de rotacion. Color por tipo: Hinge=azul,
            // Distance=verde lima, Point=magenta.
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
                            color = glm::vec3(0.30f, 0.65f, 1.00f); break;  // azul
                        case JointComponent::Type::Distance:
                            color = glm::vec3(0.40f, 1.00f, 0.30f); break;  // verde
                        case JointComponent::Type::Point:
                            color = glm::vec3(1.00f, 0.35f, 0.85f); break;  // magenta
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
                        // Eje principal en amarillo brillante (consistente con
                        // gizmos de eje en otros editores).
                        dbg.drawLine(pivotWorld, axisTip,
                                     glm::vec3(1.0f, 1.0f, 0.2f));
                    }
                });

            // F2H66 Bloque F: overlay de ragdolls. Para cada entidad con
            // RagdollComponent en estado Ragdolling, dibujamos cada body
            // como wireframe capsule (linea central + 2 circulos en los
            // extremos + 4 longitudinales) y lineas amarillas parent->
            // child para visualizar la jerarquia de constraints. Util
            // para diagnosticar pose anatomica (codos al reves, brazos
            // mal limitados) sin tener que loguear matrices.
            if (m_assetManager && m_physicsWorld) {
                const glm::vec3 capsuleColor(1.0f, 0.55f, 0.10f);  // naranja
                const glm::vec3 boneConnColor(1.0f, 0.85f, 0.20f); // amarillo
                std::vector<glm::mat4> partWorlds;  // buffer reusado
                m_scene->forEach<RagdollComponent, MeshRendererComponent>(
                    [&](Entity, RagdollComponent& rag,
                        MeshRendererComponent& mr) {
                        if (rag.state != RagdollComponent::State::Ragdolling) return;
                        if (rag.ragdollId == 0) return;
                        const MeshAsset* mesh = m_assetManager->getMesh(mr.mesh);
                        if (mesh == nullptr || !mesh->skeleton.has_value()) return;
                        if (!m_physicsWorld->readRagdollPose(rag.ragdollId,
                                                              partWorlds)) {
                            return;
                        }
                        const auto layout = ragdoll::buildMixamoLayout(
                            *mesh->skeleton, rag.totalMass, rag.limbRadius);
                        if (layout.bones.size() != partWorlds.size()) return;

                        // Helper: dibuja wireframe capsule en world space.
                        // axis = Y local del transform (default Jolt capsule).
                        auto drawWireCapsule = [&](const glm::mat4& xform,
                                                     f32 halfHeight, f32 radius,
                                                     const glm::vec3& color) {
                            const glm::vec3 center(xform[3]);
                            const glm::vec3 axis = glm::normalize(
                                glm::vec3(xform[1]));
                            // 2 tangentes perpendiculares al axis (basis
                            // ortogonal estable para los circulos).
                            glm::vec3 t1 = glm::cross(axis, glm::vec3(1, 0, 0));
                            if (glm::length(t1) < 0.1f) {
                                t1 = glm::cross(axis, glm::vec3(0, 0, 1));
                            }
                            t1 = glm::normalize(t1);
                            const glm::vec3 t2 = glm::cross(axis, t1);
                            const glm::vec3 endA = center - axis * halfHeight;
                            const glm::vec3 endB = center + axis * halfHeight;
                            // Linea central (eje del capsule).
                            dbg.drawLine(endA, endB, color);
                            // 2 circulos en endpoints (12 segmentos cada uno).
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
                            // 4 longitudinales conectando los circulos
                            // (en X+, X-, Z+, Z- del plano tangente).
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
                            // Linea amarilla parent->child (visualiza
                            // donde estan los constraints).
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
        }
    }

    // Highlights de drag-and-drop. Distinguen segun el tipo de payload:
    //   - Texture / Mesh / Prefab apuntan a un TILE -> cubo cyan sobre
    //     el tile bajo el cursor (raycast vs grid).
    //   - Material apunta a una ENTIDAD -> OBB amarillo sobre el mesh
    //     bajo el cursor (raycast vs Scene).
    using DragKind = ViewportPanel::AssetDragKind;
    const DragKind dragKind = m_ui.viewport().assetDragKind();
    const bool dragIsTile =
        dragKind == DragKind::Texture
        || dragKind == DragKind::Mesh
        || dragKind == DragKind::Prefab;
    if (hovered.hit && dragIsTile) {
        const glm::vec3 hoverColor(0.2f, 0.9f, 1.0f); // cyan
        const AABB local = m_map.aabbOfTile(hovered.tileX, hovered.tileY);
        const AABB world{worldOrigin + local.min, worldOrigin + local.max};
        dbg.drawAabb(world, hoverColor);
    }
    // Script drop tambien apunta a una entidad — mismo highlight OBB
    // amarillo que Material para mantener consistencia visual (Hito 22
    // Bloque 2). Para entidades sin mesh (Light/Audio) el OBB no se
    // dibuja porque el branch interno verifica `MeshRendererComponent`.
    const bool dragTargetsEntity =
        dragKind == DragKind::Material || dragKind == DragKind::Script;
    if (dragTargetsEntity && m_scene
        && m_mode == EditorMode::Editor
        && m_ui.viewport().imageHovered()) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                             m_ui.viewport().mouseNdcY());
        ScenePickResult ehit = pickEntity(*m_scene, view, projection, ndc,
                                            m_assetManager.get());
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
            const glm::vec3 dropColor(1.0f, 0.95f, 0.15f);
            for (const auto& e : kEdges) {
                dbg.drawLine(w[e[0]], w[e[1]], dropColor);
            }
        }
    }

    // F2H13: outline 3D de TODAS las entidades del SelectionSet.
    // Color naranja para la `active`, gris para las demas selected.
    // Para entidades sin mesh / brush (Light / Audio) no se dibuja —
    // el icono 2D ya las marca con halo cyan en el overlay 2D.
    if (m_scene && m_mode == EditorMode::Editor) {
        constexpr int kEdges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}};
        const glm::vec3 activeColor(1.00f, 0.35f, 0.00f); // naranja Blender saturado
        const glm::vec3 selColor   (0.95f, 0.95f, 0.20f); // amarillo claro (resalta sobre cielo gris-azul)

        const SelectionSet& set = m_ui.selectionSet();
        // F2H17 + F2H33: en Face Mode con cara(s) seleccionada(s),
        // NO dibujamos el outline del brush activo (caeria encima
        // del highlight de cara y los pixeles se superponen
        // perdiendo claridad). Solo se dibujan los outlines de las
        // caras seleccionadas.
        const bool faceModeWithSelectedFace =
            (m_subMode == EditorSubMode::Face) &&
            static_cast<bool>(set.active) &&
            !set.selectedFaceIndices.empty();

        for (const Entity& sel : set.selected) {
            if (!sel || !sel.hasComponent<TransformComponent>()) continue;
            const bool isActive = static_cast<bool>(set.active)
                && set.active.handle() == sel.handle();
            // Skip el outline del brush active cuando hay cara seleccionada
            // en Face Mode.
            if (isActive && faceModeWithSelectedFace) continue;
            const auto& tf = sel.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();

            // Corners locales: para brush = AABB local del brush
            // (F2H11 ya lo computa); para mesh = AABB real del
            // MeshAsset (F2H44: antes era cubo unitario hardcoded
            // -0.5/0.5, lo que hacia que el outline de meshes grandes
            // como Fox.glb fuera invisible — un cubito de 1m perdido
            // en el centro). Para entidades sin mesh ni brush (Light/
            // Audio/Trigger/Camera/ParticleEmitter) usamos un AABB
            // chico fijo centrado en el origen para que el outline
            // tambien aparezca y haya feedback de seleccion uniforme.
            glm::vec3 localMin(-0.5f), localMax(0.5f);
            if (sel.hasComponent<BrushComponent>()) {
                const auto& bc = sel.getComponent<BrushComponent>();
                if (bc.brush.localAabb.isValid()) {
                    localMin = bc.brush.localAabb.min;
                    localMax = bc.brush.localAabb.max;
                }
            } else if (sel.hasComponent<MeshRendererComponent>()) {
                if (m_assetManager) {
                    const auto& mr = sel.getComponent<MeshRendererComponent>();
                    if (const MeshAsset* asset = m_assetManager->getMesh(mr.mesh)) {
                        localMin = asset->aabbMin;
                        localMax = asset->aabbMax;
                    }
                }
            } else {
                // Point entity (Light/Audio/Trigger/Camera/ParticleEmitter):
                // marker chico fijo de 0.5m^3 alrededor del origen para
                // que el outline tambien marque la seleccion.
                localMin = glm::vec3(-0.25f);
                localMax = glm::vec3(0.25f);
            }

            const glm::vec3 corners[8] = {
                {localMin.x, localMin.y, localMin.z},
                {localMax.x, localMin.y, localMin.z},
                {localMax.x, localMax.y, localMin.z},
                {localMin.x, localMax.y, localMin.z},
                {localMin.x, localMin.y, localMax.z},
                {localMax.x, localMin.y, localMax.z},
                {localMax.x, localMax.y, localMax.z},
                {localMin.x, localMax.y, localMax.z}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(corners[i], 1.0f);
                w[i] = glm::vec3(p);
            }

            const glm::vec3& color = isActive ? activeColor : selColor;
            for (const auto& e : kEdges) {
                dbg.drawLine(w[e[0]], w[e[1]], color);
            }
        }

        // F2H17 + F2H33: highlight de cara(s) seleccionada(s) en
        // Face Mode estilo Blender — capa SEMI-TRANSPARENTE encima
        // de cada cara + outline brillante alrededor.
        //   - Caras "secundarias" (todas menos la ultima clickeada):
        //     naranja (~Half-Life).
        //   - Cara "active" (back de selectedFaceIndices, = primary
        //     para single-face ops): amarilla, mas brillante. Asi el
        //     dev distingue cual es la primaria cuando hay multi-
        //     seleccion via Shift+click.
        if (m_subMode == EditorSubMode::Face &&
            static_cast<bool>(set.active) &&
            set.active.hasComponent<BrushComponent>() &&
            set.active.hasComponent<TransformComponent>() &&
            !set.selectedFaceIndices.empty()) {
            const auto& bc = set.active.getComponent<BrushComponent>();
            const auto& tf = set.active.getComponent<TransformComponent>();
            const i32 activeIdx = set.activeFaceIndex();
            // F2H17: la capa rellena solo se dibuja cuando el dev
            // NO esta editando UV params — durante un drag de slider
            // la capa tapa la textura. Outline siempre.
            const bool editingUV = m_ui.inspector().isEditingBrushUV();
            const glm::mat4 worldMat = tf.worldMatrix();
            for (i32 faceIdxSigned : set.selectedFaceIndices) {
                if (faceIdxSigned < 0) continue;
                const u32 faceIdx = static_cast<u32>(faceIdxSigned);
                if (faceIdx >= bc.brush.faces.size()) continue;
                const bool isActive = (faceIdxSigned == activeIdx);
                // F2H33: amarillo (active/primary) vs naranja (otras).
                const glm::vec3 outlineColor = isActive
                    ? glm::vec3(1.00f, 0.95f, 0.10f)
                    : glm::vec3(1.00f, 0.50f, 0.00f);
                const glm::vec4 fillColor = isActive
                    ? glm::vec4(1.00f, 0.95f, 0.10f, 0.50f)
                    : glm::vec4(1.00f, 0.55f, 0.10f, 0.40f);
                const auto poly = Csg::collectFaceWorldPolygon(
                    bc.brush, faceIdx, worldMat);
                const usize n = poly.size();
                if (n < 3) continue;
                if (!editingUV) {
                    glm::vec3 centroid(0.0f);
                    for (const auto& v : poly) centroid += v;
                    centroid /= static_cast<f32>(n);
                    for (usize i = 0; i < n; ++i) {
                        dbg.drawTriangle(centroid, poly[i],
                                          poly[(i + 1) % n], fillColor);
                    }
                }
                for (usize i = 0; i < n; ++i) {
                    dbg.drawLine(poly[i], poly[(i + 1) % n], outlineColor);
                }
            }
        }

        // F2H35 Bloque F: preview hover en Face Mode. Cada frame, si el
        // cursor esta sobre el viewport perspectivo, calcular pickFace
        // contra el brush hovered y dibujar el outline de esa cara en
        // blanco tenue. El dev VE que cara va a seleccionar antes de
        // clickear — resuelve la queja "no se que cara voy a pickear
        // hasta que clickeo, requiere clicks ciegos hasta acertar".
        // Skip si la cara ya esta seleccionada (sin valor agregado y
        // su outline brillante ya gana la atencion).
        if (m_subMode == EditorSubMode::Face &&
            m_mode == EditorMode::Editor &&
            m_ui.viewport().imageHovered() &&
            m_scene && m_assetManager) {
            const float ndcX = m_ui.viewport().mouseNdcX();
            const float ndcY = m_ui.viewport().mouseNdcY();
            ScenePickResult hoverHit = pickEntity(*m_scene, view,
                projection, glm::vec2(ndcX, ndcY), m_assetManager.get());
            if (hoverHit && hoverHit.entity.hasComponent<BrushComponent>() &&
                hoverHit.entity.hasComponent<TransformComponent>()) {
                const auto& bc =
                    hoverHit.entity.getComponent<BrushComponent>();
                const auto& tf =
                    hoverHit.entity.getComponent<TransformComponent>();
                // Reconstruir el rayo desde la cam (mismo flow del
                // click handler).
                const glm::mat4 invVP = glm::inverse(projection * view);
                if (const auto ray = pickRayFromNdc(invVP, ndcX, ndcY)) {
                    const auto faceHit = Csg::pickFace(
                        bc.brush, ray->origin, ray->direction,
                        tf.worldMatrix());
                    if (faceHit.has_value()) {
                        const i32 idx = static_cast<i32>(*faceHit);
                        // Skip si la cara hovered ya esta en el set
                        // (su highlight brillante ya gana atencion).
                        const bool alreadySelected =
                            (set.active.handle() ==
                             hoverHit.entity.handle()) &&
                            std::find(set.selectedFaceIndices.begin(),
                                      set.selectedFaceIndices.end(),
                                      idx) != set.selectedFaceIndices.end();
                        if (!alreadySelected) {
                            const auto poly = Csg::collectFaceWorldPolygon(
                                bc.brush, static_cast<u32>(idx),
                                tf.worldMatrix());
                            const usize n = poly.size();
                            if (n >= 3) {
                                // F2H35 fix: cyan saturado para que se
                                // vea sobre cualquier textura (incluido
                                // fondos blancos/claros donde el blanco
                                // tenue inicial desaparecia). Contrasta
                                // con amarillo (active) y naranja
                                // (secondary selected) — el dev distingue
                                // hover-preview vs ya-seleccionado.
                                const glm::vec3 hoverColor(
                                    0.10f, 0.95f, 1.00f);
                                for (usize i = 0; i < n; ++i) {
                                    dbg.drawLine(poly[i],
                                                  poly[(i + 1) % n],
                                                  hoverColor);
                                }
                            }
                        }
                    }
                }
            }
        }

        // F2H30 Bloque C: preview del pincel poligonal — lineas entre
        // vertices agregados + markers blancos en cada uno + RUBBER
        // BAND del ultimo vertex al cursor live (snappeado). El user
        // ve el polígono moviendose mientras decide donde clickear.
        if (m_polyDraw.active && m_polyDraw.pointsWorld.size() > 0) {
            const glm::vec3 polyColor(1.0f, 1.0f, 1.0f);   // blanco
            const f32 snap = static_cast<f32>(m_hammerSnapStep);
            const f32 markerHalf = std::max(snap * 0.05f, 0.05f);
            const glm::vec3 he(markerHalf);
            for (usize i = 0; i < m_polyDraw.pointsWorld.size(); ++i) {
                const glm::vec3& p = m_polyDraw.pointsWorld[i];
                dbg.drawAabb(AABB{p - he, p + he}, polyColor);
                if (i + 1 < m_polyDraw.pointsWorld.size()) {
                    dbg.drawLine(p, m_polyDraw.pointsWorld[i + 1], polyColor);
                }
            }
            // Linea de cierre (preview del cierre cuando >= 3 puntos):
            // dibuja en otro tono para que el dev vea que falta cerrar.
            if (m_polyDraw.pointsWorld.size() >= 3) {
                const glm::vec3 closeColor(0.5f, 0.5f, 0.5f); // gris
                dbg.drawLine(m_polyDraw.pointsWorld.back(),
                              m_polyDraw.pointsWorld.front(), closeColor);
            }
            // Rubber band: linea celeste del ultimo vertex al cursor
            // live de la orto lockeada. Solo si hay locked ortho Y
            // cursor hovered.
            if (m_polyDraw.orthoIdx >= 0) {
                OrthoViewportPanel* ops[3] = {
                    &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
                };
                OrthoViewportPanel* op = ops[m_polyDraw.orthoIdx];
                const auto& lc = op->liveCursor();
                if (lc.hovered && op->desiredHeight() > 0) {
                    const f32 oA = static_cast<f32>(op->desiredWidth())
                                  / static_cast<f32>(op->desiredHeight());
                    glm::vec3 cursorWorld = op->camera().worldFromNdc(
                        lc.ndcX, lc.ndcY, oA);
                    // F2H31 Bloque C: rubber band perspectivo tambien
                    // sigue al snap-to-vertex.
                    cursorWorld = snapToVertexOrGrid(cursorWorld,
                                                      op->camera(), oA);
                    const glm::vec3 rubberColor(108.0f / 255.0f,
                                                  193.0f / 255.0f,
                                                  229.0f / 255.0f); // celeste GMod
                    dbg.drawLine(m_polyDraw.pointsWorld.back(),
                                  cursorWorld, rubberColor);
                    // Marker fantasma en el cursor (para que se vea
                    // donde caeria el proximo vertex).
                    dbg.drawAabb(AABB{cursorWorld - he, cursorWorld + he},
                                  rubberColor);
                }
            }
        }

        // F2H30 Bloque B: visualizacion de vertices/edges del brush
        // active en sub-mode Vertex/Edge (estilo Blender). Sin esto el
        // dev no sabe DONDE clickear para mover. Markers en blanco
        // (visibles sobre cualquier fondo); tamano proporcional al
        // snapStep para que escale con el zoom del workspace.
        if ((m_subMode == EditorSubMode::Vertex ||
             m_subMode == EditorSubMode::Edge) &&
            static_cast<bool>(set.active) &&
            set.active.hasComponent<BrushComponent>() &&
            set.active.hasComponent<TransformComponent>()) {
            const auto& bc = set.active.getComponent<BrushComponent>();
            const auto& tf = set.active.getComponent<TransformComponent>();
            const glm::mat4 wm = tf.worldMatrix();
            const auto verts = Csg::enumerateBrushVertices(bc.brush);
            const f32 snap = static_cast<f32>(m_hammerSnapStep);
            const glm::vec3 markerColor(1.0f, 1.0f, 1.0f);  // blanco

            if (m_subMode == EditorSubMode::Vertex) {
                // Markers de vertex: pequenos cubos en cada esquina.
                const f32 half = std::max(snap * 0.05f, 0.05f);
                const glm::vec3 he(half);
                for (const auto& v : verts) {
                    const glm::vec3 wp = glm::vec3(
                        wm * glm::vec4(v.localPos, 1.0f));
                    dbg.drawAabb(AABB{wp - he, wp + he}, markerColor);
                }
            } else {
                // Edge mode: lineas brillantes en cada arista (par de
                // caras que comparten exactamente 2 vertices).
                const usize n = bc.brush.faces.size();
                for (u32 i = 0; i < static_cast<u32>(n); ++i) {
                    for (u32 j = i + 1; j < static_cast<u32>(n); ++j) {
                        int idxA = -1, idxB = -1;
                        for (usize v = 0; v < verts.size(); ++v) {
                            bool hasA = false, hasB = false;
                            for (u32 p : verts[v].planeIndices) {
                                if (p == i) hasA = true;
                                if (p == j) hasB = true;
                            }
                            if (!hasA || !hasB) continue;
                            if (idxA < 0) idxA = static_cast<int>(v);
                            else if (idxB < 0) idxB = static_cast<int>(v);
                            else { idxA = -1; break; }
                        }
                        if (idxA < 0 || idxB < 0) continue;
                        const glm::vec3 a = glm::vec3(
                            wm * glm::vec4(verts[static_cast<usize>(idxA)].localPos, 1.0f));
                        const glm::vec3 b = glm::vec3(
                            wm * glm::vec4(verts[static_cast<usize>(idxB)].localPos, 1.0f));
                        dbg.drawLine(a, b, markerColor);
                    }
                }
            }
        }
    }
}

} // namespace Mood
