// F2H32 Bloque B: clip tool — splittea brushes seleccionados con un
// plano armado de 2 clicks en orto (plano perpendicular al view plane,
// extendido sobre el forwardAxis del orto). KeepMode cycle Front /
// Back / Both via tecla T durante la sesion. Reusa BooleanOpCommand
// con kind=Clip y bSnapshot vacio.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/math/Plane.h"
#include "editor/commands/BooleanOpCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/panels/scene/OrthoViewportPanel.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushOps.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace Mood {

namespace {

// Helpers replicados del scope de EditorProjectActions_Boolean.cpp
// (anonymous namespace en otra TU). Duplicados para evitar exponerlos
// en el header — son utility functions chicas y ligadas al tipo
// SavedBrush.

SavedBrush snapshotBrushClip(Entity e, const AssetManager& assets) {
    SavedBrush sb;
    if (e.hasComponent<TagComponent>()) {
        sb.tag = e.getComponent<TagComponent>().name;
    }
    if (e.hasComponent<TransformComponent>()) {
        const auto& t = e.getComponent<TransformComponent>();
        sb.position      = t.position;
        sb.rotationEuler = t.rotationEuler;
        sb.scale         = t.scale;
    }
    if (e.hasComponent<BrushComponent>()) {
        const auto& bc = e.getComponent<BrushComponent>();
        const MaterialAssetId mat0 = bc.materials.empty() ? 0u : bc.materials[0];
        sb.materialPath = (mat0 == 0) ? std::string{}
                                       : assets.materialPathOf(mat0);
        for (MaterialAssetId mid : bc.materials) {
            sb.materialPaths.push_back(
                (mid == 0) ? std::string{} : assets.materialPathOf(mid));
        }
        for (const auto& f : bc.brush.faces) {
            SavedBrushFace sf;
            sf.normal        = f.plane.normal;
            sf.distance      = f.plane.distance;
            sf.materialIndex = f.materialIndex;
            sb.faces.push_back(sf);
        }
    }
    return sb;
}

SavedBrush snapshotResultLocal(const Csg::Brush& localBrush,
                                const std::string& tag,
                                const glm::vec3& position,
                                const glm::vec3& rotationEuler,
                                const glm::vec3& scale,
                                const std::string& materialPath) {
    SavedBrush sb;
    sb.tag = tag;
    sb.materialPath = materialPath;
    sb.position = position;
    sb.rotationEuler = rotationEuler;
    sb.scale = scale;
    for (const auto& f : localBrush.faces) {
        SavedBrushFace sf;
        sf.normal        = f.plane.normal;
        sf.distance      = f.plane.distance;
        sf.materialIndex = f.materialIndex;
        sb.faces.push_back(sf);
    }
    return sb;
}

std::string uniqueClipTag(const Scene& scene, const std::string& base,
                           const std::vector<std::string>& reserved) {
    int suffix = 1;
    while (true) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s_%02d", base.c_str(), suffix);
        std::string candidate = buf;
        bool collision = false;
        const_cast<Scene&>(scene).forEach<TagComponent>(
            [&](Entity, TagComponent& tag) {
                if (tag.name == candidate) collision = true;
            });
        if (!collision) {
            for (const auto& r : reserved) {
                if (r == candidate) { collision = true; break; }
            }
        }
        if (!collision) return candidate;
        ++suffix;
    }
}

} // namespace

void EditorApplication::cycleClipKeepMode() {
    if (!m_clipTool.active) return;
    switch (m_clipTool.keepMode) {
        case ClipKeepMode::Front: m_clipTool.keepMode = ClipKeepMode::Back; break;
        case ClipKeepMode::Back:  m_clipTool.keepMode = ClipKeepMode::Both; break;
        case ClipKeepMode::Both:  m_clipTool.keepMode = ClipKeepMode::Front; break;
    }
    const char* lbl = (m_clipTool.keepMode == ClipKeepMode::Front) ? "Front"
                     : (m_clipTool.keepMode == ClipKeepMode::Back)  ? "Back"
                     : "Both";
    Log::editor()->info("[clip] keep mode -> {}", lbl);
}

void EditorApplication::cancelClipTool() {
    if (!m_clipTool.active) return;
    Log::editor()->info("[clip] cancelado");
    m_clipTool = ClipToolSession{};
}

