#include "engine/gameplay/enemy/EnemySystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/enemy/EnemySpec.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

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
                  const std::string& tagName) {
    if (ec.state == next) return;
    Log::engine()->info("[enemy] '{}' state {} -> {}",
                          tagName, stateName(ec.state), stateName(next));
    ec.state = next;
    ec.stateTime = 0.0f;
}

} // namespace

void tickSystem(Scene& scene, f32 dt, Entity playerEntity,
                AudioDevice* /*audio*/, const AssetManager& assets) {
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

        // --- State machine ---
        switch (ec.state) {
            case EnemyState::Idle: {
                if (playerValid && dist <= spec.aggroRange) {
                    ec.targetEntity = playerHandle;
                    transitionTo(ec, EnemyState::Alert, tagName);
                }
                break;
            }
            case EnemyState::Alert: {
                // F4H7: queda en Alert estable. F4H8 traera transicion a
                // Chase (movimiento + pathfinding). Pre-F4H8 no chasea pero
                // marca que el enemigo "te vio".
                // Si el player salio del aggro range × 1.5 (hysteresis),
                // volver a Idle.
                if (!playerValid || dist > spec.aggroRange * 1.5f) {
                    ec.targetEntity = EnemyComponent::k_noTarget;
                    transitionTo(ec, EnemyState::Idle, tagName);
                } else if (dist <= spec.attackRange) {
                    transitionTo(ec, EnemyState::Attack, tagName);
                }
                break;
            }
            case EnemyState::Chase: {
                // F4H8 implementa movimiento aca. F4H7: no-op cinematico.
                // Transition a Attack si el player ya esta cerca, a Alert
                // si se escapo.
                if (!playerValid || dist > spec.aggroRange * 1.5f) {
                    ec.targetEntity = EnemyComponent::k_noTarget;
                    transitionTo(ec, EnemyState::Idle, tagName);
                } else if (dist <= spec.attackRange) {
                    transitionTo(ec, EnemyState::Attack, tagName);
                } else if (dist > spec.attackRange * 1.5f) {
                    // Hysteresis para no oscilar entre Attack/Chase.
                    // No-op en F4H7.
                }
                break;
            }
            case EnemyState::Attack: {
                // F4H9 implementa daño al player aca. F4H7: cooldown timer
                // sin damage real. Vuelve a Alert/Chase si player se aleja.
                if (!playerValid || dist > spec.attackRange * 1.5f) {
                    if (dist <= spec.aggroRange) {
                        transitionTo(ec, EnemyState::Alert, tagName);
                    } else {
                        ec.targetEntity = EnemyComponent::k_noTarget;
                        transitionTo(ec, EnemyState::Idle, tagName);
                    }
                }
                break;
            }
            case EnemyState::Pain: {
                // Stagger: queda en Pain durante painDuration. Despues vuelve
                // a Alert si tiene target, sino Idle.
                if (ec.stateTime >= spec.painDuration) {
                    if (ec.targetEntity != EnemyComponent::k_noTarget && playerValid) {
                        transitionTo(ec, EnemyState::Alert, tagName);
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

        ec.stateTime += dt;
    });
}

} // namespace Enemy
} // namespace Mood
