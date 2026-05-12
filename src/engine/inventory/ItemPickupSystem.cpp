#include "engine/inventory/ItemPickupSystem.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/dialog/DialogSystem.h"      // isActive() check (defensive)
#include "engine/game/state/GameState.h"
#include "engine/i18n/I18n.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <entt/entt.hpp>

#include <string>

namespace Mood::Inventory::ItemPickupSystem {

namespace {

// Hook Lua registrado por el dev del juego (Bloque F). nullptr = no
// registrado, el motor sigue funcionando sin disparar reacciones.
PickupHook g_pickupHook;

// Cache del player entity. Re-scanea si el handle cacheado se invalida
// (scene reload / destroy / nueva escena). Mismo patron que el cache
// de cachedDialogId pero a nivel sistema, no componente.
entt::entity g_cachedPlayer = entt::null;

// Flag para distinguir "interact_prompt seteado por este sistema" de
// uno seteado por Lua. Cuando el player sale del trigger Y nosotros
// habiamos seteado el prompt, lo limpiamos.
bool g_lastSetByPickup = false;

/// @brief Resuelve el player entity buscando el primero con
///        TagComponent.name == "Player". Cachea el handle. Si el cache
///        sigue valido, lo devuelve sin re-escanear.
/// @return Entity valida o un Entity falsy si no hay player en la
///         escena.
Entity resolvePlayer(Scene& scene) {
    auto& reg = scene.registry();
    // Cache hit: validar y devolver.
    if (g_cachedPlayer != entt::null && reg.valid(g_cachedPlayer)) {
        // Confirmar que sigue teniendo el tag "Player" Y InventoryComponent.
        if (reg.all_of<TagComponent, InventoryComponent>(g_cachedPlayer)) {
            const auto& tag = reg.get<TagComponent>(g_cachedPlayer);
            if (tag.name == "Player") {
                return Entity(g_cachedPlayer, &scene);
            }
        }
        g_cachedPlayer = entt::null;
    }
    // Re-scan.
    Entity result;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (result) return;  // ya encontramos, lambda no puede break
        if (tag.name == "Player" && e.hasComponent<InventoryComponent>()) {
            result = e;
        }
    });
    if (result) {
        g_cachedPlayer = result.handle();
    }
    return result;
}

/// @brief Nombre legible del item para el prompt. Heuristica:
///        - name_literal si no vacio.
///        - sino name_key resuelto via I18n (si tiene traduccion la usa;
///          si no, devuelve la key misma — fail-safe).
///        - sino id del item (sentinela de fabrica).
std::string displayNameOf(const Inventory::Asset& asset) {
    if (!asset.name_literal.empty()) return asset.name_literal;
    if (!asset.name_key.empty())     return Mood::I18n::T(asset.name_key);
    return asset.id;
}

} // namespace

void setPickupHook(PickupHook fn) {
    g_pickupHook = std::move(fn);
}

void clearHooks() {
    g_pickupHook = nullptr;
}

void resetPlayerCache() {
    g_cachedPlayer = entt::null;
    g_lastSetByPickup = false;
}

bool tick(Scene& scene, AssetManager& assets, bool eJustPressed,
          bool dialogActiveThisFrame) {
    auto& hud = Mood::GameState::hud();

    // Si hay dialog activo: skipear todo (la tecla E le pertenece al
    // dialog). Limpiar prompt si nosotros lo habiamos seteado.
    if (dialogActiveThisFrame) {
        if (g_lastSetByPickup) {
            hud.interact_prompt.clear();
            g_lastSetByPickup = false;
        }
        return false;
    }

    Entity player = resolvePlayer(scene);
    bool anyPromptShown = false;
    bool pickedUpThisFrame = false;
    // Coleccion deferred — destroyEntity invalida iteradores del view.
    std::vector<entt::entity> toDestroy;

    scene.forEach<TriggerComponent, ItemPickupComponent>(
        [&](Entity e, TriggerComponent& tr, ItemPickupComponent& ip) {
            if (ip.itemPath.empty()) return;
            if (!tr.playerInside) return;
            if (!player) return;  // no player en escena -> no se puede pickeear

            // Resolver el asset (cache en el componente).
            if (ip.cachedItemId == 0) {
                ip.cachedItemId = assets.loadItem(ip.itemPath);
            }
            const Inventory::Asset* asset = assets.getItem(ip.cachedItemId);
            if (asset == nullptr) return;  // defensive — getItem nunca null pero

            // Setear prompt (idempotente si re-evaluamos el mismo frame).
            const std::string name = displayNameOf(*asset);
            hud.interact_prompt = Mood::I18n::T("hud.pickup.interact_prompt", name);
            g_lastSetByPickup = true;
            anyPromptShown = true;

            if (!eJustPressed) return;

            // Player apreto E: intentar add al inventario.
            auto& inv = player.getComponent<InventoryComponent>();
            const bool added = inv.state.add(ip.cachedItemId, ip.quantity, assets);
            if (!added) {
                // Capacity full o regla de stack rota. Dejar el prompt
                // visible — el player intentara despues tras tirar algo.
                // (En el futuro, mostrar "Inventario lleno"; v1 no.)
                return;
            }

            // Notificacion HUD reusando el pickup queue de F2H39.
            const std::string notif = Mood::I18n::T(
                "hud.pickup.notification", name, ip.quantity);
            Mood::GameState::pushPickup(notif.c_str());

            // Hook Lua (si registrado por el dev del juego).
            if (g_pickupHook) {
                g_pickupHook(ip.itemPath, ip.quantity);
            }

            pickedUpThisFrame = true;

            // Limpiar prompt: el item ya fue al inventario.
            hud.interact_prompt.clear();
            g_lastSetByPickup = false;
            anyPromptShown = false;

            if (ip.destroyOnPickup) {
                toDestroy.push_back(e.handle());
            }
        });

    // Destruccion deferred (no invalida el view).
    for (auto handle : toDestroy) {
        scene.destroyEntity(Entity(handle, &scene));
    }

    // Si el player no esta en NINGUN trigger pickup y nosotros habiamos
    // seteado el prompt, limpiarlo.
    if (!anyPromptShown && g_lastSetByPickup) {
        hud.interact_prompt.clear();
        g_lastSetByPickup = false;
    }
    return pickedUpThisFrame;
}

} // namespace Mood::Inventory::ItemPickupSystem
