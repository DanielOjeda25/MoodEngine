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

#include <glm/gtc/matrix_transform.hpp>  // F2H29 Bloque C: glm::scale en spawnBoxBrushAt.

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

void EditorApplication::spawnBoxBrushAt(const glm::mat4& transform) {
    // F2H29 Bloque C: variante con transform custom (block tool del
    // workspace "Editor de mapas" la usa con la matriz construida desde
    // el rectangulo dibujado por el dev).
    //
    // El `transform` recibido es `T(center) * S(dims)` (sin rotacion —
    // el block tool no rota). Si pasaramos eso directo a makeBoxBrush,
    // los planos quedarian en world coords y el TransformComponent
    // del entity (con position default (0,1,0)) sumaria un OFFSET
    // visible — el gizmo aparece en (0,1,0) pero la geometria del
    // brush queda lejos. Mismo problema que F2H12 boolean ops resolvio
    // con `snapshotResultWorld` rebasando planos a local space.
    //
    // Fix: construir el brush en LOCAL space (solo con S(dims)) y
    // override el `tf.position` con el `center`. El gizmo queda en el
    // centro del mesh y el drag-edit funciona como se espera.
    if (!m_scene) return;
    const glm::vec3 center(transform[3].x, transform[3].y, transform[3].z);
    const glm::vec3 dims(
        glm::length(glm::vec3(transform[0])),
        glm::length(glm::vec3(transform[1])),
        glm::length(glm::vec3(transform[2])));
    const glm::mat4 localScale = glm::scale(glm::mat4(1.0f), dims);
    Entity e = spawnBrushEntity(*m_scene,
        Csg::makeBoxBrush(localScale),
        "Box", "Block tool Box Brush:");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = center;
    pushCreatedEntities({e}, "Block tool Box Brush");
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
