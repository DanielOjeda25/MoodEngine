// F2H24: handleBooleanOp (F2H12) + helpers de snapshot/world transform.
// Subtract / Union / Intersect entre 2+ brushes seleccionados.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/BooleanOpCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushOps.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace Mood {

namespace {

/// @brief Captura un SavedBrush desde una entity con BrushComponent +
///        TransformComponent. Usado por el handler para snapshot
///        pre-op (que el comando undo va a usar para recrear A/B).
SavedBrush snapshotBrush(Entity e, const AssetManager& assets) {
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
        // F2H17: serializar todos los slots de material.
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

/// @brief Empaqueta un Csg::Brush (cuyos planos estan en WORLD space
///        tras la op booleana) + tag elegido en un SavedBrush con
///        transform centrado en el AABB y planos rebasados a local.
///
///        Sin esto, los brushes resultado quedan con position=(0,0,0)
///        y el gizmo aparece en el origen del mundo (UX rota — al
///        rotar/escalar lo hace alrededor de (0,0,0) en lugar del
///        centro del brush).
SavedBrush snapshotResultWorld(const Csg::Brush& worldBrush,
                                 const std::string& tag,
                                 const std::string& materialPath) {
    SavedBrush sb;
    sb.tag = tag;
    sb.materialPath = materialPath;
    sb.rotationEuler = glm::vec3(0.0f);
    sb.scale         = glm::vec3(1.0f);

    // Centroide en world = centro de la AABB del brush en world.
    const glm::vec3 centroid = worldBrush.localAabb.center();
    sb.position = centroid;

    // Planos en local: trasladar por -centroid. Plano world
    // dot(n,p) + d_w = 0  =>  con p = p_local + centroid:
    //   dot(n, p_local + centroid) + d_w = 0
    //   dot(n, p_local) + d_w + dot(n, centroid) = 0
    // => d_local = d_w + dot(n, centroid).
    for (const auto& f : worldBrush.faces) {
        SavedBrushFace sf;
        sf.normal        = f.plane.normal;
        sf.distance      = f.plane.distance + glm::dot(f.plane.normal, centroid);
        sf.materialIndex = f.materialIndex;
        sb.faces.push_back(sf);
    }
    return sb;
}

/// @brief Genera un tag unico para un brush resultado de op booleana.
///        @param reserved Tags que ya estan reservados para esta misma
///                        operacion (snapshots aun no creados como
///                        entidad). Evita que 2 llamadas consecutivas
///                        en el mismo batch devuelvan el mismo tag.
std::string uniqueResultTag(const Scene& scene, const std::string& base,
                              const std::vector<std::string>& reserved = {}) {
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

/// @brief Construye un Csg::Brush con los planos en world-space a
///        partir de un BrushComponent local + su Transform. Usado
///        antes de llamar a las ops booleanas para que operen en
///        un sistema de referencia comun.
Csg::Brush buildWorldBrush(Entity e) {
    const auto& bc = e.getComponent<BrushComponent>();
    const auto& t = e.getComponent<TransformComponent>();
    const glm::mat4 world = t.worldMatrix();
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(world));
    Csg::Brush worldBrush;
    worldBrush.faces.reserve(bc.brush.faces.size());
    for (const auto& f : bc.brush.faces) {
        const glm::vec3 worldNormal =
            glm::normalize(normalMatrix * f.plane.normal);
        const glm::vec3 pointLocal = -f.plane.distance * f.plane.normal;
        const glm::vec3 pointWorld =
            glm::vec3(world * glm::vec4(pointLocal, 1.0f));
        Csg::BrushFace wf;
        wf.plane = Mood::planeFromPointAndNormal(pointWorld, worldNormal);
        wf.materialIndex = f.materialIndex;
        worldBrush.faces.push_back(wf);
    }
    worldBrush.localAabb = Csg::computeBrushAabb(worldBrush);
    return worldBrush;
}

} // namespace

void EditorApplication::handleBooleanOp(EditorUI::BooleanOpRequestKind kind) {
    if (!m_scene || !m_assetManager) return;

    SelectionSet& set = m_ui.selectionSet();
    if (!static_cast<bool>(set.active) ||
        !set.active.hasComponent<BrushComponent>()) {
        Log::editor()->warn("BooleanOp: active no es un brush");
        return;
    }

    // Recolectar las A's: todos los seleccionados ≠ active con
    // BrushComponent. Tomar copias del Entity porque vamos a
    // modificar el set durante el loop (limpiar al final).
    Entity brushB = set.active;
    std::vector<Entity> brushAs;
    for (const Entity& e : set.selected) {
        if (e.handle() == brushB.handle()) continue;
        if (!e.hasComponent<BrushComponent>()) continue;
        brushAs.push_back(e);
    }
    if (brushAs.empty()) {
        Log::editor()->warn("BooleanOp: no hay A's en la seleccion (>=2 brushes requerido)");
        return;
    }

    const char* opName = "?";
    std::string resultBaseTag = "Brush_Result";
    BooleanOpKind cmdKind = BooleanOpKind::Subtract;
    // F2H13: Union/Intersect tienen semantica distinta del Subtract.
    //   Subtract: cascade per-A vs B; B se preserva (es la "tool").
    //   Union/Intersect: la op consume AMBOS brushes y produce
    //                    nuevos. Cascadear con N>2 no es trivial
    //                    (unionOp puede devolver N brushes y
    //                    iterar sobre eso es semanticamente
    //                    ambiguo). En F2H13 limitamos a N=2;
    //                    cascade real de Union/Intersect = hito
    //                    futuro si emerge necesidad.
    const bool preserveB = (kind == EditorUI::BooleanOpRequestKind::Subtract);
    switch (kind) {
        case EditorUI::BooleanOpRequestKind::Subtract:
            opName = "Subtract"; resultBaseTag = "Brush_Sub";
            cmdKind = BooleanOpKind::Subtract; break;
        case EditorUI::BooleanOpRequestKind::Union:
            opName = "Union"; resultBaseTag = "Brush_Union";
            cmdKind = BooleanOpKind::Union; break;
        case EditorUI::BooleanOpRequestKind::Intersect:
            opName = "Intersect"; resultBaseTag = "Brush_Int";
            cmdKind = BooleanOpKind::Intersect; break;
    }

    // Para Union/Intersect requerimos exactamente N=2 (1 A + 1 B).
    if (!preserveB && brushAs.size() != 1) {
        Log::editor()->warn(
            "BooleanOp {}: requiere exactamente 2 brushes seleccionados "
            "(N>2 = hito futuro)", opName);
        return;
    }

    // Snapshot de B una sola vez (Subtract: B preserva; Union/Intersect:
    // tambien snapshot porque el comando puede tener que recrearlo en undo).
    const SavedBrush bSnap = snapshotBrush(brushB, *m_assetManager);
    const Csg::Brush B = buildWorldBrush(brushB);

    // Para cada A: aplicar op(A, B), destroy A, crear resultados,
    // pushear command. El B se preserva.
    for (Entity brushA : brushAs) {
        if (brushA.handle() == brushB.handle()) continue;  // defensivo
        if (!m_scene->registry().valid(brushA.handle())) continue;

        SavedBrush aSnap = snapshotBrush(brushA, *m_assetManager);
        const Csg::Brush A = buildWorldBrush(brushA);

        std::vector<Csg::Brush> resultBrushes;
        switch (kind) {
            case EditorUI::BooleanOpRequestKind::Subtract:
                resultBrushes = Csg::subtract(A, B);
                break;
            case EditorUI::BooleanOpRequestKind::Union:
                resultBrushes = Csg::unionOp(A, B);
                break;
            case EditorUI::BooleanOpRequestKind::Intersect:
                if (auto r = Csg::intersectOp(A, B)) {
                    resultBrushes.push_back(std::move(*r));
                }
                break;
        }

        const auto& bcA = brushA.getComponent<BrushComponent>();
        const MaterialAssetId heritedMat = bcA.materials.empty()
            ? 0u : bcA.materials[0];
        const std::string heritedMatPath = (heritedMat == 0)
            ? std::string{} : m_assetManager->materialPathOf(heritedMat);

        std::vector<SavedBrush> resultSnaps;
        resultSnaps.reserve(resultBrushes.size());
        std::vector<std::string> reservedTags;
        reservedTags.reserve(resultBrushes.size());
        for (const auto& rb : resultBrushes) {
            const std::string tag = uniqueResultTag(*m_scene, resultBaseTag, reservedTags);
            reservedTags.push_back(tag);
            resultSnaps.push_back(snapshotResultWorld(rb, tag, heritedMatPath));
        }

        // F2H13 debug: log con detalle de los pedazos generados.
        std::string pieceList;
        for (const auto& sb : resultSnaps) {
            if (!pieceList.empty()) pieceList += ", ";
            pieceList += sb.tag;
        }
        Log::editor()->info("BooleanOp {} ({} - {}) -> {} pieces: [{}]",
                             opName, aSnap.tag, bSnap.tag,
                             resultSnaps.size(), pieceList);

        // Destruir A. Para Subtract, B se preserva; para Union /
        // Intersect, B tambien se destruye (la op consumio ambos
        // brushes en la representacion del resultado).
        m_scene->destroyEntity(brushA);
        if (!preserveB && m_scene->registry().valid(brushB.handle())) {
            m_scene->destroyEntity(brushB);
        }

        // Crear entidades resultado.
        for (const auto& sb : resultSnaps) {
            Entity e = m_scene->createEntity(sb.tag);
            auto& t = e.getComponent<TransformComponent>();
            t.position      = sb.position;
            t.rotationEuler = sb.rotationEuler;
            t.scale         = sb.scale;
            BrushComponent bc;
            for (const auto& sf : sb.faces) {
                Csg::BrushFace face;
                face.plane.normal   = sf.normal;
                face.plane.distance = sf.distance;
                face.materialIndex  = sf.materialIndex;
                bc.brush.faces.push_back(face);
            }
            bc.brush.localAabb = Csg::computeBrushAabb(bc.brush);
            // F2H17: cargar slots de material. Si materialPaths esta
            // vacio (caso del recreate sin info), fallback al
            // materialPath singular como slot 0.
            bc.materials.clear();
            if (!sb.materialPaths.empty()) {
                for (const auto& path : sb.materialPaths) {
                    bc.materials.push_back(
                        path.empty() ? 0u : m_assetManager->loadMaterial(path));
                }
            } else {
                bc.materials.push_back(sb.materialPath.empty()
                    ? 0u : m_assetManager->loadMaterial(sb.materialPath));
            }
            bc.dirty = true;
            e.addComponent<BrushComponent>(std::move(bc));
        }

        // Push command — uno por par (A, B). N Ctrl+Z para revertir
        // todo el cascade. CompoundCommand para agrupar es hito futuro.
        // El bSnap se copia (B se preserva entre iteraciones).
        m_history.push(std::make_unique<BooleanOpCommand>(
            cmdKind, std::move(aSnap), bSnap, std::move(resultSnaps),
            m_scene.get(), m_assetManager.get(),
            std::string{"Boolean "} + opName));
    }

    // Resetear seleccion. Para Subtract, B sigue vivo; para
    // Union/Intersect, todos los inputs fueron destruidos. En
    // ambos casos es mas seguro limpiar el set y dejar que el
    // user re-seleccione.
    Mood::clear(set);
    if (preserveB && m_scene->registry().valid(brushB.handle())) {
        Mood::add(set, brushB);
    }
}

} // namespace Mood
