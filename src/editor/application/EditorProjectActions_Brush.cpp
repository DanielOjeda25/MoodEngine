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

#include <algorithm>  // F2H30 Bloque C: std::max en height default.
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

void EditorApplication::togglePolygonDrawMode() {
    if (m_polyDraw.active) {
        cancelPolygonDraw();
        return;
    }
    // F2H30 fix: el pincel es un modal independiente del sub-modo. Si
    // entramos con Vertex/Edge/Face activo, el bloque 2.4e podia mover
    // un vertex del brush selecto al primer click — y la UI seguia
    // mostrando markers de vertex/edge encima del pincel. Reset a
    // Object al activar el pincel; al salir, queda en Object (sin
    // restore — pedido del dev "es poco claro").
    m_subMode = EditorSubMode::Object;
    m_ui.selectionSet().selectedFaceIndices.clear();  // F2H33
    m_polyDraw.active = true;
    m_polyDraw.orthoIdx = -1;
    m_polyDraw.axisIndex = 1;  // default Y; se overridea al primer click.
    m_polyDraw.pointsWorld.clear();
    Log::editor()->info("[pincel] modo activado — click en orto para agregar vertice");
}

void EditorApplication::cancelPolygonDraw() {
    if (!m_polyDraw.active) return;
    Log::editor()->info("[pincel] cancelado ({} puntos descartados)",
                          m_polyDraw.pointsWorld.size());
    m_polyDraw.active = false;
    m_polyDraw.orthoIdx = -1;
    m_polyDraw.pointsWorld.clear();
}

void EditorApplication::closePolygonDraw() {
    if (!m_polyDraw.active) return;
    if (!m_scene) {
        cancelPolygonDraw();
        return;
    }
    if (m_polyDraw.pointsWorld.size() < 3) {
        Log::editor()->warn(
            "[pincel] necesito >= 3 vertices para cerrar (tengo {})",
            m_polyDraw.pointsWorld.size());
        return;
    }

    // Centroide en world space (sobre el plano del view — el axis
    // perpendicular tiene componente 0).
    glm::vec3 centroid(0.0f);
    for (const auto& p : m_polyDraw.pointsWorld) centroid += p;
    centroid /= static_cast<f32>(m_polyDraw.pointsWorld.size());

    // Convertir a local (centrado en origen).
    std::vector<glm::vec3> localPoints;
    localPoints.reserve(m_polyDraw.pointsWorld.size());
    for (const auto& p : m_polyDraw.pointsWorld) {
        localPoints.push_back(p - centroid);
    }

    // Validar convexidad CCW. Si CW, invertir el orden.
    if (!Csg::isConvexPolygonCCW(localPoints, m_polyDraw.axisIndex)) {
        // Probar invirtiendo el orden — quizas el dev clickeo CW.
        std::vector<glm::vec3> reversed(localPoints.rbegin(),
                                          localPoints.rend());
        if (Csg::isConvexPolygonCCW(reversed, m_polyDraw.axisIndex)) {
            localPoints = std::move(reversed);
        } else {
            Log::editor()->warn(
                "[pincel] polígono no convexo o degenerado — no spawneo brush "
                "({} puntos)",
                m_polyDraw.pointsWorld.size());
            cancelPolygonDraw();
            return;
        }
    }

    // Altura default = snapStep * 4 (mismo que block tool).
    const f32 height = std::max(static_cast<f32>(m_hammerSnapStep) * 4.0f,
                                  4.0f);
    Csg::Brush brush = Csg::makePrismBrushFromPolygon(
        localPoints, height, m_polyDraw.axisIndex);
    if (brush.faces.empty()) {
        Log::editor()->warn(
            "[pincel] makePrismBrushFromPolygon devolvio brush vacio");
        cancelPolygonDraw();
        return;
    }

    Entity e = spawnBrushEntity(*m_scene, std::move(brush),
        "Poly", "Brush poligonal:");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = centroid;
    pushCreatedEntities({e}, "Brush poligonal");
    Log::editor()->info(
        "[pincel] cerrado: {} vertices, height {:.1f}, centroid=({:.2f}, "
        "{:.2f}, {:.2f})",
        localPoints.size(), height, centroid.x, centroid.y, centroid.z);
    m_polyDraw.active = false;
    m_polyDraw.orthoIdx = -1;
    m_polyDraw.pointsWorld.clear();
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
