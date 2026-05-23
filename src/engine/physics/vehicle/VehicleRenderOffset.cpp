#include "engine/physics/vehicle/VehicleRenderOffset.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"

namespace Mood::vehicle {

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

} // namespace Mood::vehicle
