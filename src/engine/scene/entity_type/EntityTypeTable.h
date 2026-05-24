#pragma once

// EntityTypeTable (F3H9): tabla maestra del modelo EntityType.
// Centraliza:
//   - enum <-> string (para JSON + i18n keys + logs).
//   - base components per type (no quitables del Inspector).
//   - inference de type a partir de los componentes presentes
//     (back-compat para `.moodmap` pre-F3H9).
//
// Convencion alineada con Blender Object Type + Hammer entity class:
//   - Cada Type tiene 1 o 2 base components que definen su identidad.
//   - El base component se quita borrando la entity, no via Inspector.
//   - El Type se setea al spawn (handlers en EditorProjectActions_*) y
//     se mantiene durante toda la vida de la entity (solo cambia via
//     "Change entity type..." modal — Stage 8).

#include "core/Types.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/entity_type/EntityType.h"

#include <string>
#include <vector>

namespace Mood::EntityTypeTable {

/// Stringificacion para JSON + logs + i18n keys ("light", "trigger", etc).
/// Devuelve cadena vacia si type es invalido (defensive).
std::string toString(EntityType type);

/// Parseo inverso. Strings desconocidos -> Generic (defensive — no
/// rompemos al cargar `.moodmap` con types que no conoce el binario
/// actual; el dev podra cambiar el type via Convert).
EntityType fromString(const std::string& s);

/// i18n key para mostrar el type label en UI (ej. "entity_type.light"
/// que devuelve "Luz" en es / "Light" en en).
std::string i18nKey(EntityType type);

/// Component keys (mismo vocabulario que ComponentClipboard) que son
/// "base" para este type: no se pueden quitar del Inspector via "Remove
/// component". Para quitar, el dev borra la entity entera.
///
/// Tipos compuestos (NPC, Pickable) devuelven 2 component keys.
std::vector<std::string> baseComponentKeys(EntityType type);

/// True si `componentKey` es base del `type`. Helper para el Inspector
/// — disablea "Remove component" cuando matchea.
bool isBaseComponent(EntityType type, const std::string& componentKey);

/// True si `componentKey` se puede AGREGAR a una entity del `type` via
/// el popup "Add Component". Combina 2 reglas:
///   - Bases siempre van con la entity (no se agregan, vienen al spawn).
///     → false si es base.
///   - Extensions van segun la lista por type. Generic = todo permitido;
///     Tile = nada permitido (read-only). Otros types = whitelist
///     conservadora (alineada con Hammer/Blender: una Light NO acepta
///     BrushComponent, un Brush NO acepta LightComponent, etc).
bool canAddComponent(EntityType type, const std::string& componentKey);

/// Infiere el type a partir de los componentes presentes en `entity`.
/// Usado al cargar `.moodmap` pre-F3H9 que no tiene la key
/// `entity_type` en el JSON.
///
/// Orden de chequeo (de mas especifico a menos):
///   Brush > NPC (Trigger+Dialog) > Pickable (Trigger+ItemPickup) >
///   Environment > Light > Camera > ParticleEmitter > ForceField >
///   AudioSource > Trigger (solo) > Mesh > Tile (auto-gen) > Generic.
///
/// Acepta `tagNameHint` opcional para detectar Tile/Floor sin
/// necesidad de pasar la entity (al deserializar antes de materializar
/// el componente).
EntityType inferFromEntity(Entity entity);
EntityType inferFromEntityWithTag(Entity entity, const std::string& tagNameHint);

} // namespace Mood::EntityTypeTable
