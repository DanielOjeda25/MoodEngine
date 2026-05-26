#pragma once

// ComponentClipboard (F3H9): dispatcher por `componentKey` (string) entre
// el clipboard de EditorUI y el set de componentes Tier 1 soportados para
// copy/paste cross-entity.
//
// Tier 1 (F3H9): Light, Trigger, ForceField, ParticleEmitter — componentes
// "leaf" sin refs cruzadas a entities ni indices complejos.
//
// Patron:
//   1. Copy:  componentKey + serializeComponent(entity, assets) -> payload.
//             EditorUI guarda { componentKey, payload }.
//   2. Paste: applyPayload(componentKey, payload, target, assets) ->
//             agrega componente si falta + setea fields.
//   3. Undo:  PasteComponentCommand captura el snapshot pre-edit via
//             serializeComponent y restaura via applyPayload o removeComponent.
//
// El payload es un sub-object JSON del schema `.moodmap` — reusa
// EntitySerializer cero codigo nuevo de I/O.

#include "core/Types.h"
#include "engine/scene/core/Entity.h"

#include <nlohmann/json.hpp>

#include <string>

namespace Mood {

class AssetManager;

namespace ComponentClipboard {

// Component keys del schema EntitySerializer. Usar las constantes para
// evitar typos (no strings sueltos en call-sites).
constexpr const char* kKeyLight           = "light";
constexpr const char* kKeyTrigger         = "trigger";
constexpr const char* kKeyForceField      = "force_field";
constexpr const char* kKeyParticleEmitter = "particle_emitter";
// F3H10: extension a Tier 2 — types con SavedX en SavedEntity (serializables
// al `.moodmap` desde Fase 2).
constexpr const char* kKeyMeshRenderer    = "mesh_renderer";
constexpr const char* kKeyDialog          = "dialog";
constexpr const char* kKeyItemPickup      = "item_pickup";
constexpr const char* kKeyVehicle         = "vehicle";
constexpr const char* kKeyEnvironment     = "environment";
// F3H11: Tier 3 — el bundle de Audio/Camera/Brush que requeria cambios al
// EntitySerializer (Audio + Camera: gap F2 cerrado; Brush: refactor del
// serializer al header publico + applyBrushFromSaved helper extraido).
constexpr const char* kKeyAudioSource     = "audio_source";
constexpr const char* kKeyCamera          = "camera";
constexpr const char* kKeyBrush           = "brush";

/// @brief True si componentKey esta en el set Tier 1 soportado por F3H9.
bool isSupported(const std::string& componentKey);

/// @brief i18n key para mostrar el nombre del tipo del componente en la UI.
///        Devuelve "component.name.<key>" para keys soportados; cadena
///        vacia si no esta soportado.
std::string componentNameKey(const std::string& componentKey);

/// @brief True si `entity` tiene el componente identificado por componentKey.
///        Usado para gris-eado de "Pegar valores" (requiere componente
///        existente) vs "Pegar como nuevo" (requiere componente ausente).
bool entityHasComponent(const std::string& componentKey, const Entity& entity);

/// @brief Serializa el componente de `entity` identificado por componentKey
///        a JSON. Devuelve `null` JSON si la entidad no tiene el
///        componente o componentKey no es soportado. El payload tiene el
///        mismo schema que el sub-object del `.moodmap`.
nlohmann::json serializeComponent(const std::string& componentKey,
                                    Entity entity,
                                    const AssetManager& assets);

/// @brief Aplica `payload` a `entity`. Si la entidad NO tenia el componente,
///        lo agrega. Si lo tenia, sobrescribe sus fields. Devuelve true si
///        se aplico, false si componentKey no soportado o payload invalido.
///
/// @note  ParticleEmitter resuelve texturePath -> TextureAssetId via assets,
///        idempotente entre sesiones. Light/Trigger/ForceField son
///        independientes de assets.
bool applyPayload(const std::string& componentKey,
                   const nlohmann::json& payload,
                   Entity entity,
                   AssetManager& assets);

/// @brief Quita el componente identificado por componentKey de `entity`.
///        Usado por PasteComponentCommand::undo cuando hadComponentBefore=false
///        (paste como nuevo -> undo remueve). Devuelve true si lo quito,
///        false si componentKey no soportado o la entidad no lo tenia.
bool removeComponent(const std::string& componentKey, Entity entity);

} // namespace ComponentClipboard

} // namespace Mood
