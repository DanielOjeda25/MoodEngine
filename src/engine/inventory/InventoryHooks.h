#pragma once

// InventoryHooks (F2H52 Bloque F): callbacks que el dev del juego
// registra una vez al start para reaccionar a eventos del inventario.
// Engine-grade strict: el motor NO conoce semantica game-specific —
// solo mueve items entre containers + invoca el hook apropiado cuando
// el evento ocurre. Es el dev quien define que pasa al levantar /
// soltar / usar un item escribiendo Lua.
//
// 3 eventos:
//
//   - on_pickup(item_path, qty): se dispara cuando el player levanta
//     una entity con ItemPickupComponent del mundo (via tecla E del
//     ItemPickupSystem). Disparado DESPUES de `inventory.add` exitoso.
//
//   - on_drop(item_path, qty): se dispara cuando el dev del juego (o
//     la UI del HUD del inventario en Bloque H via "Tirar") elimina
//     items del inventario explicitamente. Disparado DESPUES de
//     `inventory.remove` exitoso, solo cuando viene de un drop logico
//     (no se dispara en cada `inventory.remove` — solo en drops del
//     HUD o llamadas explicitas a `inventory.drop` cuando exista).
//
//   - on_use(entity, item_path): se dispara cuando se llama
//     `inventory.use(entity, path)`. NO auto-consume: el callback del
//     dev decide si hace `inventory.remove(...)` adentro (caso pocion)
//     o no (caso arma equipable). El motor solo invoca; la mecanica
//     vive en Lua.
//
// Hooks NO son veto-style (mismo patron F2H48.1 dialog hooks): el motor
// completa la operacion + dispara el callback como "after success
// notification". Si el dev necesita veto, implementa con
// `inventory.has` check antes de la accion.
//
// Multi-script: solo UN callback registrado a la vez por evento. Si el
// dev registra `on_use` desde un script Y de otro, el segundo overridea
// al primero (warn al log). Esto es intencional: simplicidad >
// composability en v1. Si emerge necesidad de N callbacks, sumar
// `add_listener(...)` aparte y deprecar el setter unico.

#include <functional>
#include <string>

namespace Mood {
class Entity;
} // namespace Mood

namespace Mood::Inventory::Hooks {

/// @brief Callback invocado tras `inventory.add` exitoso de un pickup
///        del mundo (via tecla E del ItemPickupSystem). Args:
///        (item_path, quantity).
using PickupHook = std::function<void(const std::string& item_path, int quantity)>;

/// @brief Callback invocado tras un drop explicito del inventario
///        (HUD "Tirar" o `inventory.drop` Lua). Args:
///        (item_path, quantity).
using DropHook = std::function<void(const std::string& item_path, int quantity)>;

/// @brief Callback invocado por `inventory.use(entity, path)`. NO se
///        auto-consume el item; el callback decide si remove o no.
///        Args: (entity, item_path).
using UseHook = std::function<void(Entity entity, const std::string& item_path)>;

/// @brief Registra el callback de pickup. Reemplaza el anterior si ya
///        habia uno (warn al log). nullptr para limpiar.
void setPickupHook(PickupHook fn);
void setDropHook  (DropHook   fn);
void setUseHook   (UseHook    fn);

/// @brief Limpia los 3 hooks. Llamar al final de la escena / tests.
void clearAll();

/// @brief Invocan los hooks registrados (no-op si no hay).
void invokePickup(const std::string& item_path, int quantity);
void invokeDrop  (const std::string& item_path, int quantity);
void invokeUse   (Entity entity, const std::string& item_path);

/// @brief Para tests: `hasXxxHook()` retorna true si hay callback
///        registrado. Las invocadas no se exponen para inspeccion
///        — usar lambdas captured-by-ref en los tests para verificar.
bool hasPickupHook();
bool hasDropHook  ();
bool hasUseHook   ();

} // namespace Mood::Inventory::Hooks
