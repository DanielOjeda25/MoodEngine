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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Mood::VehicleSystem {

namespace {

// F2H70.2 debug: estado por-vehicle para emitir logs de input + velocidad sin
// floodear. Edge-trigger en throttle/brake/handbrake (logueamos solo cuando
// cambian estos de 0 -> nonzero o viceversa) + sample periodico de speed
// cada ~0.5s mientras el chasis esta en movimiento o con input activo. Sin
// esto el dev no puede saber si "el auto sigue moviendose al soltar el gas"
// es porque (a) el input no se solto, (b) hay momentum residual del damping,
// o (c) la gravedad en pendiente lo empuja. La elegimos correr siempre
// (no detras de un flag) porque la log line es rara y deja un trail util
// post-mortem si el dev reporta un bug de feel.
struct VehicleDebugState {
    f32 prevThrottle = 0.0f;
    f32 prevBrake    = 0.0f;
    f32 prevHandbrake = 0.0f;
    std::chrono::steady_clock::time_point lastSpeedLog =
        std::chrono::steady_clock::now();
    f32 lastLoggedSpeed = 0.0f;
    bool wasMoving = false;
};

// Storage de debug states. Limpieza al destroy del vehicle es responsabilidad
// del PhysicsWorld -> entity teardown; aca solo crece on-demand. Asumimos
// que un editor session no spawnea miles de vehicles distintos.
std::unordered_map<u32, VehicleDebugState>& debugStates() {
    static std::unordered_map<u32, VehicleDebugState> s;
    return s;
}

// Umbral por debajo del cual consideramos el input "soltado" (no logueamos
// fluctuaciones inferiores como edges falsos).
constexpr f32 k_inputEdgeThreshold = 0.05f;
// Speed por debajo del cual el auto se considera "parado" y dejamos de
// sampleo periodico (sino tendriamos un log/s al spawn).
constexpr f32 k_movingThreshold = 0.1f;
// Periodo entre samples periodicos de speed mientras se mueve.
constexpr f32 k_speedLogPeriodSec = 0.5f;

// Helper: edge detector con umbral. Retorna true si el input cruzo de
// "soltado" a "presionado" o viceversa entre prev y curr.
bool inputEdge(f32 prev, f32 curr) {
    const bool wasOn = std::fabs(prev) >= k_inputEdgeThreshold;
    const bool isOn  = std::fabs(curr) >= k_inputEdgeThreshold;
    return wasOn != isOn;
}

// F2H70.3 H: nombres canonicos de los sub-meshes de rueda en el .glb
// (centrados en su hub por tools/glb/split_wheels.py). Orden = WheelIndex
// (FL, FR, RL, RR). El chassis los excluye via hideSubMeshPrefix="wheel_" y
// 4 wheel-entities los renderean por separado, sincronizadas a wheelWorlds[i].
constexpr std::array<const char*, vehicle::WheelCount> k_wheelSubMesh = {
    "wheel_FL", "wheel_FR", "wheel_RL", "wheel_RR",
};

// F2H82 Bloque B: por cada rueda, que sub-mesh del .glb renderear y cuanto
// correrlo para que rote en su hub.
struct WheelMeshBinding {
    std::string subMesh;
    glm::vec3   pivot{0.0f};
};

// Resuelve los bindings desde el config: si la rueda trae un `meshSubName`
// real (auto importado SIN procesar), se usa ese nombre + su `meshHubOffset`.
// Si esta vacio, cae al canonico `wheel_FL/FR/RL/RR` con pivot 0 (DeLorean y
// cualquier modelo ya centrado por tools/glb/split_wheels.py).
std::array<WheelMeshBinding, vehicle::WheelCount>
wheelMeshBindings(const vehicle::VehicleConfig& cfg) {
    std::array<WheelMeshBinding, vehicle::WheelCount> out;
    for (int i = 0; i < vehicle::WheelCount; ++i) {
        const vehicle::WheelConfig& w = cfg.wheels[i];
        out[i].subMesh = w.meshSubName.empty() ? std::string(k_wheelSubMesh[i])
                                               : w.meshSubName;
        out[i].pivot   = w.meshHubOffset;
    }
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
                                   f32 chassisYOffset,
                                   f32 yawOffsetDeg) {
    tf.position    = glm::vec3(world[3]);
    tf.position.y -= chassisYOffset;
    // F2H70.2 D5: el `tf.pivotYawOffsetDeg` (si != 0) se aplica en
    // `worldMatrix()` como POST-multiply al rotation del dev. Para que la
    // rotacion serializada al TC quede en el frame "logico" (lo que el
    // dev escribiria en el moodmap) y NO acumule el yaw cada save/reload,
    // restamos el yaw acá: `R_logic = R_world * Ry(-yawOffsetDeg)`. Si
    // `yawOffsetDeg == 0` esto es identidad y no rompe nada.
    glm::mat3 R = glm::mat3(world);
    if (std::fabs(yawOffsetDeg) > 1e-3f) {
        const glm::mat3 yawInv = glm::mat3(glm::rotate(
            glm::mat4(1.0f),
            glm::radians(-yawOffsetDeg),
            glm::vec3(0.0f, 1.0f, 0.0f)));
        R = R * yawInv;
    }
    // glm::quat_cast asume scale unitario en el mat3. Los matrices que
    // vienen de Jolt (chassis Jolt es scale 1) cumplen eso. Si en el
    // futuro algun sistema mete scale != 1 en la matriz fisica, hay que
    // normalizar las columnas R antes del cast.
    tf.rotation       = glm::quat_cast(R);
    tf.useQuaternion  = true;
}

} // anonymous

f32 chassisRenderYOffset(Entity e, AssetManager& assets) {
    // (1) Mesh offset: `-aabbMin.y` posiciona la base del modelo en piso.
    // Para origin-en-base (aabbMin.y ~= 0), offset 0. Para origin-en-centro
    // (aabbMin.y ~= -halfHeight), offset = +halfHeight. Cualquier convencion
    // de origin produce el mismo resultado visual: modelo apoyado en piso
    // cuando TC.position.y = 0.
    f32 meshOffset = 0.0f;
    if (e.hasComponent<MeshRendererComponent>()) {
        const auto& mr = e.getComponent<MeshRendererComponent>();
        const MeshAsset* mesh = assets.getMesh(mr.mesh);
        if (mesh != nullptr) {
            meshOffset = -mesh->aabbMin.y;
        }
    }

    // (2) F2H70.2 Bloque B — spring rest compression. Jolt spawnea las
    // wheels con `suspensionMaxLength` extendida (sin gravedad aplicada
    // aun). Despues del primer step la gravedad comprime el spring hasta
    // el equilibrio, bajando el chassis `x = g/(2π·f)²` (~7-10 cm para
    // f=1.5-1.8 Hz). Sin compensar, el chassis flota visiblemente al spawn
    // antes de "asentarse". Sumar el spring offset al meshOffset eleva el
    // chassis al spawn de modo que el settle natural lo deje exactamente
    // apoyado en piso.
    f32 springOffset = 0.0f;
    if (e.hasComponent<VehicleComponent>()) {
        const auto& veh = e.getComponent<VehicleComponent>();
        vehicle::VehicleConfig cfg;
        if (!veh.configPath.empty()) {
            const VehicleConfigAssetId id =
                assets.loadVehicleConfig(veh.configPath);
            const auto* loaded = assets.getVehicleConfig(id);
            cfg = (loaded != nullptr) ? *loaded
                                       : vehicle::makeFallbackGenericSedan();
        } else {
            cfg = vehicle::makeFallbackGenericSedan();
        }
        // Asumimos las 4 wheels comparten freq (caso 99% real; si difieren,
        // el offset siguen un compromiso entre ambos axles).
        springOffset = vehicle::wheelRestCompression(cfg.wheels[0]);
    }

    return meshOffset + springOffset;
}

// F2H70.3 H: true si el vehiculo necesita (re)spawnear sus wheel-entities.
// Cubre 3 casos: nunca spawneadas (handle 0), o el handle quedo stale (la
// entity fue destruida — p.ej. al borrar+undo el chassis, o tras recargar el
// mapa donde wheelEntities[] no persiste). Comparar contra entity invalida
// evita ruedas faltantes en esos casos.
bool wheelsNeedSpawn(const VehicleComponent& veh, Scene& scene) {
    if (veh.wheelEntities[0] == 0) return true;
    return !scene.entityFromHandle(
        static_cast<entt::entity>(veh.wheelEntities[0]));
}

// F2H70.3 H: crea las 4 wheel-entities de cada chassis encolado. Comparten el
// MeshAssetId + materiales del chassis y renderean solo su sub-mesh wheel_*
// (centrado en el hub). Se llama DESPUES del forEach de la view para no crear
// entities (con Transform) mientras iteramos. Lo usan tick() (Play) y
// previewRest() (Editor).
void spawnPendingWheels(Scene& scene, AssetManager& assets,
                        const std::vector<entt::entity>& pending) {
    for (entt::entity chassisHandle : pending) {
        Entity chassis = scene.entityFromHandle(chassisHandle);
        if (!chassis || !chassis.hasComponent<VehicleComponent>()
            || !chassis.hasComponent<MeshRendererComponent>()) {
            continue;
        }
        auto& veh = chassis.getComponent<VehicleComponent>();

        // Leemos TODO del chassisMr antes de crear entities: addComponent en
        // otra entity puede reallocar el pool de MeshRendererComponent e
        // invalidar esta referencia.
        const vehicle::VehicleConfig cfg = resolveConfig(veh, assets);
        const auto bind = wheelMeshBindings(cfg);
        const bool imported = !cfg.wheels[0].meshSubName.empty();

        auto& chassisMr = chassis.getComponent<MeshRendererComponent>();
        const MeshAssetId meshId = chassisMr.mesh;
        const std::vector<MaterialAssetId> mats = chassisMr.materials;
        // El chassis no dibuja sus ruedas: por prefijo `wheel_` (modelo
        // procesado/canonico) o por la lista exacta de nombres reales (auto
        // importado sin procesar).
        if (imported) {
            chassisMr.hideSubMeshPrefix.clear();
            chassisMr.hideSubMeshNames.clear();
            for (int i = 0; i < vehicle::WheelCount; ++i) {
                chassisMr.hideSubMeshNames.push_back(bind[i].subMesh);
            }
        } else {
            chassisMr.hideSubMeshPrefix = "wheel_";
        }

        // A partir de aca NO tocar chassisMr (ref potencialmente invalidada).
        const u32 chassisHandleU32 = static_cast<u32>(chassisHandle);
        for (int i = 0; i < vehicle::WheelCount; ++i) {
            Entity w = scene.createEntity(bind[i].subMesh.c_str());
            auto& wmr = w.addComponent<MeshRendererComponent>(meshId, mats);
            wmr.subMeshName = bind[i].subMesh;
            wmr.subMeshPivotOffset = bind[i].pivot;
            // F2H82: marker para que el editor (picking / jerarquia / serial)
            // trate a la rueda como entity interna del motor, NO seleccionable
            // ni listada. Reemplaza el check legacy por nombre canonico que
            // dejaba escapar las ruedas con nombres reales (ej. `f_t_l`).
            w.addComponent<VehicleWheelMarker>(
                VehicleWheelMarker{chassisHandleU32, i});
            veh.wheelEntities[i] = static_cast<u32>(w.handle());
        }
    }
}

void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets) {
    // F2H70.3 H: chassis entities que necesitan spawn de wheel-entities. Se
    // procesan despues del forEach para no crear entities (con Transform)
    // mientras iteramos la view sobre TransformComponent.
    std::vector<entt::entity> pendingWheelSpawn;

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

                // F2H70.2 D5: `pivotYawOffsetDeg` se setea desde el campo
                // `mesh_yaw_offset_deg` del .moodvehicle. Patron simetrico
                // al `pivotYOffset`: el TC queda en el frame "logico" (lo
                // que el dev escribe en el moodmap), y `worldMatrix()` lo
                // re-aplica transparente — afectando tanto el render del
                // editor (sin Play) como la pose Jolt al spawn fisico.
                // Drag-drop: cualquier GLB con convencion no-estandar
                // funciona con SOLO setear el campo del .moodvehicle.
                tf.pivotYawOffsetDeg = cfg.meshYawOffsetDeg;
                // `tf.worldMatrix()` ahora compone Ry(yawOffset) al final,
                // asi que el chasis fisico arranca con la rotacion bakeada
                // sin necesidad de componer aca.
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

                // F2H70.3 H: las 4 wheel-entities se auto-spawnean DESPUES
                // del forEach (crear entities con TransformComponent aca
                // invalidaria el iterador de la view). Si el chassis tiene
                // mesh, lo encolamos para spawn. El exclude de los sub-meshes
                // de rueda lo setea `spawnPendingWheels` (sabe si el modelo es
                // canonico `wheel_*` o un import con nombres reales).
                if (e.hasComponent<MeshRendererComponent>()
                    && wheelsNeedSpawn(veh, scene)) {
                    pendingWheelSpawn.push_back(e.handle());
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

            // F2H70.2 debug: log de inputs (edge) + velocidad (periodico).
            // Ayuda a diagnosticar reportes tipo "se sigue moviendo despues
            // de soltar el gas" — la trace dice exactamente cuando entro/
            // salio cada input y como decaye la velocidad. Si en futuro el
            // dev reporta que el log es molesto, se gatea detras de un flag
            // del .moodproj (loggingVehicleDebug).
            {
                auto& dbg = debugStates()[veh.vehicleId];
                const f32 spd = st.forwardSpeed;
                const auto now = std::chrono::steady_clock::now();

                if (inputEdge(dbg.prevThrottle, veh.inputThrottle)) {
                    Log::physics()->info(
                        "vehicle[{}] THROTTLE {:.2f} -> {:.2f} (speed={:.2f} m/s)",
                        veh.vehicleId, dbg.prevThrottle, veh.inputThrottle, spd);
                }
                if (inputEdge(dbg.prevBrake, veh.inputBrake)) {
                    Log::physics()->info(
                        "vehicle[{}] BRAKE    {:.2f} -> {:.2f} (speed={:.2f} m/s)",
                        veh.vehicleId, dbg.prevBrake, veh.inputBrake, spd);
                }
                if (inputEdge(dbg.prevHandbrake, veh.inputHandbrake)) {
                    Log::physics()->info(
                        "vehicle[{}] HANDBRK  {:.2f} -> {:.2f} (speed={:.2f} m/s)",
                        veh.vehicleId, dbg.prevHandbrake, veh.inputHandbrake, spd);
                }

                const bool isMoving = std::fabs(spd) > k_movingThreshold;
                const f32 elapsed = std::chrono::duration<f32>(
                    now - dbg.lastSpeedLog).count();
                if (isMoving && elapsed >= k_speedLogPeriodSec) {
                    Log::physics()->info(
                        "vehicle[{}] speed={:.2f} m/s (Δ={:+.2f} vs hace {:.2f}s) "
                        "thr={:.2f} brk={:.2f} hbk={:.2f} grounded={}",
                        veh.vehicleId, spd, spd - dbg.lastLoggedSpeed, elapsed,
                        veh.inputThrottle, veh.inputBrake, veh.inputHandbrake,
                        st.grounded);
                    dbg.lastSpeedLog    = now;
                    dbg.lastLoggedSpeed = spd;
                } else if (!isMoving && dbg.wasMoving) {
                    Log::physics()->info(
                        "vehicle[{}] STOPPED (speed={:.3f} m/s) thr={:.2f} "
                        "brk={:.2f} hbk={:.2f}",
                        veh.vehicleId, spd, veh.inputThrottle, veh.inputBrake,
                        veh.inputHandbrake);
                    dbg.lastLoggedSpeed = spd;
                }
                dbg.wasMoving     = isMoving;
                dbg.prevThrottle  = veh.inputThrottle;
                dbg.prevBrake     = veh.inputBrake;
                dbg.prevHandbrake = veh.inputHandbrake;
            }

            // Chassis -> entity Transform. F2H70: pasamos `tf.pivotYOffset`
            // para que `writeWorldMatrixToTransform` reste el offset Y
            // antes de escribir al TC. Asi el TC.position queda en "raw"
            // (lo que el dev escribe en el moodmap) mientras que el
            // `worldMatrix()` lo re-aplica transparente al renderear/spawn.
            writeWorldMatrixToTransform(
                st.chassisWorld, tf, tf.pivotYOffset, tf.pivotYawOffsetDeg);

            // 4 wheels -> child-entity Transforms (si estan cacheadas).
            // Las wheels no llevan offset propio: la pose Jolt (wheelWorlds[i])
            // ya esta en world absoluto e incluye spin + steer. La wheel-entity
            // renderea el sub-mesh wheel_* centrado en su hub (F2H70.3 H), asi
            // que el TransformComponent la posiciona+rota desde cero. Offset 0.
            for (int i = 0; i < vehicle::WheelCount; ++i) {
                if (veh.wheelEntities[i] == 0) continue;
                Entity wheel = scene.entityFromHandle(
                    static_cast<entt::entity>(veh.wheelEntities[i]));
                if (!wheel) continue;
                if (!wheel.hasComponent<TransformComponent>()) continue;
                auto& wtf = wheel.getComponent<TransformComponent>();
                writeWorldMatrixToTransform(st.wheelWorlds[i], wtf, 0.0f, 0.0f);
            }
        });

    spawnPendingWheels(scene, assets, pendingWheelSpawn);
}