void EditorApplication::confirmClipTool() {
    if (!m_clipTool.active) return;
    if (!m_scene || !m_assetManager) {
        cancelClipTool();
        return;
    }
    if (!m_clipTool.hasP1 || !m_clipTool.hasP2) {
        Log::editor()->warn("[clip] necesito 2 puntos para definir el plano");
        return;
    }

    // F2H32 Bloque B: el plano se arma con la linea (p1->p2) extendida
    // sobre el forwardAxis del orto donde se hicieron los clicks.
    // normal = cross(forwardAxis, lineDir) — perpendicular a ambos,
    // sobre el view plane. d = -dot(normal, p1).
    OrthoViewportPanel* clipOrthoPanels[3] = {
        &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
    };
    if (m_clipTool.orthoIdx < 0 || m_clipTool.orthoIdx >= 3) {
        cancelClipTool();
        return;
    }
    const auto& cam = clipOrthoPanels[m_clipTool.orthoIdx]->camera();
    const glm::vec3 forwardAxis = cam.forwardAxis();
    const glm::vec3 lineDir = m_clipTool.p2World - m_clipTool.p1World;
    const glm::vec3 crossv = glm::cross(forwardAxis, lineDir);
    if (glm::length(crossv) < kPlaneEpsilon) {
        Log::editor()->warn("[clip] plano degenerado (linea paralela "
                              "al forwardAxis), abortando");
        cancelClipTool();
        return;
    }
    Plane clipPlane;
    clipPlane.normal   = glm::normalize(crossv);
    clipPlane.distance = -glm::dot(clipPlane.normal, m_clipTool.p1World);

    // Recolectar brushes selectos (active + multi-select). Si no hay
    // ninguno, log warn + abortar.
    std::vector<Entity> targets;
    SelectionSet& set = m_ui.selectionSet();
    for (const Entity& e : set.selected) {
        if (e && e.hasComponent<BrushComponent>() &&
            e.hasComponent<TransformComponent>()) {
            targets.push_back(e);
        }
    }
    if (targets.empty()) {
        Log::editor()->warn(
            "[clip] no hay brushes seleccionados — selecciona uno con "
            "el tool Selecc primero, luego activa Clip de nuevo");
        m_ui.setStatusMessage(
            "Clip: selecciona un brush primero (Selecc -> click brush -> Clip)");
        cancelClipTool();
        return;
    }

    const bool keepFront = (m_clipTool.keepMode == ClipKeepMode::Front
                             || m_clipTool.keepMode == ClipKeepMode::Both);
    const bool keepBack  = (m_clipTool.keepMode == ClipKeepMode::Back
                             || m_clipTool.keepMode == ClipKeepMode::Both);

    // Por cada brush target: snapshot pre + computar split en world +
    // empujar BooleanOpCommand individual al HistoryStack. Multiple
    // commands -> Ctrl+Z deshace uno por uno (acceptable trade-off
    // por simplicidad).
    int totalSpawned = 0;
    int totalDestroyed = 0;
    for (Entity target : targets) {
        if (!m_scene->registry().valid(target.handle())) continue;

        SavedBrush aSnap = snapshotBrushClip(target, *m_assetManager);

        // Convertir el brush a world space para clipear con el plano
        // que esta en world coords. Mismo helper que F2H12 (re-imple
        // para no exponer al header).
        const auto& bcA = target.getComponent<BrushComponent>();
        const auto& tA = target.getComponent<TransformComponent>();
        const glm::mat4 worldM = tA.worldMatrix();
        const glm::mat3 normalM = glm::inverseTranspose(glm::mat3(worldM));
        Csg::Brush worldBrush;
        worldBrush.faces.reserve(bcA.brush.faces.size());
        for (const auto& f : bcA.brush.faces) {
            const glm::vec3 worldNormal =
                glm::normalize(normalM * f.plane.normal);
            const glm::vec3 pointLocal = -f.plane.distance * f.plane.normal;
            const glm::vec3 pointWorld =
                glm::vec3(worldM * glm::vec4(pointLocal, 1.0f));
            Csg::BrushFace wf;
            wf.plane = Mood::planeFromPointAndNormal(pointWorld, worldNormal);
            wf.materialIndex = f.materialIndex;
            wf.uAxis        = f.uAxis;
            wf.vAxis        = f.vAxis;
            wf.uvOffset     = f.uvOffset;
            wf.uvScale      = f.uvScale;
            wf.uvRotation   = f.uvRotation;
            wf.lockToWorld  = f.lockToWorld;
            worldBrush.faces.push_back(wf);
        }
        worldBrush.localAabb = Csg::computeBrushAabb(worldBrush);

        std::vector<Csg::Brush> sides = Csg::clipBrushByPlane(
            worldBrush, clipPlane, keepFront, keepBack);

        const MaterialAssetId heritedMat = bcA.materials.empty()
            ? 0u : bcA.materials[0];
        const std::string heritedMatPath = (heritedMat == 0)
            ? std::string{} : m_assetManager->materialPathOf(heritedMat);

        // Snapshot de cada side: en el flow de BooleanOpCommand, los
        // resultSnaps deben quedar con planos en LOCAL space respecto
        // a la posicion del brush nuevo. Centrar cada uno en su
        // centroide.
        std::vector<SavedBrush> resultSnaps;
        std::vector<std::string> reservedTags;
        resultSnaps.reserve(sides.size());
        reservedTags.reserve(sides.size());
        for (const auto& side : sides) {
            const std::string tag = uniqueClipTag(*m_scene, "Brush_Clip",
                                                    reservedTags);
            reservedTags.push_back(tag);
            // Centroide en world; rebase planos a local.
            const glm::vec3 centroid = side.localAabb.center();
            Csg::Brush localBrush;
            localBrush.faces.reserve(side.faces.size());
            for (const auto& f : side.faces) {
                Csg::BrushFace lf;
                lf.plane.normal   = f.plane.normal;
                lf.plane.distance = f.plane.distance
                                  + glm::dot(f.plane.normal, centroid);
                lf.materialIndex = f.materialIndex;
                lf.uAxis        = f.uAxis;
                lf.vAxis        = f.vAxis;
                lf.uvOffset     = f.uvOffset;
                lf.uvScale      = f.uvScale;
                lf.uvRotation   = f.uvRotation;
                lf.lockToWorld  = f.lockToWorld;
                localBrush.faces.push_back(lf);
            }
            localBrush.localAabb = Csg::computeBrushAabb(localBrush);
            resultSnaps.push_back(snapshotResultLocal(
                localBrush, tag, centroid, glm::vec3(0.0f), glm::vec3(1.0f),
                heritedMatPath));
        }

        // Destruir el original.
        m_scene->destroyEntity(target);
        ++totalDestroyed;

        // Spawn de los brushes resultado.
        for (const auto& sb : resultSnaps) {
            Entity e = m_scene->createEntity(sb.tag);
            auto& t = e.getComponent<TransformComponent>();
            t.position      = sb.position;
            t.rotationEuler = sb.rotationEuler;
            t.scale         = sb.scale;
            BrushComponent bc;
            bc.brush.faces.reserve(sb.faces.size());
            for (const auto& sf : sb.faces) {
                Csg::BrushFace face;
                face.plane.normal   = sf.normal;
                face.plane.distance = sf.distance;
                face.materialIndex  = sf.materialIndex;
                bc.brush.faces.push_back(face);
            }
            bc.brush.localAabb = Csg::computeBrushAabb(bc.brush);
            bc.materials = {0};
            bc.materials[0] = (sb.materialPath.empty())
                ? 0u : m_assetManager->loadMaterial(sb.materialPath);
            bc.dirty = true;
            e.addComponent<BrushComponent>(std::move(bc));
            ++totalSpawned;
        }

        // Push command: bSnapshot vacio (no hay B en clip).
        SavedBrush bEmpty{};
        auto cmd = std::make_unique<BooleanOpCommand>(
            BooleanOpKind::Clip,
            std::move(aSnap),
            std::move(bEmpty),
            std::move(resultSnaps),
            m_scene.get(),
            m_assetManager.get(),
            "Clip brush " + (target.hasComponent<TagComponent>()
                ? target.getComponent<TagComponent>().name
                : std::string{"(?)"}));
        m_history.push(std::move(cmd));
    }

    Log::editor()->info("[clip] confirmado: {} destroyed, {} spawned "
                          "(keepMode={})",
                          totalDestroyed, totalSpawned,
                          (m_clipTool.keepMode == ClipKeepMode::Front) ? "Front"
                          : (m_clipTool.keepMode == ClipKeepMode::Back) ? "Back"
                          : "Both");

    // Cerrar la sesion.
    m_clipTool = ClipToolSession{};
}

} // namespace Mood
