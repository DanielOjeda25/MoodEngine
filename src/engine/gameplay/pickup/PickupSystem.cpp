#include "engine/gameplay/pickup/PickupSystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/weapon/WeaponSpec.h"  // tipo completo Spec
#include "engine/gameplay/weapon/WeaponSystem.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Mood {
namespace Pickup {

namespace {

// Encuentra el primer slot vacio del WeaponComponent. Devuelve k_maxSlots
// si todos los slots estan ocupados.
u32 firstEmptySlot(const WeaponComponent& wc) {
    for (u32 i = 0; i < WeaponComponent::k_maxSlots; ++i) {
        if (wc.slots[i].weaponAssetId == 0) return i;
    }
    return WeaponComponent::k_maxSlots;
}

// Si el player tiene esta arma ya equipada en algun slot, devuelve el slot.
// k_maxSlots si no la tiene.
u32 slotWithWeaponPath(const WeaponComponent& wc, const AssetManager& assets,
                       const std::string& path) {
    for (u32 i = 0; i < WeaponComponent::k_maxSlots; ++i) {
        if (wc.slots[i].weaponAssetId == 0) continue;
        const std::string equipped = assets.weaponPathOf(wc.slots[i].weaponAssetId);
        if (equipped == path) return i;
    }
    return WeaponComponent::k_maxSlots;
}

bool applyHealthPickup(Entity player, const PickupComponent& p) {
    if (!player.hasComponent<HealthComponent>()) return false;
    auto& h = player.getComponent<HealthComponent>();
    if (h.dead) return false;
    if (h.current >= h.max) return false; // ya full → no consume (Doom-style)
    const f32 before = h.current;
    h.current = std::min(h.max, h.current + p.healthAmount);
    Log::engine()->info("[pickup] health +{:.0f} ({:.0f} -> {:.0f}/{:.0f})",
                        p.healthAmount, before, h.current, h.max);
    return true;
}

bool applyArmorPickup(Entity player, const PickupComponent& p) {
    if (!player.hasComponent<ArmorComponent>()) return false;
    auto& a = player.getComponent<ArmorComponent>();
    if (a.current >= a.max) return false;
    const f32 before = a.current;
    a.current = std::min(a.max, a.current + p.armorAmount);
    Log::engine()->info("[pickup] armor +{:.0f} ({:.0f} -> {:.0f}/{:.0f})",
                        p.armorAmount, before, a.current, a.max);
    return true;
}

bool applyWeaponPickup(Scene& scene, Entity player, const PickupComponent& p,
                       AssetManager& assets) {
    if (p.weaponPath.empty()) {
        Log::engine()->warn("[pickup] weapon pickup sin weaponPath — no-op");
        return false;
    }
    if (!player.hasComponent<WeaponComponent>()) {
        Log::engine()->warn("[pickup] player sin WeaponComponent — no-op");
        return false;
    }
    auto& wc = player.getComponent<WeaponComponent>();

    // D4: si el player ya tiene esta arma, refill ammo a magazineSize.
    const u32 existingSlot = slotWithWeaponPath(wc, assets, p.weaponPath);
    if (existingSlot < WeaponComponent::k_maxSlots) {
        const auto id = wc.slots[existingSlot].weaponAssetId;
        if (const auto* spec = assets.getWeapon(id)) {
            wc.slots[existingSlot].currentAmmo = static_cast<int>(spec->magazineSize);
            Log::engine()->info("[pickup] '{}' refill ammo a {} (slot {})",
                                p.weaponPath, spec->magazineSize, existingSlot);
        }
        return true;
    }

    // Buscar primer slot vacio.
    const u32 emptySlot = firstEmptySlot(wc);
    if (emptySlot >= WeaponComponent::k_maxSlots) {
        Log::engine()->warn(
            "[pickup] arsenal lleno, '{}' no se equipa (F4H7 replace/discard)",
            p.weaponPath);
        return false;
    }
    return Weapon::equipWeaponInSlot(scene, player, emptySlot, p.weaponPath, assets);
}

void applyAmmoPickup(Entity player, const PickupComponent& p,
                      AssetManager& assets) {
    if (!player.hasComponent<WeaponComponent>()) return;
    auto& wc = player.getComponent<WeaponComponent>();

    // Si ammoForWeapon vacio → arma activa.
    u32 targetSlot = WeaponComponent::k_maxSlots;
    if (p.ammoForWeapon.empty()) {
        targetSlot = (wc.activeSlot < WeaponComponent::k_maxSlots)
                       ? wc.activeSlot : 0u;
    } else {
        targetSlot = slotWithWeaponPath(wc, assets, p.ammoForWeapon);
    }

    if (targetSlot >= WeaponComponent::k_maxSlots
        || wc.slots[targetSlot].weaponAssetId == 0) {
        // No tiene esa arma equipada — pickup se "pierde" (no se consume).
        return;
    }

    const auto* spec = assets.getWeapon(wc.slots[targetSlot].weaponAssetId);
    if (spec == nullptr) return;
    const int maxAmmo = static_cast<int>(spec->magazineSize);
    int& current = wc.slots[targetSlot].currentAmmo;
    if (current < 0) current = 0;  // sanity: -1 (unset) cuenta como 0
    const int before = current;
    current = std::min(maxAmmo, current + p.ammoAmount);
    Log::engine()->info("[pickup] ammo +{} a slot {} ({} -> {}/{})",
                        p.ammoAmount, targetSlot, before, current, maxAmmo);
}

} // namespace

void tickSystem(Scene& scene, f32 dt, Entity playerEntity, AssetManager* assets) {
    auto& reg = scene.registry();

    // Resolver posicion del player una sola vez por frame.
    const bool playerValid = playerEntity
                              && playerEntity.hasComponent<TransformComponent>();
    const glm::vec3 playerPos = playerValid
        ? playerEntity.getComponent<TransformComponent>().position
        : glm::vec3(0.0f);

    std::vector<entt::entity> toDestroy;

    reg.view<PickupComponent, TransformComponent>().each(
        [&](entt::entity ent, PickupComponent& p, TransformComponent& tf) {
            if (p.consumed) {
                toDestroy.push_back(ent);
                return;
            }

            p.ageSec += dt;

            // Spin sobre Y (grados acumulados).
            tf.rotationEuler.y += p.spinDegPerSec * dt;
            if (tf.rotationEuler.y > 360.0f)  tf.rotationEuler.y -= 360.0f;
            if (tf.rotationEuler.y < -360.0f) tf.rotationEuler.y += 360.0f;

            // Bob: deltaY desde la posicion original "base" guardada en
            // ageSec=0. Sin baseline persistido por simpleza: bobeamos
            // alrededor de la posicion actual usando la fase derivada de age.
            // El primer frame el bob es ~0 (sin doc del baseline) y el
            // movimiento queda relativo a la posicion actual. Aceptable:
            // pickups son chiquitos.
            const f32 bobPhase  = p.ageSec * p.bobSpeed;
            const f32 bobOffset = std::sin(bobPhase) * p.bobAmplitude;
            const f32 prevPhase = (p.ageSec - dt) * p.bobSpeed;
            const f32 prevOffset = std::sin(prevPhase) * p.bobAmplitude;
            tf.position.y += (bobOffset - prevOffset);

            if (!playerValid) return;

            const f32 dx = tf.position.x - playerPos.x;
            const f32 dy = tf.position.y - playerPos.y;
            const f32 dz = tf.position.z - playerPos.z;
            const f32 distSq = dx*dx + dy*dy + dz*dz;
            const f32 r = p.pickupRadius;
            if (distSq > r * r) return;

            // Overlap! Aplicar payload segun tipo.
            bool consumed = true;
            switch (p.type) {
                case PickupType::Health:
                    consumed = applyHealthPickup(playerEntity, p);
                    break;
                case PickupType::Armor:
                    consumed = applyArmorPickup(playerEntity, p);
                    break;
                case PickupType::Weapon:
                    if (assets == nullptr) {
                        Log::engine()->warn(
                            "[pickup] weapon pickup sin AssetManager — skip");
                        consumed = false;
                    } else {
                        consumed = applyWeaponPickup(scene, playerEntity, p, *assets);
                    }
                    break;
                case PickupType::Ammo:
                    if (assets == nullptr) {
                        consumed = false;
                    } else {
                        applyAmmoPickup(playerEntity, p, *assets);
                    }
                    break;
            }
            p.consumed = consumed;
            if (consumed) toDestroy.push_back(ent);
        });

    // Cleanup post-iteracion.
    for (entt::entity ent : toDestroy) {
        if (reg.valid(ent)) reg.destroy(ent);
    }
}

} // namespace Pickup
} // namespace Mood