// F2H70.3 H: preview de las ruedas en Editor mode (sin Play). El tick de
// fisica no corre, asi que posicionamos las wheel-entities en su pose de
// REPOSO calculada analiticamente desde el config + la pose del chassis. Sin
// esto, las ruedas (sub-meshes centrados en su hub) quedarian colapsadas en
// el centro del auto hasta que el dev entre a Play.
//
// La pose fisica del chassis NO lleva el yaw visual (pivotYawOffsetDeg): el
// mesh se rota 180 para mostrarse, pero las wheel-entities usan poses en
// convencion fisica (+Z forward), igual que en Play (wheelWorlds[i]).
void previewRest(Scene& scene, AssetManager& assets) {
    std::vector<entt::entity> pendingWheelSpawn;

    scene.forEach<VehicleComponent, TransformComponent>(
        [&](Entity e, VehicleComponent& veh, TransformComponent& tf) {
            if (!e.hasComponent<MeshRendererComponent>()) return;

            // Spawn diferido (crear entities con Transform aca invalidaria
            // la view). El exclude lo setea `spawnPendingWheels`.
            if (wheelsNeedSpawn(veh, scene)) {
                pendingWheelSpawn.push_back(e.handle());
                return;
            }

            // Pose fisica del chassis = worldMatrix() SIN el pivotYawOffsetDeg
            // (el yaw visual) ni scale (vehiculos = 1). Asi las ruedas siguen
            // al chassis si el dev lo mueve con el gizmo.
            glm::vec3 effPos = tf.position;
            effPos.y += tf.pivotYOffset;
            glm::mat4 physMat = glm::translate(glm::mat4(1.0f), effPos);
            if (tf.useQuaternion) {
                physMat = physMat * glm::mat4_cast(tf.rotation);
            } else {
                physMat = glm::rotate(physMat, glm::radians(tf.rotationEuler.y),
                                       glm::vec3(0, 1, 0));
                physMat = glm::rotate(physMat, glm::radians(tf.rotationEuler.x),
                                       glm::vec3(1, 0, 0));
                physMat = glm::rotate(physMat, glm::radians(tf.rotationEuler.z),
                                       glm::vec3(0, 0, 1));
            }

            const vehicle::VehicleConfig cfg = resolveConfig(veh, assets);
            for (int i = 0; i < vehicle::WheelCount; ++i) {
                if (veh.wheelEntities[i] == 0) continue;
                Entity wheel = scene.entityFromHandle(
                    static_cast<entt::entity>(veh.wheelEntities[i]));
                if (!wheel || !wheel.hasComponent<TransformComponent>()) continue;

                const vehicle::WheelConfig& w = cfg.wheels[i];
                // En reposo la suspension se comprime `wheelRestCompression`
                // desde max. El centro de la rueda cuelga del attach por la
                // longitud resultante (clamp al min de la suspension).
                const f32 comp = vehicle::wheelRestCompression(w);
                const f32 restLen = std::max(
                    w.suspensionMinLength, w.suspensionMaxLength - comp);
                const glm::vec3 wheelLocal =
                    w.attachLocal - glm::vec3(0.0f, restLen, 0.0f);
                const glm::mat4 wheelWorld =
                    physMat * glm::translate(glm::mat4(1.0f), wheelLocal);

                auto& wtf = wheel.getComponent<TransformComponent>();
                writeWorldMatrixToTransform(wheelWorld, wtf, 0.0f, 0.0f);
            }
        });

    spawnPendingWheels(scene, assets, pendingWheelSpawn);
}

} // namespace Mood::VehicleSystem
