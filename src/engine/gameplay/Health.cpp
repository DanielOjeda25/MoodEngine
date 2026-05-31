#include "engine/gameplay/Health.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <algorithm>

namespace Mood {
namespace Health {

namespace {

// F4H1: duración del flash blanco on-hit (D1). Convención HL/Quake/Doom.
constexpr f32 k_hitFlashSec   = 0.08f;
// F4H1: flash más largo al morir — marca el momento del kill.
constexpr f32 k_deathFlashSec = 0.25f;

} // namespace

void applyDamage(Scene& scene, Entity target, f32 amount,
                  const glm::vec3& /*dir*/) {
    (void)scene;  // target ya carga su scene; futuro dir se usa en F4H2
    if (!target || !target.hasComponent<HealthComponent>()) return;
    if (amount <= 0.0f) return;

    auto& h = target.getComponent<HealthComponent>();
    if (h.dead) return;  // ya muerto: idempotente

    // F4H4 — La armor (si existe) come un % del damage primero. El sobrante
    // va al HP. Convencion HL2 (default 0.66). Si armor.current==0 o no hay
    // ArmorComponent, todo el damage pasa derecho al HP.
    f32 dmgToHp = amount;
    if (target.hasComponent<ArmorComponent>()) {
        auto& armor = target.getComponent<ArmorComponent>();
        if (armor.current > 0.0f && armor.absorbRatio > 0.0f) {
            const f32 wanted   = amount * std::clamp(armor.absorbRatio, 0.0f, 1.0f);
            const f32 absorbed = std::min(wanted, armor.current);
            armor.current -= absorbed;
            dmgToHp        = amount - absorbed;
        }
    }

    const f32 before = h.current;
    h.current = std::max(0.0f, h.current - dmgToHp);
    h.lastDamageTime = 0.0f;  // F4H2 lo puede usar para timers de pain animation

    const std::string tagName = target.hasComponent<TagComponent>()
        ? target.getComponent<TagComponent>().name
        : std::string("<sin-tag>");

    if (h.current <= 0.0f && !h.dead) {
        h.dead = true;
        h.hitFlashTimer = k_deathFlashSec;
        Log::engine()->info(
            "[health] '{}' MUERTO (damage={:.1f}, {:.1f} -> 0/{:.1f})",
            tagName, amount, before, h.max);
    } else {
        h.hitFlashTimer = k_hitFlashSec;
        Log::engine()->info(
            "[health] '{}' damage={:.1f} ({:.1f} -> {:.1f}/{:.1f})",
            tagName, amount, before, h.current, h.max);
    }
}

void heal(Scene& scene, Entity target, f32 amount) {
    (void)scene;
    if (!target || !target.hasComponent<HealthComponent>()) return;
    if (amount <= 0.0f) return;

    auto& h = target.getComponent<HealthComponent>();
    if (h.dead) return;  // morir es definitivo en F4H1

    const f32 before = h.current;
    h.current = std::min(h.max, h.current + amount);

    const std::string tagName = target.hasComponent<TagComponent>()
        ? target.getComponent<TagComponent>().name
        : std::string("<sin-tag>");
    Log::engine()->info(
        "[health] '{}' heal={:.1f} ({:.1f} -> {:.1f}/{:.1f})",
        tagName, amount, before, h.current, h.max);
}

void tickSystem(Scene& scene, f32 dt) {
    auto& reg = scene.registry();
    reg.view<HealthComponent>().each([&](entt::entity h, HealthComponent& hc) {
        // Decay flash on-hit.
        if (hc.hitFlashTimer > 0.0f) {
            hc.hitFlashTimer = std::max(0.0f, hc.hitFlashTimer - dt);
        }
        // F4H1 D2: al morir, si NO tiene RigidBody, agregamos uno Dynamic
        // para que `updateRigidBodies` lo materialice en Jolt el próximo
        // frame y caiga con gravedad. Si ya tenía RigidBody (Static/
        // Kinematic), F4H1 no lo convierte — agendizable a F4H1.5+.
        if (hc.dead && !reg.all_of<RigidBodyComponent>(h)) {
            // Box default 0.5×0.5×0.5 — match con el cubo del maniquí.
            reg.emplace<RigidBodyComponent>(h,
                RigidBodyComponent::Type::Dynamic,
                RigidBodyComponent::Shape::Box,
                glm::vec3(0.5f),
                1.0f /* mass */);
        }
    });
}

} // namespace Health
} // namespace Mood
