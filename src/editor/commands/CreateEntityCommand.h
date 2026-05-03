#pragma once

// CreateEntityCommand (Hito 28): undoable creation de una o varias entidades.
//
// Estrategia simetrica a DeleteEntityCommand: el callsite ya creo la(s)
// entidad(es) (con sus componentes/animator/etc); este comando captura
// un SAVED-ENTITY snapshot por entidad en el ctor. El primer `execute()`
// (cuando el HistoryStack hace `push`) es no-op porque las entidades
// ya viven. `undo()` destruye (cleanup de bodies de Jolt incluido).
// El siguiente `redo()` re-llama `execute()` que esta vez SI recrea
// las entidades desde los snapshots via `SceneLoader::applyOneEntity`.
//
// Caveat de handles: igual que en DeleteEntityCommand, el handle EnTT
// cambia cada vez que se recrea. Otros comandos del history que apunten
// al handle viejo quedan no-op silenciosos (m_registry.valid() falla).

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/serialization/SceneSerializer.h"  // SavedEntity

#include <functional>
#include <string>
#include <vector>

namespace Mood {

class AssetManager;
class Scene;

class CreateEntityCommand : public ICommand {
public:
    /// Callback opcional para cleanup del body de Jolt (igual contrato
    /// que `DeleteEntityCommand::BodyCleanup`).
    using BodyCleanup = std::function<void(u32 bodyId)>;

    /// @param created Entidades que el callsite acaba de crear y ya pobló
    ///        con sus componentes. El ctor las serializa a snapshot.
    ///        Si `created` esta vacio, el comando es no-op.
    /// @param scene Scene dueño.
    /// @param assets Para resolver paths al recrear.
    /// @param bodyCleanup Callback opcional para Jolt.
    /// @param actionLabel Texto corto para el menu Editar > Deshacer.
    ///        Ej. "Spawn rotador demo", "Drop mesh barrel.glb".
    CreateEntityCommand(std::vector<Entity> created,
                         Scene* scene,
                         AssetManager* assets,
                         BodyCleanup bodyCleanup = {},
                         std::string actionLabel = "Crear");

    void execute() override;
    void undo() override;
    std::string name() const override;
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

private:
    void destroyAllAlive();
    void recreateFromSnapshots();

    Scene*                   m_scene;
    AssetManager*            m_assets;
    BodyCleanup              m_bodyCleanup;
    std::vector<SavedEntity> m_snapshots;
    std::vector<Entity>      m_alive;  // entidades vivas; vaciada tras undo, rellenada por redo.
    std::string              m_label;
};

} // namespace Mood
