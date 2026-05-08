// F2H24: nucleo de DemoSpawners. Cada handler vive en archivos
// parciales por dominio:
//   _Basic.cpp   — demos simples (rotator, HUD, physics box, etc.)
//   _Stress.cpp  — demos pesados / showcase (enemy, shadow, PBR grid,
//                  light stress, animated char, stress tris)
//   _Prefab.cpp  — guardar/instanciar prefab
//   _Drop.cpp    — drops del viewport (textura, mesh, material, script)
//
// Aca solo vive `pushCreatedEntities`, helper compartido para empujar
// CreateEntityCommand al HistoryStack.

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
