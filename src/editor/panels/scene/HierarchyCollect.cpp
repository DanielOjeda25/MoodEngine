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
        out.push_back(HierarchyEntry{e.handle(), &tag});
    });
}

} // namespace Mood
