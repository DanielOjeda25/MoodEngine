// F2H32 Bloque C: carve UI — Hammer-style boolean subtract automatico.
// El dev selecciona un brush A y clickea "Carve" en el toolbar; el
// handler resta del A todos los brushes que intersectan su AABB world.
// Los carvers se preservan; A se reemplaza por sus fragmentos.
//
// Reuso: Csg::subtract de F2H12 itera el algoritmo BSP-style; el
// resultado puede ser N >= 0 brushes (cero si A queda completamente
// contenido por algun carver, N > 1 si los carvers descomponen A).
// BooleanOpCommand cubre el undo (kind=Subtract con bSnapshot vacio).

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "editor/commands/BooleanOpCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"  // brushAabbWorld
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

SavedBrush snapshotBrushCarve(Entity e, const AssetManager& assets) {
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

Csg::Brush buildWorldBrushCarve(Entity e) {
    const auto& bc = e.getComponent<BrushComponent>();
    const auto& t = e.getComponent<TransformComponent>();
    const glm::mat4 worldM = t.worldMatrix();
    const glm::mat3 normalM = glm::inverseTranspose(glm::mat3(worldM));
    Csg::Brush wb;
    wb.faces.reserve(bc.brush.faces.size());
    for (const auto& f : bc.brush.faces) {
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
        wb.faces.push_back(wf);
    }
    wb.localAabb = Csg::computeBrushAabb(wb);
    return wb;
}

SavedBrush snapshotResultLocalCarve(const Csg::Brush& worldBrush,
                                     const std::string& tag,
                                     const std::string& materialPath) {
    SavedBrush sb;
    sb.tag = tag;
    sb.materialPath = materialPath;
    sb.rotationEuler = glm::vec3(0.0f);
    sb.scale         = glm::vec3(1.0f);
    const glm::vec3 centroid = worldBrush.localAabb.center();
    sb.position = centroid;
    for (const auto& f : worldBrush.faces) {
        SavedBrushFace sf;
        sf.normal        = f.plane.normal;
        sf.distance      = f.plane.distance
                           + glm::dot(f.plane.normal, centroid);
        sf.materialIndex = f.materialIndex;
        sb.faces.push_back(sf);
    }
    return sb;
}

std::string uniqueCarveTag(const Scene& scene,
                            const std::vector<std::string>& reserved) {
    int suffix = 1;
    while (true) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Brush_Carve_%02d", suffix);
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

void EditorApplication::handleCarve() {
    if (!m_scene || !m_assetManager) return;

    SelectionSet& set = m_ui.selectionSet();
    Entity active = set.active;
    if (!active || !active.hasComponent<BrushComponent>() ||
        !active.hasComponent<TransformComponent>()) {
        Log::editor()->warn(
            "[carve] active no es un brush — selecciona uno con Selecc tool");
        m_ui.setStatusMessage(
            "Carve: selecciona un brush con el tool Selecc primero");
        return;
    }

    // Recolectar carvers: todos los brushes con AABB world que
    // intersecte el AABB del active (broadphase). Los carvers NO se
    // destruyen en el flow Hammer: solo cortan al active.
    const auto& bcA = active.getComponent<BrushComponent>();
    const auto& tA = active.getComponent<TransformComponent>();
    const AABB aabbA = brushAabbWorld(tA, bcA);
    std::vector<Entity> carvers;
    m_scene->forEach<TransformComponent, BrushComponent>(
        [&](Entity e, TransformComponent& t, BrushComponent& bc) {
            if (e.handle() == active.handle()) return;
            const AABB aabbB = brushAabbWorld(t, bc);
            if (intersects(aabbA, aabbB)) {
                carvers.push_back(e);
            }
        });
    if (carvers.empty()) {
        Log::editor()->info(
            "[carve] no hay brushes intersectando al active — nada que cortar");
        m_ui.setStatusMessage("Carve: ningun brush intersecta al active");
        return;
    }

    // Snapshot pre del active (para el undo). Los carvers no se
    // tocan, no necesitan snapshot.
    SavedBrush aSnap = snapshotBrushCarve(active, *m_assetManager);

    // Iterativo: empezamos con [A_world]; por cada carver B,
    // remplazar fragments por la union de subtract(fragment, B).
    std::vector<Csg::Brush> fragments;
    fragments.push_back(buildWorldBrushCarve(active));
    int carverIdx = 0;
    for (Entity carver : carvers) {
        if (!m_scene->registry().valid(carver.handle())) continue;
        const Csg::Brush B = buildWorldBrushCarve(carver);
        std::vector<Csg::Brush> newFragments;
        for (const auto& frag : fragments) {
            std::vector<Csg::Brush> sub = Csg::subtract(frag, B);
            for (auto& s : sub) newFragments.push_back(std::move(s));
        }
        fragments = std::move(newFragments);
        ++carverIdx;
        if (fragments.empty()) {
            Log::editor()->info(
                "[carve] tras carver #{} ('{}') no quedan fragmentos "
                "(active completamente consumido)", carverIdx,
                carver.hasComponent<TagComponent>()
                    ? carver.getComponent<TagComponent>().name
                    : std::string{"?"});
            break;
        }
    }

    // Snapshot de los fragments en local space (rebase por centroide).
    const MaterialAssetId heritedMat = bcA.materials.empty()
        ? 0u : bcA.materials[0];
    const std::string heritedMatPath = (heritedMat == 0)
        ? std::string{} : m_assetManager->materialPathOf(heritedMat);
    std::vector<SavedBrush> resultSnaps;
    std::vector<std::string> reservedTags;
    resultSnaps.reserve(fragments.size());
    reservedTags.reserve(fragments.size());
    for (const auto& frag : fragments) {
        const std::string tag = uniqueCarveTag(*m_scene, reservedTags);
        reservedTags.push_back(tag);
        resultSnaps.push_back(snapshotResultLocalCarve(
            frag, tag, heritedMatPath));
    }

    Log::editor()->info(
        "[carve] '{}' carved by {} brushes -> {} fragments",
        aSnap.tag, carvers.size(), resultSnaps.size());

    // Destruir el active.
    m_scene->destroyEntity(active);

    // Spawn de los fragments. Las posiciones del SavedBrush ya estan
    // centradas (snapshotResultLocalCarve rebasea planos por centroide
    // y mete el centroide como sb.position).
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
    }

    // Push command. Reusa BooleanOpCommand con kind=Subtract pero
    // bSnapshot vacio (los carvers no se destruyeron, no hay que
    // recrearlos en undo). Sin "Clip" como kind — semanticamente
    // esto es un Subtract de A vs N carvers, donde el undo solo
    // necesita restaurar A y borrar los fragments.
    SavedBrush bEmpty{};
    auto cmd = std::make_unique<BooleanOpCommand>(
        BooleanOpKind::Subtract,
        std::move(aSnap),
        std::move(bEmpty),
        std::move(resultSnaps),
        m_scene.get(),
        m_assetManager.get(),
        "Carve brush");
    m_history.push(std::move(cmd));

    m_ui.setStatusMessage("Carve: completado");
}

} // namespace Mood
