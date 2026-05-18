#pragma once

// DeleteEntityCommand (Hito 27 Bloque 4): undoable delete de una entidad.
//
// Estrategia: capturar un SAVED-ENTITY snapshot ANTES de borrar (via
// EntitySerializer + SceneSerializer). El execute() destruye la entidad
// (incluyendo cleanup del body de Jolt si aplica). El undo() recrea la
// entidad desde el snapshot via SceneLoader::applyOneEntity, y guarda
// el nuevo handle para que un siguiente redo destruya el correcto.
//
// Hito 32: handle remap. Cuando undo() recrea con un handle nuevo, el
// comando notifica al HistoryStack para que patchee a TODOS los demas
// comandos que apuntaban al handle viejo. Asi edit→delete→undo→undo
// vuelve a aplicar el edit original (antes era no-op silencioso).
// Requiere `HistoryStack*` en el ctor — opcional para tests headless.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/serialization/SceneSerializer.h"  // SavedEntity

#include <functional>
#include <string>

namespace Mood {

class AssetManager;
class HistoryStack;
class Scene;

class DeleteEntityCommand : public ICommand {
public:
    /// Callback opcional: invocado al ejecutar el delete con el bodyId de
    /// Jolt si la entidad tenia RigidBodyComponent. Permite cleanup
    /// (ej. PhysicsWorld::destroyBody) sin acoplar el comando al backend
    /// de fisica — los tests pasan {} y queda no-op.
    using BodyCleanup = std::function<void(u32 bodyId)>;

    /// F2H65: callback opcional analogo a BodyCleanup, para destruir el
    /// constraint asociado al JointComponent (si lo tiene). Inyectado por
    /// EditorApplication como lambda que llama PhysicsWorld::destroyConstraint.
    using ConstraintCleanup = std::function<void(u32 constraintId)>;

    /// @param entity Entidad viva a borrar. La capturamos en snapshot
    ///        ANTES del primer execute().
    /// @param scene Scene dueño (necesario para destroyEntity + recreate).
    /// @param assets Para resolver mesh/textura paths al recrear.
    /// @param bodyCleanup Callback opcional para cleanup del body de Jolt.
    ///        EditorApplication pasa una lambda que llama PhysicsWorld::destroyBody.
    /// @param history Opcional: si presente, el undo() llama
    ///        `remapEntityInStack(originalHandle, nuevoHandle)` tras recrear.
    /// @param constraintCleanup F2H65: opcional. Cleanup del constraint
    ///        de Jolt si la entidad tenia JointComponent.
    DeleteEntityCommand(Entity entity,
                         Scene* scene,
                         AssetManager* assets,
                         BodyCleanup bodyCleanup = {},
                         HistoryStack* history = nullptr,
                         ConstraintCleanup constraintCleanup = {});

    void execute() override;
    void undo() override;
    std::string name() const override;

private:
    void destroyAlive();

    Scene*            m_scene;
    AssetManager*     m_assets;
    BodyCleanup       m_bodyCleanup;
    ConstraintCleanup m_constraintCleanup;  // F2H65
    HistoryStack*     m_history;       // Hito 32: para disparar remap post-undo
    SavedEntity       m_snapshot;
    Entity            m_alive;          // valida tras ctor; vaciada tras execute(); rellenada por undo().
    entt::entity      m_originalHandle; // handle pre-delete; usado para el remap
};

} // namespace Mood
