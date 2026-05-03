#pragma once

// Helper compartido editor <-> player: carga un `.moodmap` desde disco
// y aplica sus entidades a una Scene. Hito 21 Bloque 4: extraido de
// `EditorApplication::tryOpenProjectPath` para que MoodPlayer lo
// reuse al cargar el proyecto del game.json.
//
// El flujo es:
//   1) `SceneSerializer::load(path, assets)` -> SavedMap
//   2) Aplicar `savedMap.map` al GridMap de salida + rebuildSceneFromMap.
//   3) Aplicar las entidades persistidas (mesh + light + rigidbody +
//      environment + prefab link) al Scene.
//
// Antes el (3) vivia inline en EditorProjectActions; ahora lo expone
// `applyEntitiesToScene` para que el player tambien lo use.

#include "engine/scene/core/Entity.h"

#include <filesystem>

namespace Mood {

class AssetManager;
class Entity;
class Scene;
struct SavedEntity;
struct SavedMap;

namespace SceneLoader {

/// @brief Aplica las entidades no-tile del SavedMap al `scene`. Asume
///        que `rebuildSceneFromMap` ya creo las entidades-tile via el
///        GridMap; este helper agrega encima Mesh / Light / RigidBody /
///        Environment / PrefabLink segun lo persistido.
///
///        Hace upgrades de back-compat: paths v6 (texturas en lugar de
///        materials) los envuelve en wrappers; tonemap/fogMode strings
///        viejos se mapean a los enums actuales.
void applyEntitiesToScene(const SavedMap& saved,
                          Scene& scene,
                          AssetManager& assets);

/// @brief Aplica UNA `SavedEntity` al scene. Devuelve la Entity creada.
///        Util para Hito 27 DeleteEntityCommand::undo: recrear una
///        entidad borrada desde su snapshot pre-delete.
Entity applyOneEntity(const SavedEntity& saved,
                      Scene& scene,
                      AssetManager& assets);

} // namespace SceneLoader

} // namespace Mood
