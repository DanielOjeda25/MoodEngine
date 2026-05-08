// F2H24: handlers de spawn de primitivas CSG (F2H11+):
// Box / Cylinder / Sphere / Pyramid / Wedge / Prism (3-gono / 6-gono).

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/csg/Primitives.h"

#include <cstdio>
#include <string>

namespace Mood {

namespace {

/// @brief Helper compartido para spawn de cualquier primitiva CSG.
///        Crea entidad con tag unico (`Brush_<prefix>_NN`),
///        TransformComponent en (0, 1, 0), BrushComponent con la
///        Csg::Brush ya construida. Push al HistoryStack para undo.
Entity spawnBrushEntity(Scene& scene,
                         Csg::Brush brush,
                         const char* tagPrefix,
                         const char* opLabel) {
    int suffix = 1;
    std::string tagName;
    while (true) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Brush_%s_%02d", tagPrefix, suffix);
        tagName = buf;
        bool collision = false;
        scene.forEach<TagComponent>(
            [&](Entity, TagComponent& tag) {
                if (tag.name == tagName) collision = true;
            });
        if (!collision) break;
        ++suffix;
    }

    Entity e = scene.createEntity(tagName);
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 1.0f, 0.0f);
    t.scale    = glm::vec3(1.0f);

    BrushComponent bc;
    bc.brush = std::move(brush);
    bc.materials = {0};  // F2H17: 1 slot con material default
    bc.dirty = true;
    e.addComponent<BrushComponent>(std::move(bc));

    Log::editor()->info("{} '{}' en (0, 1, 0)", opLabel, tagName);
    return e;
}

} // namespace

void EditorApplication::handleAddBoxBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makeBoxBrush(glm::mat4(1.0f)),
        "Box", "Anadir Box Brush:");
    pushCreatedEntities({e}, "Anadir Box Brush");
}

void EditorApplication::handleAddCylinderBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makeCylinderBrush(glm::mat4(1.0f)),
        "Cyl", "Anadir Cylinder Brush:");
    pushCreatedEntities({e}, "Anadir Cylinder Brush");
}

void EditorApplication::handleAddSphereBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makeSphereBrush(glm::mat4(1.0f)),
        "Sph", "Anadir Sphere Brush:");
    pushCreatedEntities({e}, "Anadir Sphere Brush");
}

void EditorApplication::handleAddPyramidBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makePyramidBrush(glm::mat4(1.0f)),
        "Pyr", "Anadir Pyramid Brush:");
    pushCreatedEntities({e}, "Anadir Pyramid Brush");
}

void EditorApplication::handleAddWedgeBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makeWedgeBrush(glm::mat4(1.0f)),
        "Wed", "Anadir Wedge Brush:");
    pushCreatedEntities({e}, "Anadir Wedge Brush");
}

void EditorApplication::handleAddPrismTriangularBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makePrismBrush(glm::mat4(1.0f), 3),
        "PrTri", "Anadir Prism Triangular:");
    pushCreatedEntities({e}, "Anadir Prism Triangular");
}

void EditorApplication::handleAddPrismHexagonalBrush() {
    if (!m_scene) return;
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makePrismBrush(glm::mat4(1.0f), 6),
        "PrHex", "Anadir Prism Hexagonal:");
    pushCreatedEntities({e}, "Anadir Prism Hexagonal");
}

} // namespace Mood
