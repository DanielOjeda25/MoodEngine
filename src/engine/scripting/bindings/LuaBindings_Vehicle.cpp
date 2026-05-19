// F2H67 Bloque E: tabla `vehicle` para los scripts Lua.
//
// Convencion: el input se escribe en `VehicleComponent.inputThrottle/brake/
// steer/handbrake` cada frame; el `VehicleSystem` lo lee y lo pushea al
// `PhysicsWorld::setVehicleInput`. La separacion permite que el binding
// sea barato (solo modifica memoria del componente) y que el "frame del
// physics" centralice el dispatch a Jolt.
//
// API:
//   vehicle.set_input(tag, {throttle=, brake=, steer=, handbrake=})
//     -- valores opcionales; los faltantes quedan en 0.
//   vehicle.get_speed(tag) -> float (m/s en la direccion forward del chasis)
//   vehicle.is_grounded(tag) -> bool (al menos 1 rueda toca)
//   vehicle.respawn(tag, {x=,y=,z=}, rotYDeg)
//     -- teleport del chassis a posicion + rotacion Y (cero pitch/roll).
//        Util cuando el dev vuelca el auto y quiere reset.
//   vehicle.gear(tag) -> int (-1 reverse, 0 neutral, 1..N)
//   vehicle.rpm(tag)  -> float
//
// Patrones consistentes con `ragdoll.enable` y `inventory.give`: lookup
// por tag (primer match gana), warn + retorno default si no se encuentra.

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <string>

namespace Mood {

namespace {

Entity findByTag(Scene& scene, const std::string& tag) {
    Entity out;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (out) return;
        if (t.name == tag) out = e;
    });
    return out;
}

VehicleComponent* findVehicleByTag(Scene& scene, const std::string& tag,
                                     const char* fn) {
    Entity e = findByTag(scene, tag);
    if (!e) {
        Log::script()->warn("{}: entity con tag '{}' no encontrada", fn, tag);
        return nullptr;
    }
    if (!e.hasComponent<VehicleComponent>()) {
        Log::script()->warn(
            "{}: entity '{}' no tiene VehicleComponent", fn, tag);
        return nullptr;
    }
    return &e.getComponent<VehicleComponent>();
}

} // anonymous

void setupVehicleBindings(sol::state& lua, Scene* scene,
                            PhysicsWorld* physicsWorld) {
    sol::table t = lua.create_named_table("vehicle");

    // vehicle.set_input(tag, {throttle=,brake=,steer=,handbrake=})
    // Valores faltantes default a 0. Magnitud clampeada por el wrapper de
    // PhysicsWorld en setVehicleInput.
    t.set_function("set_input",
        [scene](const std::string& tag, sol::table input) -> bool {
            if (scene == nullptr) {
                Log::script()->warn("vehicle.set_input: scene es nullptr");
                return false;
            }
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.set_input");
            if (veh == nullptr) return false;
            veh->inputThrottle  = input.get_or("throttle",  0.0f);
            veh->inputBrake     = input.get_or("brake",     0.0f);
            veh->inputSteer     = input.get_or("steer",     0.0f);
            veh->inputHandbrake = input.get_or("handbrake", 0.0f);
            return true;
        });

    t.set_function("get_speed",
        [scene, physicsWorld](const std::string& tag) -> f32 {
            if (scene == nullptr || physicsWorld == nullptr) return 0.0f;
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.get_speed");
            if (veh == nullptr || veh->vehicleId == 0) return 0.0f;
            PhysicsWorld::VehicleState st;
            if (!physicsWorld->readVehicleState(veh->vehicleId, st)) return 0.0f;
            return st.forwardSpeed;
        });

    t.set_function("is_grounded",
        [scene, physicsWorld](const std::string& tag) -> bool {
            if (scene == nullptr || physicsWorld == nullptr) return false;
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.is_grounded");
            if (veh == nullptr || veh->vehicleId == 0) return false;
            PhysicsWorld::VehicleState st;
            if (!physicsWorld->readVehicleState(veh->vehicleId, st)) return false;
            return st.grounded;
        });

    t.set_function("gear",
        [scene, physicsWorld](const std::string& tag) -> int {
            if (scene == nullptr || physicsWorld == nullptr) return 0;
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.gear");
            if (veh == nullptr || veh->vehicleId == 0) return 0;
            PhysicsWorld::VehicleState st;
            if (!physicsWorld->readVehicleState(veh->vehicleId, st)) return 0;
            return st.currentGear;
        });

    t.set_function("rpm",
        [scene, physicsWorld](const std::string& tag) -> f32 {
            if (scene == nullptr || physicsWorld == nullptr) return 0.0f;
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.rpm");
            if (veh == nullptr || veh->vehicleId == 0) return 0.0f;
            PhysicsWorld::VehicleState st;
            if (!physicsWorld->readVehicleState(veh->vehicleId, st)) return 0.0f;
            return st.engineRPM;
        });

    // vehicle.respawn(tag, {x=,y=,z=}, rotYDeg)
    // Teleport del chassis. rotYDeg = 0 mira hacia +Z (forward del config).
    // El wrapper destruye + recrea el vehicle internamente, mas simple
    // que tratar de "setear pose" sobre un constraint vivo de Jolt.
    t.set_function("respawn",
        [scene, physicsWorld](const std::string& tag, sol::table pos,
                                 f32 rotYDeg) -> bool {
            if (scene == nullptr || physicsWorld == nullptr) {
                Log::script()->warn(
                    "vehicle.respawn: scene/physicsWorld nullptr");
                return false;
            }
            VehicleComponent* veh =
                findVehicleByTag(*scene, tag, "vehicle.respawn");
            if (veh == nullptr) return false;
            const f32 x = pos.get_or("x", 0.0f);
            const f32 y = pos.get_or("y", 0.0f);
            const f32 z = pos.get_or("z", 0.0f);
            // Destruir el vehicle actual; el VehicleSystem lo recreara con
            // los nuevos valores del Transform en el proximo tick.
            if (veh->vehicleId != 0) {
                physicsWorld->destroyVehicle(veh->vehicleId);
                veh->vehicleId = 0;
            }
            // Setear el Transform de la entity al respawn target.
            Entity e = findByTag(*scene, tag);
            if (e && e.hasComponent<TransformComponent>()) {
                auto& tf = e.getComponent<TransformComponent>();
                tf.position = glm::vec3(x, y, z);
                tf.rotationEuler = glm::vec3(0.0f, rotYDeg, 0.0f);
            }
            // Resetear input para que el respawn no traiga input residual.
            veh->inputThrottle  = 0.0f;
            veh->inputBrake     = 0.0f;
            veh->inputSteer     = 0.0f;
            veh->inputHandbrake = 0.0f;
            veh->dirty = true;
            Log::script()->info(
                "vehicle.respawn: '{}' a ({:.2f}, {:.2f}, {:.2f}) rotY={:.1f}°",
                tag, x, y, z, rotYDeg);
            return true;
        });
}

} // namespace Mood
