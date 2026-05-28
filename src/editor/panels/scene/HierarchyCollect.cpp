// Implementacion pura del helper collectHierarchyEntries (F2H5). Vive
// aparte del HierarchyPanel.cpp porque ese ultimo incluye imgui.h y los
// tests no linkean ImGui — tener el helper en su propio TU permite
// testearlo sin arrastrar la dependencia de UI.
//
// F3H27: reescrito para emitir DFS pre-order respetando jerarquia
// parent/child. Pre-F3H27 era flat (todas las entities en orden de
// entt::registry). Ahora:
//   - Roots primero (entities sin parent), iteradas en el orden
//     natural del registry.
//   - Por cada root, descender DFS: emitir hijos con depth+1 antes de
//     los hermanos del root.
//   - Skip subtrees colapsadas (no emite ni el padre ni los hijos —
//     wait, NO: emite el padre con `hasChildren=true`, skip los hijos).

#include "editor/panels/scene/HierarchyPanel.h"

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <algorithm>

namespace Mood {

namespace {

bool entityIsHidden(Entity e) {
    // F2H70.4 follow-up: las wheel-entities las maneja el VehicleSystem
    // (spawn/rematerializa desde el chassis); son internas, no se listan.
    if (e.hasComponent<VehicleWheelMarker>()) return true;
    // F3H22: Environment es un singleton implícito del proyecto — auto-
    // spawn al cargar/crear escena, accesible vía la categoría
    // "Environment" del Inspector. No-listable en el Outliner.
    if (e.hasComponent<EnvironmentComponent>()) return true;
    return false;
}

} // namespace

void collectHierarchyEntries(Scene& scene,
                               std::vector<HierarchyEntry>& out,
                               const std::unordered_set<entt::entity>& collapsed) {
    out.clear();

    // Pass 1: indexar handle -> tag + parent en orden registry.
    // Pass 2: para cada handle root (parent==null o invalido), DFS desde
    //         ese root emitiendo en pre-order.

    auto& reg = scene.registry();

    // Build child-list map: para cada parent, lista de hijos (handles).
    // Ojo: usamos vector + dedupe-by-iteration en lugar de unordered_map<
    // entt::entity, vector> para evitar inflate sin escenas grandes. Para
    // escenas chicas (típico < 1K entries) el O(N*K) es aceptable.
    //
    // Actualmente prefiero claridad sobre micro-perf: el panel-side ya
    // limita el set con clipper, y F2H5 mostro que con 8K entries el
    // bottleneck no era esta función.

    std::vector<entt::entity> roots;
    roots.reserve(64);
    reg.view<TagComponent>().each([&](entt::entity h, TagComponent&) {
        Entity e(h, &scene);
        if (entityIsHidden(e)) return;
        if (reg.all_of<TransformComponent>(h)) {
            const auto& tc = reg.get<TransformComponent>(h);
            if (tc.parent == entt::null) {
                roots.push_back(h);
            } else if (!reg.valid(tc.parent)) {
                // Padre invalido (recreado tras undo/redo o stale) =>
                // tratar como root para evitar "se queda fuera del arbol".
                roots.push_back(h);
            }
        } else {
            roots.push_back(h);
        }
    });

    // DFS pre-order.
    auto isVisible = [&](entt::entity h) -> bool {
        if (!reg.valid(h)) return false;
        if (!reg.all_of<TagComponent>(h)) return false;
        return !entityIsHidden(Entity(h, &scene));
    };

    auto hasAnyChild = [&](entt::entity parent) -> bool {
        bool found = false;
        reg.view<TransformComponent>().each(
            [&](entt::entity h, TransformComponent& tc) {
                if (found) return;
                if (tc.parent == parent && isVisible(h)) found = true;
            });
        return found;
    };

    auto emit = [&](entt::entity h, int depth) {
        const auto& tag = reg.get<TagComponent>(h);
        const bool kids = hasAnyChild(h);
        out.push_back(HierarchyEntry{h, &tag, depth, kids});
    };

    // DFS stack: pares (handle, depth). Iteramos para emitir hijos
    // inmediatamente después del padre (pre-order).
    std::vector<std::pair<entt::entity, int>> stack;
    stack.reserve(64);
    for (entt::entity root : roots) {
        stack.clear();
        stack.push_back({root, 0});
        while (!stack.empty()) {
            const auto [cur, dep] = stack.back();
            stack.pop_back();
            if (!isVisible(cur)) continue;
            emit(cur, dep);
            // Skip hijos si la subtree esta colapsada.
            if (collapsed.count(cur) > 0) continue;
            // Recolectar hijos en el orden del registry, push en reverse
            // para que el primer hijo quede en el tope del stack y se
            // emita primero al continuar.
            std::vector<entt::entity> kids;
            reg.view<TransformComponent>().each(
                [&](entt::entity h, TransformComponent& tc) {
                    if (tc.parent != cur) return;
                    if (!isVisible(h)) return;
                    kids.push_back(h);
                });
            for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
                stack.push_back({*it, dep + 1});
            }
        }
    }
}

} // namespace Mood
