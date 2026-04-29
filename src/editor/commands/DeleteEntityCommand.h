#pragma once

// DeleteEntityCommand (Hito 27 Bloque 4): undoable delete de una entidad.
//
// Estrategia: capturar un SAVED-ENTITY snapshot ANTES de borrar (via
// EntitySerializer + SceneSerializer). El execute() destruye la entidad
// (incluyendo cleanup del body de Jolt si aplica). El undo() recrea la
// entidad desde el snapshot via SceneLoader::applyOneEntity, y guarda
// el nuevo handle para que un siguiente redo destruya el correcto.
//
// Caveat: el handle EnTT cambia al recrear (versionado). Otros comandos
// del history que apuntaban al handle viejo (ej. EditTransformCommand
// previo a este delete) no podran aplicarse — `m_registry.valid(handle)`
// fallaria y son no-op silenciosos. Aceptable para v1; si se vuelve un
// problema visible se puede sumar un mapa "old handle -> new handle"
// en el HistoryStack.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/Entity.h"
#include "engine/serialization/SceneSerializer.h"  // SavedEntity

#include <string>

namespace Mood {

class AssetManager;
class PhysicsWorld;
class Scene;

class DeleteEntityCommand : public ICommand {
public:
    /// @param entity Entidad viva a borrar. La capturamos en snapshot
    ///        ANTES del primer execute().
    /// @param scene Scene dueño (necesario para destroyEntity + recreate).
    /// @param assets Para resolver mesh/textura paths al recrear.
    /// @param physics Opcional; si la entidad tiene RigidBodyComponent,
    ///        este wrapper destruye el body de Jolt al delete y deja
    ///        bodyId=0 en la nueva entidad para que se rematerialize.
    DeleteEntityCommand(Entity entity,
                         Scene* scene,
                         AssetManager* assets,
                         PhysicsWorld* physics);

    void execute() override;
    void undo() override;
    std::string name() const override;

private:
    void destroyAlive();

    Scene*        m_scene;
    AssetManager* m_assets;
    PhysicsWorld* m_physics;
    SavedEntity   m_snapshot;
    Entity        m_alive;   // valida tras ctor; vaciada tras execute(); rellenada por undo().
};

} // namespace Mood
