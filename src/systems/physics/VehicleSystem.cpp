#include "systems/physics/VehicleSystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <array>
#include <cmath>

namespace Mood::VehicleSystem {

namespace {

// Tags fijos para las 4 child-entities wheel. Convencion del mapa v1.
constexpr std::array<const char*, vehicle::WheelCount> k_wheelTags = {
    "Wheel_FL", "Wheel_FR", "Wheel_RL", "Wheel_RR",
};

// Busca una entity por tag exacto. Primer match gana (mismo patron que
// LuaBindings_Ragdoll::findByTag).
Entity findEntityByTag(Scene& scene, const char* tag) {
    Entity out;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (out) return;
        if (t.name == tag) out = e;
    });
    return out;
}

// Resuelve la VehicleConfig que la entity quiere usar. Si el path esta
// vacio -> default SA. Si no, delega a `assets.loadVehicleConfig` que
// cachea + parsea el JSON; si falla, el AssetManager ya cae al slot 0
// (default SA) -- aca solo lo invocamos y derefenciamos.
vehicle::VehicleConfig resolveConfig(const VehicleComponent& veh,
                                       AssetManager& assets) {
    if (veh.configPath.empty()) {
        return vehicle::makeDefaultSA();
    }
    const VehicleConfigAssetId id = assets.loadVehicleConfig(veh.configPath);
    const vehicle::VehicleConfig* cfg = assets.getVehicleConfig(id);
    if (cfg == nullptr) {
        Log::physics()->warn(
            "VehicleSystem: configPath='{}' no resolvio; fallback default SA.",
            veh.configPath);
        return vehicle::makeDefaultSA();
    }
    return *cfg;
}

// Extrae euler XYZ de una matrix 4x4 con basis ortonormal (asumiendo
// rotacion ZYX como en glm::extractEulerAngleXYZ). Util para escribir
// de vuelta al TransformComponent del chassis sin invocar quat math.
glm::vec3 eulerFromMat4(const glm::mat4& m) {
    f32 x = 0.0f, y = 0.0f, z = 0.0f;
    glm::extractEulerAngleXYZ(m, x, y, z);
    return glm::vec3(glm::degrees(x), glm::degrees(y), glm::degrees(z));
}

// Aplica una world matrix a un TransformComponent: descompone en pos +
// rotation euler. El scale del TransformComponent se preserva (el
// VehicleSystem no lo toca; queda a discrecion del dev en el mapa).
void writeWorldMatrixToTransform(const glm::mat4& world,
                                   TransformComponent& tf) {
    tf.position = glm::vec3(world[3]);
    tf.rotationEuler = eulerFromMat4(world);
}

} // anonymous

void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets) {
    scene.forEach<VehicleComponent, TransformComponent>(
        [&](Entity, VehicleComponent& veh, TransformComponent& tf) {
            // --- 1) Materializacion lazy (dirty -> create) ---
            if (veh.dirty && veh.vehicleId == 0) {
                const vehicle::VehicleConfig cfg = resolveConfig(veh, assets);
                if (!vehicle::isValid(cfg)) {
                    Log::physics()->error(
                        "VehicleSystem: VehicleConfig invalido; skip.");
                    veh.dirty = false;  // evitar re-intentos cada frame
                    return;
                }
                veh.vehicleId = physicsWorld.createVehicle(
                    cfg, tf.worldMatrix());
                if (veh.vehicleId == 0) {
                    Log::physics()->error(
                        "VehicleSystem: createVehicle fallo; vehicle "
                        "desactivado.");
                    veh.dirty = false;
                    return;
                }

                // Buscar las 4 wheel-entities por tag fijo. Si faltan,
                // log warn pero no aborto -- el physics sigue corriendo
                // sin sync visual de esa wheel.
                for (int i = 0; i < vehicle::WheelCount; ++i) {
                    Entity w = findEntityByTag(scene, k_wheelTags[i]);
                    if (!w) {
                        Log::physics()->warn(
                            "VehicleSystem: no se encontro entity con tag "
                            "'{}' -- la rueda no sincronizara su Transform "
                            "visual (physics sigue activa).",
                            k_wheelTags[i]);
                        veh.wheelEntities[i] = 0;
                        continue;
                    }
                    veh.wheelEntities[i] =
                        static_cast<u32>(w.handle());
                }
                veh.dirty = false;
            }

            if (veh.vehicleId == 0) return;

            // --- 2) Push input al physics ---
            physicsWorld.setVehicleInput(
                veh.vehicleId, veh.inputThrottle, veh.inputBrake,
                veh.inputSteer, veh.inputHandbrake);

            // --- 3) Sync pose post-step ---
            PhysicsWorld::VehicleState st;
            if (!physicsWorld.readVehicleState(veh.vehicleId, st)) return;

            // Chassis -> entity Transform.
            writeWorldMatrixToTransform(st.chassisWorld, tf);

            // 4 wheels -> child-entity Transforms (si estan cacheadas).
            for (int i = 0; i < vehicle::WheelCount; ++i) {
                if (veh.wheelEntities[i] == 0) continue;
                Entity wheel = scene.entityFromHandle(
                    static_cast<entt::entity>(veh.wheelEntities[i]));
                if (!wheel) continue;
                if (!wheel.hasComponent<TransformComponent>()) continue;
                auto& wtf = wheel.getComponent<TransformComponent>();
                writeWorldMatrixToTransform(st.wheelWorlds[i], wtf);
            }
        });
}

} // namespace Mood::VehicleSystem
