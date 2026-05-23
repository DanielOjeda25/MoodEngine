// Implementacion pura del helper collectHierarchyEntries (F2H5). Vive
// aparte del HierarchyPanel.cpp porque ese ultimo incluye imgui.h y los
// tests no linkean ImGui — tener el helper en su propio TU permite
// testearlo sin arrastrar la dependencia de UI.

#include "editor/panels/scene/HierarchyPanel.h"

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

namespace Mood {

void collectHierarchyEntries(Scene& scene,
                               std::vector<HierarchyEntry>& out) {
    out.clear();
    scene.forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        // F2H70.4 follow-up: las wheel-entities las maneja el VehicleSystem
        // (spawn/rematerializa desde el chassis); son internas, no se listan.
        // F2H82: marker en vez de check por nombre (autos importados tienen
        // ruedas con nombres arbitrarios).
        if (e.hasComponent<VehicleWheelMarker>()) return;
        out.push_back(HierarchyEntry{e.handle(), &tag});
    });
}

} // namespace Mood
