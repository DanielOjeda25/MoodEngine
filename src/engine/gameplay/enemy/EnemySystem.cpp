#include "engine/gameplay/enemy/EnemySystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"                 // F4H9: applyDamage al player en melee
#include "engine/gameplay/enemy/EnemySpec.h"
#include "engine/gameplay/weapon/WeaponSpec.h"      // F4H9: spec del weapon disparado
#include "engine/gameplay/weapon/WeaponSystem.h"    // F4H9: Weapon::fire para projectile
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace Mood {
namespace Enemy {

namespace {

const char* stateName(EnemyState s) {
    switch (s) {
        case EnemyState::Idle:   return "Idle";
        case EnemyState::Alert:  return "Alert";
        case EnemyState::Chase:  return "Chase";
        case EnemyState::Attack: return "Attack";
        case EnemyState::Pain:   return "Pain";
        case EnemyState::Dead:   return "Dead";
    }
    return "?";
}

inline f32 horizontalDistance(const glm::vec3& a, const glm::vec3& b) {
    const f32 dx = a.x - b.x;
    const f32 dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

void transitionTo(EnemyComponent& ec, EnemyState next,
                  const std::string& tagName, const Spec* spec = nullptr) {
    if (ec.state == next) return;
    Log::engine()->info("[enemy] '{}' state {} -> {}",
                          tagName, stateName(ec.state), stateName(next));
    ec.state = next;
    ec.stateTime = 0.0f;
    // F4H9 D7: al entrar Attack re-arm timers (wind-up del telegraph
    // + cooldown=0 listo para golpear post wind-up).
    if (next == EnemyState::Attack) {
        ec.windUpTimer = spec != nullptr ? spec->windUpSec : 0.3f;
        ec.attackCooldownTimer = 0.0f;
    }
}

// F4H8: ensure NavAgent + setup para chase/attack. Auto-add component
// si no existe (D7). En estados quietos (Idle/Alert/Pain/Dead) se llama
// a deactivateNavAgent en su lugar.
void activateNavAgent(entt::registry& reg, entt::entity e,
                       const glm::vec3& target, f32 speed) {
    if (!reg.all_of<NavAgentComponent>(e)) {
        reg.emplace<NavAgentComponent>(e);
    }
    auto& nav = reg.get<NavAgentComponent>(e);
    nav.target = target;
    nav.speed  = speed;
    nav.active = true;
}

// F4H8: stop al NavAgent sin borrarlo (D7 — re-allocar cada
// Idle↔Chase es churn innecesario; solo desactivamos).
void deactivateNavAgent(entt::registry& reg, entt::entity e) {
    if (reg.all_of<NavAgentComponent>(e)) {
        reg.get<NavAgentComponent>(e).active = false;
    }
}

// F4H9: aplica el golpe del enemy al player. Dispatch segun
// `spec.attackKind`. Engine-generic.
void applyEnemyAttack(Scene& scene, Entity enemy, Entity player,
                       const Spec& spec, const std::string& tagName,
                       PhysicsWorld* physics, AudioDevice* audio,
                       AssetManager& assets) {
    if (!player) return;

    if (spec.attackKind == "projectile") {
        // F4H9 D3: el enemy "dispara" un .moodweapon hacia el player.
        // Reusa todo el pipeline F4H5 — Weapon::fire calcula direccion
        // + raycast + spawn projectile + cleanup. El enemy se vuelve
        // el shooter; ignoreOwner del weapon spec evita splash damage
        // sobre si mismo.
        //
        // Si no hay physics o audio (tests headless), skip silent. La
        // state machine sigue funcional pero el ataque no se materializa.
        if (physics == nullptr || audio == nullptr) {
            Log::engine()->warn(
                "[enemy] '{}' projectile attack skipped (sin physics/audio)",
                tagName);
            return;
        }
        if (spec.projectileWeapon.empty()) {
            Log::engine()->warn(
                "[enemy] '{}' projectile attack sin projectileWeapon configurado",
                tagName);
            return;
        }

        // Auto-add WeaponComponent on-demand (D7 patron) con el
        // projectileWeapon equipped en slot 0.
        auto& reg = scene.registry();
        const entt::entity eh = enemy.handle();
        if (!reg.all_of<WeaponComponent>(eh)) {
            reg.emplace<WeaponComponent>(eh);
        }
        auto& wc = reg.get<WeaponComponent>(eh);
        // Si el slot 0 está vacio o tiene otro weapon, cargar el
        // projectileWeapon. Si ya esta cargado, no re-loadear.
        const WeaponAssetId desiredId = assets.loadWeapon(spec.projectileWeapon);
        if (wc.slots[0].weaponAssetId != desiredId) {
            wc.slots[0].weaponAssetId = desiredId;
            if (const Weapon::Spec* ws = assets.getWeapon(desiredId)) {
                wc.slots[0].currentAmmo = static_cast<int>(ws->magazineSize);
            }
        }
        wc.activeSlot = 0;
        // Reset fire cooldown del weapon — el enemy controla su propio
        // cooldown via attackCooldownTimer, no via fireRate del weapon.
        wc.fireTimer = 0.0f;

        // Compute origin + direction towards player.
        const auto& enemyXform  = enemy.getComponent<TransformComponent>();
        const auto& playerXform = player.getComponent<TransformComponent>();
        const glm::vec3 origin = enemyXform.position + glm::vec3(0.0f, 0.5f, 0.0f);
        const glm::vec3 toPlayer = playerXform.position - origin;
        const f32 len = glm::length(toPlayer);
        const glm::vec3 dir = (len > 1e-4f) ? (toPlayer / len)
                                              : glm::vec3(0.0f, 0.0f, -1.0f);

        Weapon::FireParams params;
        params.origin    = origin;
        params.direction = dir;
        params.ignoredBodyId = 0;  // sin physics body del enemy en F4H9

        const auto result = Weapon::fire(scene, enemy, params,
                                            *physics, *audio, assets);
        Log::engine()->info(
            "[enemy] '{}' projectile attack fired={} ({} pellets, {} hits)",
            tagName, result.fired, result.pelletsFired, result.pelletsHit);
        return;
    }

    // Default: melee body slam. Damage instantaneo via Health::applyDamage.
    if (spec.damage <= 0.0f) return;
    const auto& enemyXform  = enemy.getComponent<TransformComponent>();
    const auto& playerXform = player.getComponent<TransformComponent>();
    const glm::vec3 dir = glm::normalize(
        playerXform.position - enemyXform.position
        + glm::vec3(1e-4f, 0.0f, 0.0f));  // epsilon evita 0-vector
    Health::applyDamage(scene, player, spec.damage, dir);
    Log::engine()->info(
        "[enemy] '{}' melee hit player ({:.0f} dmg)", tagName, spec.damage);
}

} // namespace

void tickSystem(Scene& scene, f32 dt, Entity playerEntity,
                AudioDevice* audio, AssetManager& assets,
                PhysicsWorld* physics) {
    auto& reg = scene.registry();

    // Resolver pos del player. Si no es valida → playerPos quedará en NaN
    // sentinela; los enemies tratan eso como "sin target" y quedan en Idle.
    bool playerValid = false;
    glm::vec3 playerPos{0.0f};
    u32 playerHandle = 0;
    if (playerEntity && playerEntity.hasComponent<TransformComponent>()) {
        playerPos = playerEntity.getComponent<TransformComponent>().position;
        playerValid = true;
        playerHandle = static_cast<u32>(playerEntity.handle());
    }

    reg.view<EnemyComponent>().each([&](entt::entity e, EnemyComponent& ec) {
        // Si el enemy no tiene Transform, no podemos calcular distancia →
        // skip silencioso. Convencion del engine (no crash).
        if (!reg.all_of<TransformComponent>(e)) return;
        const auto& xform = reg.get<TransformComponent>(e);

        const std::string tagName = reg.all_of<TagComponent>(e)
            ? reg.get<TagComponent>(e).name
            : std::string("<sin-tag>");

        // Spec del enemy. Si enemyAssetId no corresponde a un spec cargado,
        // getEnemy(id) cae al slot 0 con defaults sanos (no crash).
        const Spec* specPtr = assets.getEnemy(ec.enemyAssetId);
        const Spec  defaults{};
        const Spec& spec = specPtr != nullptr ? *specPtr : defaults;

        // Dead = terminal. Solo asegurar Dynamic RB en la primera tick
        // post-mortem (auto-fall con fisica, mismo patron F4H1 Health).
        if (ec.state == EnemyState::Dead) {
            if (!reg.all_of<RigidBodyComponent>(e)) {
                reg.emplace<RigidBodyComponent>(e,
                    RigidBodyComponent::Type::Dynamic,
                    RigidBodyComponent::Shape::Box,
                    glm::vec3(0.5f),
                    1.0f /* mass */);
            }
            ec.stateTime += dt;
            return;
        }

        // Health del enemy. Si no tiene HealthComponent, no transiciona a
        // Pain/Dead — queda inmortal hasta que algun script lo mate
        // manualmente (raro pero soportado).
        HealthComponent* hc = reg.all_of<HealthComponent>(e)
            ? &reg.get<HealthComponent>(e)
            : nullptr;

        // --- Pain trigger (polling de hitFlashTimer) ---
        // El frame anterior comparamos el flash; si subio en este tick →
        // recibio damage → Pain state. Mismo patron R4 F4H4 que evita
        // callbacks engine→game.
        if (hc != nullptr) {
            const f32 nowFlash = hc->hitFlashTimer;
            if (nowFlash > ec.prevHitFlashTimer + 1e-4f &&
                ec.state != EnemyState::Pain && ec.state != EnemyState::Dead) {
                // Filtra hits chicos. El damage del hit se infiere por la
                // diferencia del flash NO esta disponible — el threshold
                // se evalua via spec (asume el caller respeta el contrato).
                // En F4H7 cualquier hit dispara Pain (filtro per-dmg requiere
                // que Health pase damage al callback, fuera de scope).
                transitionTo(ec, EnemyState::Pain, tagName);
                ec.painsTotal += 1;
            }
            ec.prevHitFlashTimer = nowFlash;
        }

        // --- Dead transition ---
        if (hc != nullptr && hc->dead && ec.state != EnemyState::Dead) {
            transitionTo(ec, EnemyState::Dead, tagName);
            // F4H8: desactivar NavAgent ANTES de agregar Dynamic RB —
            // evita la pelea de autoridad sobre el Transform que el
            // comment de NavAgentComponent flagea explicito (D8).
            deactivateNavAgent(reg, e);
            // Auto-add Dynamic RB en el MISMO tick — el test inmediato
            // verifica el component (mismo patron F4H1 Health::tickSystem).
            if (!reg.all_of<RigidBodyComponent>(e)) {
                reg.emplace<RigidBodyComponent>(e,
                    RigidBodyComponent::Type::Dynamic,
                    RigidBodyComponent::Shape::Box,
                    glm::vec3(0.5f),
                    1.0f /* mass */);
            }
            ec.stateTime += dt;
            return;
        }

        // Distancia al player (si hay).
        const f32 dist = playerValid
            ? horizontalDistance(xform.position, playerPos)
            : 1e9f;

        // --- State machine — evalua transitions segun el state actual ---
        switch (ec.state) {
            case EnemyState::Idle: {
                if (playerValid && dist <= spec.aggroRange) {
                    ec.targetEntity = playerHandle;
                    transitionTo(ec, EnemyState::Alert, tagName);
                }
                break;
            }
            case EnemyState::Alert: {
                // F4H8 (D5): Alert ya no es estable — apenas te ve, persigue.
                // Si dist > attackRange → Chase. Si dist <= attackRange → Attack.
                if (!playerValid || dist > spec.aggroRange * 1.5f) {
                    ec.targetEntity = EnemyComponent::k_noTarget;
                    transitionTo(ec, EnemyState::Idle, tagName);
                } else if (dist <= spec.attackRange) {
                    transitionTo(ec, EnemyState::Attack, tagName, &spec);
                } else {
                    transitionTo(ec, EnemyState::Chase, tagName);
                }
                break;
            }
            case EnemyState::Chase: {
                if (!playerValid || dist > spec.aggroRange * 1.5f) {
                    ec.targetEntity = EnemyComponent::k_noTarget;
                    transitionTo(ec, EnemyState::Idle, tagName);
                } else if (dist <= spec.attackRange) {
                    transitionTo(ec, EnemyState::Attack, tagName, &spec);
                }
                break;
            }
            case EnemyState::Attack: {
                // F4H9: wind-up + cooldown + apply damage.
                // Si player se aleja → Chase/Idle como en F4H8.
                if (!playerValid || dist > spec.attackRange * 1.5f) {
                    if (dist <= spec.aggroRange) {
                        transitionTo(ec, EnemyState::Chase, tagName);
                    } else {
                        ec.targetEntity = EnemyComponent::k_noTarget;
                        transitionTo(ec, EnemyState::Idle, tagName);
                    }
                    break;
                }
                // En Attack stable: decrement wind-up; cuando expira y
                // cooldown <= 0, aplicar el golpe + re-arm (D7). Tolerance
                // de 1e-5 evita float epsilon edge cases — 6 decrements
                // de 0.05 sobre 0.3 no llegan a 0 exacto (residuo 5e-8).
                constexpr f32 k_timerEps = 1e-5f;
                if (ec.windUpTimer > k_timerEps) {
                    ec.windUpTimer = std::max(0.0f, ec.windUpTimer - dt);
                } else if (ec.attackCooldownTimer <= k_timerEps) {
                    applyEnemyAttack(scene, Entity{e, &scene}, playerEntity,
                                      spec, tagName, physics, audio, assets);
                    ec.attackCooldownTimer = spec.attackCooldown;
                    ec.windUpTimer = spec.windUpSec;  // re-arm wind-up del proximo
                } else {
                    ec.attackCooldownTimer = std::max(0.0f,
                        ec.attackCooldownTimer - dt);
                }
                break;
            }
            case EnemyState::Pain: {
                // Stagger: queda en Pain durante painDuration. Despues vuelve
                // a Chase (con target — el A* retoma) o Idle (sin target).
                if (ec.stateTime >= spec.painDuration) {
                    if (ec.targetEntity != EnemyComponent::k_noTarget && playerValid) {
                        transitionTo(ec, EnemyState::Chase, tagName);
                    } else {
                        transitionTo(ec, EnemyState::Idle, tagName);
                    }
                }
                break;
            }
            case EnemyState::Dead:
                // Manejado arriba (early return).
                break;
        }

        // --- F4H8: NavAgent side-effects basado en el state POST-transition.
        // Separado del switch para que las transitions Alert→Chase activen
        // el NavAgent en el MISMO tick (D5 — apenas Alert decide Chase, el
        // A* arranca).
        switch (ec.state) {
            case EnemyState::Chase:
            case EnemyState::Attack:
                // D1+D6: tracking continuo en Chase Y Attack. Target/speed
                // refresh cada frame para sync con spec live edits.
                activateNavAgent(reg, e, playerPos, spec.moveSpeed);
                break;
            case EnemyState::Idle:
            case EnemyState::Alert:
            case EnemyState::Pain:
                deactivateNavAgent(reg, e);
                break;
            case EnemyState::Dead:
                // Ya desactivado al transition arriba.
                break;
        }

        ec.stateTime += dt;
    });
}

} // namespace Enemy
} // namespace Mood
