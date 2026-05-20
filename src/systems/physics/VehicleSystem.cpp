#include "systems/physics/VehicleSystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

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
        return vehicle::makeFallbackGenericSedan();
    }
    const VehicleConfigAssetId id = assets.loadVehicleConfig(veh.configPath);
    const vehicle::VehicleConfig* cfg = assets.getVehicleConfig(id);
    if (cfg == nullptr) {
        Log::physics()->warn(
            "VehicleSystem: configPath='{}' no resolvio; fallback default SA.",
            veh.configPath);
        return vehicle::makeFallbackGenericSedan();
    }
    return *cfg;
}

// F2H70: aplica una world matrix a un TransformComponent extrayendo la
// rotacion como quaternion (no euler) para evitar gimbal lock. El path
// anterior usaba glm::extractEulerAngleXYZ -> setear rotationEuler ->
// worldMatrix() re-aplicaba como R = Ry*Rx*Rz; los ordenes no matcheaban
// y rotaciones cerca de singular configs (180° en Y, etc.) producian
// orientaciones visualmente distintas de la matrix original. Ahora se
// guarda el quat y `worldMatrix()` lo aplica directo cuando
// `useQuaternion == true`. Scale se preserva (Jolt no lo toca; el dev
// puede setearlo en el moodmap).
//
// `chassisYOffset` (F2H70 Bloque A): si != 0, se RESTA al `position.y` de
// la matriz mundial antes de escribirla al TC. Asi el TC queda en
// "raw position" (lo que el dev escribio en el moodmap) y el render path
// re-aplica el offset on-the-fly via `chassisRenderYOffset`. Sin esto,
// el TC quedaria con el offset bakeado de la pose Jolt, persistiendose
// mal al guardar y duplicandose al recargar.
void writeWorldMatrixToTransform(const glm::mat4& world,
                                   TransformComponent& tf,
                                   f32 chassisYOffset) {
    tf.position    = glm::vec3(world[3]);
    tf.position.y -= chassisYOffset;
    // glm::quat_cast asume scale unitario en el mat3. Los matrices que
    // vienen de Jolt (chassis Jolt es scale 1) cumplen eso. Si en el
    // futuro algun sistema mete scale != 1 en la matriz fisica, hay que
    // normalizar las columnas R antes del cast.
    tf.rotation       = glm::quat_cast(glm::mat3(world));
    tf.useQuaternion  = true;
}

} // anonymous

f32 chassisRenderYOffset(Entity e, AssetManager& assets) {
    if (!e.hasComponent<MeshRendererComponent>()) return 0.0f;
    const auto& mr = e.getComponent<MeshRendererComponent>();
    const MeshAsset* mesh = assets.getMesh(mr.mesh);
    if (mesh == nullptr) return 0.0f;
    // `-aabbMin.y`: para origin-en-base (aabbMin.y ~= 0), offset 0
    // (modelo apoya cuando TC.position.y = 0). Para origin-en-centro
    // (aabbMin.y ~= -halfHeight), offset = +halfHeight. Cualquier
    // convencion de origin produce el mismo resultado visual: modelo
    // apoyado en piso cuando TC.position.y = 0.
    return -mesh->aabbMin.y;
}

void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets) {
    scene.forEach<VehicleComponent, TransformComponent>(
        [&](Entity e, VehicleComponent& veh, TransformComponent& tf) {
            // --- 1) Materializacion lazy (dirty -> create) ---
            if (veh.dirty && veh.vehicleId == 0) {
                const vehicle::VehicleConfig cfg = resolveConfig(veh, assets);
                if (!vehicle::isValid(cfg)) {
                    Log::physics()->error(
                        "VehicleSystem: VehicleConfig invalido; skip.");
                    veh.dirty = false;  // evitar re-intentos cada frame
                    return;
                }
                // F2H70 Bloque A: auto-spawn-height. Setea
                // `tf.pivotYOffset` para que `tf.worldMatrix()` ya lo
                // incluya transparente (render + spawn Jolt). Asi el dev
                // escribe `position.y=0` (piso) en el moodmap y funciona
                // con cualquier convencion de origin del modelo (base o
                // centro vertical). El SceneLoader hace el mismo paso al
                // cargar, asi que en Editor mode (sin Play) tambien aplica.
                // Aqui es fallback para entities creadas por el editor
                // sin pasar por SceneLoader.
                tf.pivotYOffset = chassisRenderYOffset(e, assets);
                veh.vehicleId = physicsWorld.createVehicle(cfg, tf.worldMatrix());
                if (veh.vehicleId == 0) {
                    Log::physics()->error(
                        "VehicleSystem: createVehicle fallo; vehicle "
                        "desactivado.");
                    veh.dirty = false;
                    return;
                }
                // F2H68: registrar el chassis body en el mapeo
                // body->entity para que el ContactListener pueda saber
                // que un impacto contra este body pertenece a esta
                // entity (necesario para encolar el impacto como
                // "atacante" o "victima").
                const u32 chassisBody = physicsWorld.vehicleChassisBodyId(
                    veh.vehicleId);
                if (chassisBody != 0) {
                    physicsWorld.registerBodyEntity(
                        chassisBody, static_cast<u32>(e.handle()));
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

            // Chassis -> entity Transform. F2H70: pasamos `tf.pivotYOffset`
            // para que `writeWorldMatrixToTransform` reste el offset Y
            // antes de escribir al TC. Asi el TC.position queda en "raw"
            // (lo que el dev escribe en el moodmap) mientras que el
            // `worldMatrix()` lo re-aplica transparente al renderear/spawn.
            writeWorldMatrixToTransform(st.chassisWorld, tf, tf.pivotYOffset);

            // 4 wheels -> child-entity Transforms (si estan cacheadas).
            // Las wheels no llevan offset propio: la pose Jolt ya esta en
            // world absoluto, y la entity wheel no tiene MeshRenderer en el
            // path consolidado (F2H69) — la visual de wheels viene del
            // MeshAsset consolidado del chassis. Asi que pasamos offset 0.
            for (int i = 0; i < vehicle::WheelCount; ++i) {
                if (veh.wheelEntities[i] == 0) continue;
                Entity wheel = scene.entityFromHandle(
                    static_cast<entt::entity>(veh.wheelEntities[i]));
                if (!wheel) continue;
                if (!wheel.hasComponent<TransformComponent>()) continue;
                auto& wtf = wheel.getComponent<TransformComponent>();
                writeWorldMatrixToTransform(st.wheelWorlds[i], wtf, 0.0f);
            }
        });
}

} // namespace Mood::VehicleSystem
