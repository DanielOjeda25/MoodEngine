// Nucleo del modulo (post-v2.0.2 cleanup). Cada handler vive en archivos
// parciales por dominio:
//   _Stress.cpp  — stress tests del menu Debug (LightStress, StressTris)
//   _Prefab.cpp  — guardar/instanciar prefab
//   _Drop.cpp    — drops del viewport (textura, mesh, material, script,
//                  item, vehicle)
//
// Aca solo vive `pushCreatedEntities`, helper compartido para empujar
// CreateEntityCommand al HistoryStack.
//
// Nota historica: nombre del modulo se mantiene "DemoSpawners" por costo
// de rename (cross-CMakeLists + git mv). Tras el cleanup de demos +
// spawners legacy, el nombre es algo enganoso (los archivos vivos hacen
// stress tests + drops + prefabs, no demos) pero no bloqueante. Rename
// queda como polish opcional.

#include "editor/application/EditorApplication.h"

#include "editor/commands/CreateEntityCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/physics/world/PhysicsWorld.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

void EditorApplication::pushCreatedEntities(std::vector<Entity> created,
                                              std::string label) {
    if (created.empty()) return;

    CreateEntityCommand::BodyCleanup cleanup;
    if (m_physicsWorld) {
        PhysicsWorld* pw = m_physicsWorld.get();
        cleanup = [pw](u32 bodyId) { pw->destroyBody(bodyId); };
    }
    auto cmd = std::make_unique<CreateEntityCommand>(
        std::move(created), m_scene.get(), m_assetManager.get(),
        std::move(cleanup), std::move(label));
    m_history.push(std::move(cmd));
    markDirty();
}

} // namespace Mood
