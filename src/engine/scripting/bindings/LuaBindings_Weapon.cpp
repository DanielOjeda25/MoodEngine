// F4H2 — Tabla `weapon` para los scripts Lua. Engine-generic: el
// motor solo conoce el sistema (raycast + damage + feedback). El juego
// orquesta input -> weapon.fire(...) en sus scripts.
//
// API expuesta:
//   weapon.equip(tag, "weapons/shotgun.moodweapon") -> bool
//   weapon.fire(tag, {origin_x=,y=,z=, dir_x=,y=,z=}) -> bool
//                  (origin/dir opcionales; default = self transform pos + -Z)
//   weapon.reload(tag) -> bool
//   weapon.can_fire(tag) -> bool
//   weapon.ammo(tag) -> int
//   weapon.spec(tag) -> table (read-only) | nil
//
// `tag` busca la primera entity con ese tag (convencion editor); si no
// existe o no tiene WeaponComponent, la API loguea warn y devuelve
// nil / false / 0.

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/audio/device/AudioDevice.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/gameplay/weapon/WeaponSystem.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/BindingsCommon.h"

#include <sol/sol.hpp>

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

namespace {

using bindings::findEntityByTag;

} // anonymous

void setupWeaponBindings(sol::state& lua, Scene* scene,
                          PhysicsWorld* physicsWorld,
                          AudioDevice* audioDevice,
                          AssetManager* assetManager) {
    sol::table t = lua.create_named_table("weapon");

    t.set_function("equip",
        [scene, assetManager](const std::string& tag, const std::string& path)
        -> bool {
            if (scene == nullptr || assetManager == nullptr) {
                Log::script()->warn("weapon.equip: scene/assets nullptr");
                return false;
            }
            Entity e = findEntityByTag(*scene, tag);
            if (!e) {
                Log::script()->warn("weapon.equip: tag '{}' no encontrado", tag);
                return false;
            }
            return Weapon::equipWeapon(*scene, e, path, *assetManager);
        });

    t.set_function("fire",
        [scene, physicsWorld, audioDevice, assetManager]
        (const std::string& tag, sol::optional<sol::table> opts) -> bool {
            if (scene == nullptr || physicsWorld == nullptr
                || audioDevice == nullptr || assetManager == nullptr) {
                Log::script()->warn(
                    "weapon.fire: requiere scene + physics + audio + assets");
                return false;
            }
            Entity e = findEntityByTag(*scene, tag);
            if (!e) {
                Log::script()->warn("weapon.fire: tag '{}' no encontrado", tag);
                return false;
            }
            Weapon::FireParams fp{};
            // Default: origin = position de la entity, dir = -Z local.
            if (e.hasComponent<TransformComponent>()) {
                const auto& tf = e.getComponent<TransformComponent>();
                fp.origin = tf.position;
            }
            fp.direction = glm::vec3(0.0f, 0.0f, -1.0f);

            if (opts.has_value()) {
                sol::table o = *opts;
                fp.origin.x = o.get_or("origin_x", fp.origin.x);
                fp.origin.y = o.get_or("origin_y", fp.origin.y);
                fp.origin.z = o.get_or("origin_z", fp.origin.z);
                fp.direction.x = o.get_or("dir_x", fp.direction.x);
                fp.direction.y = o.get_or("dir_y", fp.direction.y);
                fp.direction.z = o.get_or("dir_z", fp.direction.z);
                fp.ignoredBodyId = o.get_or("ignored_body", 0u);
            }
            Weapon::FireResult r = Weapon::fire(*scene, e, fp, *physicsWorld,
                                                  *audioDevice, *assetManager);
            return r.fired;
        });

    t.set_function("reload",
        [scene, assetManager](const std::string& tag) -> bool {
            if (scene == nullptr || assetManager == nullptr) return false;
            Entity e = findEntityByTag(*scene, tag);
            if (!e) {
                Log::script()->warn("weapon.reload: tag '{}' no encontrado", tag);
                return false;
            }
            return Weapon::reload(*scene, e, *assetManager);
        });

    t.set_function("can_fire",
        [scene, assetManager](const std::string& tag) -> bool {
            if (scene == nullptr || assetManager == nullptr) return false;
            Entity e = findEntityByTag(*scene, tag);
            if (!e) return false;
            return Weapon::canFire(*scene, e, *assetManager);
        });

    t.set_function("ammo",
        [scene, assetManager](const std::string& tag) -> int {
            if (scene == nullptr || assetManager == nullptr) return 0;
            Entity e = findEntityByTag(*scene, tag);
            if (!e) return 0;
            return Weapon::ammoLeft(*scene, e, *assetManager);
        });

    t.set_function("spec",
        [scene, assetManager, &lua](const std::string& tag) -> sol::object {
            if (scene == nullptr || assetManager == nullptr) return sol::nil;
            Entity e = findEntityByTag(*scene, tag);
            if (!e || !e.hasComponent<WeaponComponent>()) return sol::nil;
            const auto& wc = e.getComponent<WeaponComponent>();
            // F4H3: spec del slot activo.
            const u32 idx = (wc.activeSlot < WeaponComponent::k_maxSlots)
                            ? wc.activeSlot : 0u;
            const u32 wid = wc.slots[idx].weaponAssetId;
            if (wid == 0) return sol::nil;
            const Weapon::Spec* spec = assetManager->getWeapon(wid);
            if (spec == nullptr) return sol::nil;
            sol::table out = lua.create_table();
            out["displayName"]    = spec->displayName;
            out["category"]       = spec->category;
            out["damage"]         = spec->damage;
            out["range"]          = spec->range;
            out["pellets"]        = spec->pellets;
            out["spreadDeg"]      = spec->spreadDeg;
            out["fireRatePerSec"] = spec->fireRatePerSec;
            out["magazineSize"]   = spec->magazineSize;
            out["reloadTimeSec"]  = spec->reloadTimeSec;
            out["activeSlot"]     = wc.activeSlot;
            return out;
        });
}

} // namespace Mood
