#pragma once

// ItemPickupSystem (F2H52 Bloque C): conecta los Triggers con
// ItemPickupComponent al InventoryComponent del jugador. Cada frame de
// Play Mode:
//   1) Resuelve el "player entity" (primer entity con TagComponent.name
//      == "Player", cacheado al primer scan exitoso).
//   2) Itera entities con (TriggerComponent + ItemPickupComponent).
//   3) Si el player esta dentro de un trigger cuya entidad tiene
//      ItemPickupComponent valido (itemPath no vacio) y el dialog NO
//      esta activo, muestra el interact_prompt del HUD ("[E] Levantar X").
//   4) Si `eJustPressed` y se cumplen las condiciones:
//      - Resuelve el ItemAssetId via AssetManager::loadItem (cacheado).
//      - Llama player.inventory.state.add(itemId, qty, am).
//      - Si add() succeded: pushPickup notif al HUD + invoca el hook
//        `on_pickup` (si registrado via setPickupHook desde Lua en
//        Bloque F) + destroy entity si destroyOnPickup=true.
//      - Si add() fail (capacity), no destruye y deja el prompt para
//        que el player lo intente despues.
//
// El motor NO sabe que un "iron_sword hace daño" — solo move items
// entre containers. La semantica de gameplay vive en el hook que el
// dev registra en Lua. Engine-grade strict (VISION 2026-05-12).
//
// Mismo patron que DialogInteractSystem (F2H48): el caller (EditorPlayMode
// / PlayerApplication) computa el flanco up->down de la tecla E.

#include "engine/scene/components/Components.h"  // Para ItemPickupComponent fwd...

#include <functional>
#include <string>

namespace Mood {

class AssetManager;
class Scene;

} // namespace Mood

namespace Mood::Inventory::ItemPickupSystem {

/// @brief Avanza un frame del sistema. `eJustPressed` true = el caller
///        detecto el flanco up->down de la tecla E este frame.
///        `dialogActiveThisFrame` true = un dialog esta abierto: el
///        sistema skipea el prompt y el handler de E (la prioridad de
///        E queda en el dialog).
/// @return true si este frame se levanto al menos un pickup (el caller
///         puede usar esto para skipear otros handlers de E si quiere
///         evitar double-consumo — v1 el dialog ya tiene prioridad).
bool tick(Scene& scene, AssetManager& assets, bool eJustPressed,
          bool dialogActiveThisFrame);

/// @brief Callback invocado tras un pickup exitoso (despues de add() y
///        antes de destroyEntity). Firma: (itemPath, quantity). El dev
///        del juego lo registra una vez al start via Lua (Bloque F).
///        Si no esta seteado, el motor sigue funcionando — solo no
///        dispara reacciones game-specific.
using PickupHook = std::function<void(const std::string& itemPath, int quantity)>;
void setPickupHook(PickupHook fn);

/// @brief Limpia el hook (testing).
void clearHooks();

/// @brief Resetea el cache del player entity. Llamar al cargar un
///        proyecto / cambiar de escena para forzar re-scan al proximo
///        tick. Idempotente.
void resetPlayerCache();

} // namespace Mood::Inventory::ItemPickupSystem
