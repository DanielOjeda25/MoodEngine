// F2H52 Bloque E — bindings Lua de la tabla `inventory`. Split del
// LuaBindings.cpp principal por tamaño (regla soft de ~500 lineas por
// archivo).
//
// API expuesta a los scripts:
//   - Lecturas:
//       inventory.has(entity, path, min_qty?)    -- bool
//       inventory.has(path, min_qty?)            -- bool (player implicit)
//       inventory.count(entity, path)            -- int
//       inventory.count(path)                    -- int  (player implicit)
//       inventory.entries(entity)                -- table {item_path, qty, slot_index}
//       inventory.entries()                      -- table (player implicit)
//       inventory.sum_stat(entity, stat_name)    -- float
//       inventory.sum_stat(stat_name)            -- float (player implicit)
//
//   - Mutaciones:
//       inventory.add(entity, path, qty)         -- bool
//       inventory.add(path, qty)                 -- bool  (player implicit)
//       inventory.remove(entity, path, qty)      -- bool
//       inventory.remove(path, qty)              -- bool  (player implicit)
//
//   - Spawn:
//       inventory.spawn_pickup(path, x, y, z, qty?) -- bool (true si entity creada)
//
// "Player implicit": cuando el primer arg es un string (path) en lugar
// de Entity, el motor busca la entity con `TagComponent.name == "Player"`
// en la scene del `self` actual y la usa como target. Si no existe
// (no hay player), retorna false/0/{} con un warn por log.
//
// Engine-grade: el motor SOLO mueve items entre containers y resuelve
// stats. La semantica game-specific (que hace cada item al usarse)
// vive en los hooks `on_pickup/on_drop/on_use` registrados desde Lua
// (Bloque F).

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/InventoryHooks.h"   // F2H52 Bloque F
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <string>
#include <vector>

namespace Mood {

namespace {

/// @brief Busca la entity con TagComponent.name == "Player" y que tenga
///        InventoryComponent. Si no hay, devuelve Entity falsy.
///        Re-escanea cada call (sin cache): los scripts NO se llaman
///        cada frame para queries de inventory, asi que es OK. Si emerge
///        feedback de performance, cachear con invalidacion por scene-load.
Entity findPlayer(Scene& scene) {
    Entity result;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (result) return;
        if (tag.name == "Player" && e.hasComponent<InventoryComponent>()) {
            result = e;
        }
    });
    return result;
}

/// @brief Carga el ItemAssetId desde el path logico. Retorna 0 si no
///        hay AssetManager o si el item no existe.
ItemAssetId resolveItem(AssetManager* assets, const std::string& path) {
    if (assets == nullptr) return 0;
    return assets->loadItem(path);
}

/// @brief Sumatoria de `entry.stat * entry.quantity` sobre todos los
///        entries del inventory. Si el item no tiene esa stat, suma 0
///        (no es error).
float sumStat(const Inventory::State& inv, AssetManager& assets,
              const std::string& statName) {
    float total = 0.0f;
    for (const auto& e : inv.entries) {
        const Inventory::Asset* asset = assets.getItem(e.itemId);
        if (asset == nullptr) continue;
        auto it = asset->stats.find(statName);
        if (it == asset->stats.end()) continue;
        total += it->second * static_cast<float>(e.quantity);
    }
    return total;
}

/// @brief Convierte los entries del inventario a una tabla Lua (1-indexed)
///        de tablas {item_path, qty, slot_index}.
sol::table entriesAsTable(sol::state_view lua, const Inventory::State& inv,
                          AssetManager& assets) {
    sol::table arr = lua.create_table();
    int luaIdx = 1;
    for (const auto& e : inv.entries) {
        sol::table row = lua.create_table();
        row["item_path"]  = assets.itemPathOf(e.itemId);
        row["qty"]        = e.quantity;
        row["slot_index"] = e.slot_index;
        arr[luaIdx++] = row;
    }
    return arr;
}

} // namespace

