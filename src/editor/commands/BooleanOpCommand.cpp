#include "editor/commands/BooleanOpCommand.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

#include <utility>

namespace Mood {

namespace {

/// @brief Reconstruye una entidad con BrushComponent desde un
///        SavedBrush. Mismo flow que SceneLoader::applyEntitiesToScene
///        para brushes — duplicado aqui para no acoplar el comando al
///        SceneLoader (que carga mapas enteros).
Entity recreateBrushEntity(Scene& scene, AssetManager* assets,
                            const SavedBrush& sb) {
    Entity e = scene.createEntity(sb.tag);
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
    bc.material = (sb.materialPath.empty() || assets == nullptr)
        ? 0u : assets->loadMaterial(sb.materialPath);
    bc.dirty = true;
    e.addComponent<BrushComponent>(std::move(bc));
    return e;
}

const char* opKindName(BooleanOpKind k) {
    switch (k) {
        case BooleanOpKind::Subtract:  return "Subtract";
        case BooleanOpKind::Union:     return "Union";
        case BooleanOpKind::Intersect: return "Intersect";
    }
    return "?";
}

} // namespace

BooleanOpCommand::BooleanOpCommand(BooleanOpKind kind,
                                    SavedBrush aSnapshot,
                                    SavedBrush bSnapshot,
                                    std::vector<SavedBrush> resultSnapshots,
                                    Scene* scene,
                                    AssetManager* assets,
                                    std::string label)
    : m_kind(kind),
      m_aSnapshot(std::move(aSnapshot)),
      m_bSnapshot(std::move(bSnapshot)),
      m_resultSnapshots(std::move(resultSnapshots)),
      m_scene(scene),
      m_assets(assets),
      m_label(std::move(label)) {}

void BooleanOpCommand::execute() {
    if (m_currentlyApplied) {
        // Primer push: el callsite ya destruyo A/B y creo los resultados.
        // No hacer nada.
        return;
    }
    // Redo tras undo: A/B viven en el scene, los resultados no. Hay que
    // destruir A/B y recrear los resultados.
    destroyByTags({m_aSnapshot.tag, m_bSnapshot.tag});
    recreateResults();
    m_currentlyApplied = true;
}

void BooleanOpCommand::undo() {
    // Estado actual: los resultados viven, A/B no.
    // Destruir resultados y recrear A/B.
    std::vector<std::string> resultTags;
    resultTags.reserve(m_resultSnapshots.size());
    for (const auto& r : m_resultSnapshots) resultTags.push_back(r.tag);
    destroyByTags(resultTags);
    recreateOriginals();
    m_currentlyApplied = false;
}

std::string BooleanOpCommand::name() const {
    return m_label;
}

void BooleanOpCommand::destroyByTags(const std::vector<std::string>& tags) {
    if (m_scene == nullptr) return;
    for (const auto& tag : tags) {
        Entity victim;
        m_scene->forEach<TagComponent>([&](Entity e, TagComponent& t) {
            if (!static_cast<bool>(victim) && t.name == tag) {
                victim = e;
            }
        });
        if (static_cast<bool>(victim)) {
            m_scene->destroyEntity(victim);
        } else {
            Log::editor()->warn(
                "BooleanOpCommand: no encontre entidad con tag '{}' "
                "para destruir", tag);
        }
    }
}

void BooleanOpCommand::recreateOriginals() {
    if (m_scene == nullptr) return;
    recreateBrushEntity(*m_scene, m_assets, m_aSnapshot);
    recreateBrushEntity(*m_scene, m_assets, m_bSnapshot);
}

void BooleanOpCommand::recreateResults() {
    if (m_scene == nullptr) return;
    for (const auto& sb : m_resultSnapshots) {
        recreateBrushEntity(*m_scene, m_assets, sb);
    }
}

} // namespace Mood
