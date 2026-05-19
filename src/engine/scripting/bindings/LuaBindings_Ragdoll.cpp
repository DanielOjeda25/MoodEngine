// F2H66 Bloque E: tabla `ragdoll` para los scripts Lua.
//
// API expuesta:
//   ragdoll.enable(tag)                          -- transiciona a Ragdolling
//   ragdoll.enable(tag, {x=, y=, z=})            -- + impulse al torso (HL2)
//   ragdoll.is_ragdolling(tag) -> bool
//
// Convencion HL2: una vez activado el ragdoll, queda activado. El
// "impulse" simula el efecto de la bala / cohete que mato al NPC — el
// cadaver vuela en esa direccion. Sin impulse, el ragdoll solo cae por
// gravedad.
//
// La activacion solo setea el estado del componente + el impulse pending.
// La materializacion fisica real (bodies + constraints en Jolt) la hace
// el `RagdollSystem` en el siguiente tick. Mismo patron lazy que
// JointComponent.dirty del F2H65.

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

namespace {

// Busca la entity con el tag dado. Primera match gana — convencion
// "tags unicos" del editor (Hammer-style). Si no encuentra retorna falsy.
Entity findByTag(Scene& scene, const std::string& tag) {
    Entity out;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (out) return;
        if (t.name == tag) out = e;
    });
    return out;
}

// Aplicacion comun: setea state + spawn impulse + dirty=true. No-op si
// el ragdoll ya esta activo (convencion HL2: una vez ragdoll, queda).
bool enableImpl(Scene& scene, const std::string& tag,
                const glm::vec3& impulse) {
    Entity e = findByTag(scene, tag);
    if (!e) {
        Log::script()->warn(
            "ragdoll.enable: no se encontro entity con tag '{}'", tag);
        return false;
    }
    if (!e.hasComponent<RagdollComponent>()) {
        Log::script()->warn(
            "ragdoll.enable: entity '{}' no tiene RagdollComponent — "
            "agregarselo desde el Inspector o serializado.",
            tag);
        return false;
    }
    auto& rag = e.getComponent<RagdollComponent>();
    if (rag.state == RagdollComponent::State::Ragdolling) {
        // Ya esta ragdolleando — no-op silencioso para que el script
        // pueda llamar enable() varias veces sin loguear ruido.
        return false;
    }
    rag.state        = RagdollComponent::State::Ragdolling;
    rag.spawnImpulse = impulse;
    Log::script()->info(
        "ragdoll.enable: '{}' transicion a Ragdolling (impulse=({:.2f}, {:.2f}, {:.2f}))",
        tag, impulse.x, impulse.y, impulse.z);
    return true;
}

} // anonymous

void setupRagdollBindings(sol::state& lua, Scene* scene) {
    sol::table t = lua.create_named_table("ragdoll");

    // ragdoll.enable(tag) — sin impulse.
    // ragdoll.enable(tag, {x=, y=, z=}) — con impulse.
    t.set_function("enable",
        sol::overload(
            [scene](const std::string& tag) -> bool {
                if (scene == nullptr) {
                    Log::script()->warn("ragdoll.enable: scene es nullptr");
                    return false;
                }
                return enableImpl(*scene, tag, glm::vec3(0.0f));
            },
            [scene](const std::string& tag, sol::table impulse) -> bool {
                if (scene == nullptr) {
                    Log::script()->warn("ragdoll.enable: scene es nullptr");
                    return false;
                }
                const f32 x = impulse.get_or("x", 0.0f);
                const f32 y = impulse.get_or("y", 0.0f);
                const f32 z = impulse.get_or("z", 0.0f);
                return enableImpl(*scene, tag, glm::vec3(x, y, z));
            }
        ));

    t.set_function("is_ragdolling", [scene](const std::string& tag) -> bool {
        if (scene == nullptr) return false;
        Entity e = findByTag(*scene, tag);
        if (!e || !e.hasComponent<RagdollComponent>()) return false;
        return e.getComponent<RagdollComponent>().state
                 == RagdollComponent::State::Ragdolling;
    });
}

} // namespace Mood