void setupInventoryBindings(sol::state& lua, Entity self,
                             AssetManager* assets) {
    // F2H52 F: registrar Entity usertype si no esta ya, para que los
    // callbacks Lua puedan leer `entity.tag` cuando reciben una Entity
    // como arg (caso `inventory.on_use(function(entity, path) ... end)`).
    // En produccion `setupLuaBindings` ya lo registro antes; en tests
    // que solo llaman `setupInventoryBindings`, esto evita un crash si
    // el callback toca `entity.tag`. Idempotente — sol2 sobrescribe.
    if (!lua["Entity"].valid()) {
        lua.new_usertype<Entity>("Entity",
            "tag", sol::property([](Entity& e) -> std::string {
                return e.hasComponent<TagComponent>()
                    ? e.getComponent<TagComponent>().name
                    : std::string{};
            }));
    }

    sol::table inv = lua.create_named_table("inventory");

    // -------- Lecturas --------

    // has(entity, path, [min_qty]) / has(path, [min_qty])
    inv.set_function("has", sol::overload(
        [assets](Entity ent, const std::string& path,
                  sol::optional<int> minQty) -> bool {
            if (!ent.hasComponent<InventoryComponent>()) return false;
            const int q = minQty.value_or(1);
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return ent.getComponent<InventoryComponent>().state.has(id, q);
        },
        [self, assets](const std::string& path,
                       sol::optional<int> minQty) -> bool {
            if (self.scene() == nullptr) return false;
            Entity player = findPlayer(*self.scene());
            if (!player) {
                Log::script()->warn(
                    "[inventory.has] no hay entity con tag 'Player' + InventoryComponent");
                return false;
            }
            const int q = minQty.value_or(1);
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return player.getComponent<InventoryComponent>().state.has(id, q);
        }
    ));

    // count(entity, path) / count(path)
    inv.set_function("count", sol::overload(
        [assets](Entity ent, const std::string& path) -> int {
            if (!ent.hasComponent<InventoryComponent>()) return 0;
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return 0;
            return ent.getComponent<InventoryComponent>().state.count(id);
        },
        [self, assets](const std::string& path) -> int {
            if (self.scene() == nullptr) return 0;
            Entity player = findPlayer(*self.scene());
            if (!player) return 0;
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return 0;
            return player.getComponent<InventoryComponent>().state.count(id);
        }
    ));

    // entries(entity) / entries()  — devuelve array de {item_path, qty, slot_index}
    inv.set_function("entries", sol::overload(
        [assets, &lua](Entity ent) -> sol::table {
            sol::state_view L(lua);
            if (!ent.hasComponent<InventoryComponent>() || assets == nullptr) {
                return L.create_table();
            }
            return entriesAsTable(L,
                ent.getComponent<InventoryComponent>().state, *assets);
        },
        [self, assets, &lua]() -> sol::table {
            sol::state_view L(lua);
            if (self.scene() == nullptr || assets == nullptr) {
                return L.create_table();
            }
            Entity player = findPlayer(*self.scene());
            if (!player) return L.create_table();
            return entriesAsTable(L,
                player.getComponent<InventoryComponent>().state, *assets);
        }
    ));

    // sum_stat(entity, stat_name) / sum_stat(stat_name)
    inv.set_function("sum_stat", sol::overload(
        [assets](Entity ent, const std::string& statName) -> float {
            if (!ent.hasComponent<InventoryComponent>() || assets == nullptr) {
                return 0.0f;
            }
            return sumStat(ent.getComponent<InventoryComponent>().state,
                            *assets, statName);
        },
        [self, assets](const std::string& statName) -> float {
            if (self.scene() == nullptr || assets == nullptr) return 0.0f;
            Entity player = findPlayer(*self.scene());
            if (!player) return 0.0f;
            return sumStat(player.getComponent<InventoryComponent>().state,
                            *assets, statName);
        }
    ));

    // -------- Mutaciones --------

    // add(entity, path, qty) / add(path, qty)
    inv.set_function("add", sol::overload(
        [assets](Entity ent, const std::string& path, int qty) -> bool {
            if (!ent.hasComponent<InventoryComponent>() || assets == nullptr) {
                return false;
            }
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return ent.getComponent<InventoryComponent>().state.add(id, qty, *assets);
        },
        [self, assets](const std::string& path, int qty) -> bool {
            if (self.scene() == nullptr || assets == nullptr) return false;
            Entity player = findPlayer(*self.scene());
            if (!player) {
                Log::script()->warn(
                    "[inventory.add] no hay entity con tag 'Player' + InventoryComponent");
                return false;
            }
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return player.getComponent<InventoryComponent>().state.add(id, qty, *assets);
        }
    ));

    // remove(entity, path, qty) / remove(path, qty)
    inv.set_function("remove", sol::overload(
        [assets](Entity ent, const std::string& path, int qty) -> bool {
            if (!ent.hasComponent<InventoryComponent>() || assets == nullptr) {
                return false;
            }
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return ent.getComponent<InventoryComponent>().state.remove(id, qty);
        },
        [self, assets](const std::string& path, int qty) -> bool {
            if (self.scene() == nullptr || assets == nullptr) return false;
            Entity player = findPlayer(*self.scene());
            if (!player) return false;
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            return player.getComponent<InventoryComponent>().state.remove(id, qty);
        }
    ));

    // -------- Spawn pickup en el mundo --------
    //
    // spawn_pickup(path, x, y, z, [qty]) — crea una nueva entity con
    // Transform en (x,y,z) + MeshRenderer (cubo + missing texture, o
    // icon_path si el item lo tiene configurado) + Trigger (1m³) +
    // ItemPickup (destroyOnPickup=true). Misma config que el drag-drop
    // del editor (F2H52 Bloque D) pero desde Lua para que el dev pueda
    // dropear items al matar un enemigo, abrir un cofre, etc.
    //
    // Devuelve true si la entity se creo, false si fail (sin scene / sin
    // assets / item invalido). NO devuelve la entity en si (sol2 no
    // expone bien refs no-owning hacia entities recien creadas; si
    // emerge necesidad, sumar getter `inventory.last_spawned()`).
    inv.set_function("spawn_pickup",
        [self, assets](const std::string& path,
                       float x, float y, float z,
                       sol::optional<int> qty) -> bool {
            if (self.scene() == nullptr) {
                Log::script()->warn("[inventory.spawn_pickup] no hay scene");
                return false;
            }
            if (assets == nullptr) {
                Log::script()->warn("[inventory.spawn_pickup] no hay AssetManager");
                return false;
            }
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) {
                Log::script()->warn("[inventory.spawn_pickup] item '{}' no existe", path);
                return false;
            }
            const Inventory::Asset* asset = assets->getItem(id);
            const int q = qty.value_or(1);

            // Mesh: model_path o cubo fallback.
            MeshAssetId meshId = 0;
            const bool hasModel = (asset != nullptr && !asset->model_path.empty());
            if (hasModel) {
                meshId = assets->loadMesh(asset->model_path);
            }
            // Material: si tiene model_path usa createMaterialsForMesh; si tiene
            // icon_path lo usa como albedo del cubo placeholder; sino default.
            std::vector<MaterialAssetId> mats;
            const bool hasIcon = (asset != nullptr && !asset->icon_path.empty());
            if (hasModel) {
                mats = assets->createMaterialsForMesh(meshId);
            } else if (hasIcon) {
                const TextureAssetId tex = assets->loadTexture(asset->icon_path);
                mats.push_back(assets->createMaterialFromTexture(tex));
            } else {
                mats = assets->createMaterialsForMesh(meshId);
            }

            // displayName para el Tag.
            std::string displayName;
            if (asset != nullptr && !asset->name_literal.empty()) {
                displayName = asset->name_literal;
            } else if (asset != nullptr && !asset->id.empty()) {
                displayName = asset->id;
            } else {
                displayName = "item";
            }

            Entity e = self.scene()->createEntity("Pickup_" + displayName);
            auto& t = e.getComponent<TransformComponent>();
            t.position = glm::vec3(x, y, z);
            t.scale    = glm::vec3(1.0f);

            e.addComponent<MeshRendererComponent>(meshId, std::move(mats));

            TriggerComponent trig;
            trig.halfExtents = glm::vec3(0.5f);
            e.addComponent<TriggerComponent>(trig);

            ItemPickupComponent ip;
            ip.itemPath = path;
            ip.quantity = q;
            e.addComponent<ItemPickupComponent>(ip);

            return true;
        });

    // -------- Hooks: on_pickup / on_drop / on_use (F2H52 Bloque F) --------
    //
    // El dev registra UNA vez al inicio del juego (NO cada frame). Los
    // hooks se mantienen vivos via sol::function capturada — el Lua state
    // que registro el hook debe seguir vivo mientras se invoque (si el
    // dev recarga el script en hot-reload, el sol::function vieja queda
    // colgando; los hooks se sobreescriben al re-registrarse). Si nadie
    // registra, el motor sigue funcionando sin disparar nada.
    //
    // Mismo patron que F2H48.1 DialogSystem::setEvaluator/Executor.

    inv.set_function("on_pickup",
        [](sol::function fn) {
            if (!fn.valid()) {
                Inventory::Hooks::setPickupHook(nullptr);
                return;
            }
            Inventory::Hooks::setPickupHook(
                [fn](const std::string& itemPath, int quantity) {
                    sol::protected_function_result r = fn(itemPath, quantity);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[inventory.on_pickup] callback error: {}",
                            err.what());
                    }
                });
        });

    inv.set_function("on_drop",
        [](sol::function fn) {
            if (!fn.valid()) {
                Inventory::Hooks::setDropHook(nullptr);
                return;
            }
            Inventory::Hooks::setDropHook(
                [fn](const std::string& itemPath, int quantity) {
                    sol::protected_function_result r = fn(itemPath, quantity);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[inventory.on_drop] callback error: {}",
                            err.what());
                    }
                });
        });

    inv.set_function("on_use",
        [](sol::function fn) {
            if (!fn.valid()) {
                Inventory::Hooks::setUseHook(nullptr);
                return;
            }
            Inventory::Hooks::setUseHook(
                [fn](Entity entity, const std::string& itemPath) {
                    sol::protected_function_result r = fn(entity, itemPath);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[inventory.on_use] callback error: {}",
                            err.what());
                    }
                });
        });

    // -------- inventory.use(entity, path) / inventory.use(path) --------
    //
    // Verifica que el entity tenga InventoryComponent + count >= 1 del
    // item. Si si, invoca el hook on_use(entity, item_path) — el callback
    // decide si consume el item (caso pocion: hace `inventory.remove`
    // adentro) o no (caso arma equipable: lo mantiene). Retorna true si
    // el hook fue invocado, false si:
    //   - El entity no tiene InventoryComponent.
    //   - No tiene >=1 unidad del item.
    //   - No hay hook on_use registrado.
    inv.set_function("use", sol::overload(
        [assets](Entity ent, const std::string& path) -> bool {
            if (!ent.hasComponent<InventoryComponent>()) return false;
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            if (!ent.getComponent<InventoryComponent>().state.has(id, 1)) {
                return false;
            }
            if (!Inventory::Hooks::hasUseHook()) {
                Log::script()->warn(
                    "[inventory.use] no hay callback on_use registrado");
                return false;
            }
            Inventory::Hooks::invokeUse(ent, path);
            return true;
        },
        [self, assets](const std::string& path) -> bool {
            if (self.scene() == nullptr || assets == nullptr) return false;
            Entity player = findPlayer(*self.scene());
            if (!player) return false;
            const ItemAssetId id = resolveItem(assets, path);
            if (id == 0) return false;
            if (!player.getComponent<InventoryComponent>().state.has(id, 1)) {
                return false;
            }
            if (!Inventory::Hooks::hasUseHook()) {
                Log::script()->warn(
                    "[inventory.use] no hay callback on_use registrado");
                return false;
            }
            Inventory::Hooks::invokeUse(player, path);
            return true;
        }
    ));
}

} // namespace Mood
