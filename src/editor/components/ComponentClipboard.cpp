#include "editor/components/ComponentClipboard.h"

#include "core/Log.h"  // F3H11: warn al clampear material indices del Brush
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"  // F3H11: paste Brush
#include "engine/scene/components/Components.h"
#include "engine/scene/serialization/EntitySerializer.h"
#include "engine/scene/serialization/SceneLoader.h"   // F3H11: applyBrushFromSaved
#include "engine/scene/serialization/SceneSerializer.h"  // F3H11: serializeBrush/parseBrush

#include <unordered_set>

namespace Mood::ComponentClipboard {

namespace {

const std::unordered_set<std::string>& supportedKeys() {
    static const std::unordered_set<std::string> k = {
        kKeyLight, kKeyTrigger, kKeyForceField, kKeyParticleEmitter,
        // F3H10: Tier 2 — types con SavedX en SavedEntity.
        kKeyMeshRenderer, kKeyDialog, kKeyItemPickup, kKeyVehicle, kKeyEnvironment,
        // F3H11: Tier 3 — bundle Audio/Camera/Brush.
        kKeyAudioSource, kKeyCamera, kKeyBrush,
    };
    return k;
}

// Apliers per-componentKey. Replican la logica de SceneLoader::applyOneEntity
// pero mutan un componente existente (en lugar de addComponent over fresh
// entity). Si la entidad no tiene el componente, lo agregamos primero con
// defaults y luego sobrescribimos los fields del payload.

// Helper: wrap payload sub-object en un fake-entity JSON y parsear via
// EntitySerializer. Devuelve SavedEntity poblado SOLO con el componente
// del componentKey indicado. Reusa el codigo de parsing existente.
SavedEntity parsePayloadAsSavedEntity(const std::string& componentKey,
                                        const nlohmann::json& payload) {
    nlohmann::json wrap = nlohmann::json::object();
    wrap["tag"] = "__clipboard_tmp__";  // requerido por parseEntityFromJson, no se usa
    wrap[componentKey] = payload;
    return parseEntityFromJson(wrap);
}

void applyLight(const SavedLight& sl, Entity& e) {
    if (!e.hasComponent<LightComponent>()) {
        e.addComponent<LightComponent>();
    }
    auto& lc = e.getComponent<LightComponent>();
    lc.type = (sl.type == "directional")
        ? LightComponent::Type::Directional
        : LightComponent::Type::Point;
    lc.color       = sl.color;
    lc.intensity   = sl.intensity;
    lc.radius      = sl.radius;
    lc.direction   = sl.direction;
    lc.enabled     = sl.enabled;
    lc.castShadows = sl.castShadows;
}

void applyTrigger(const SavedTrigger& s, Entity& e) {
    if (!e.hasComponent<TriggerComponent>()) {
        e.addComponent<TriggerComponent>();
    }
    auto& tc = e.getComponent<TriggerComponent>();
    tc.halfExtents      = s.halfExtents;
    tc.requiredTag      = s.requiredTag;
    tc.triggersOnPlayer = s.triggersOnPlayer;
    tc.oneShot          = s.oneShot;
    tc.enabled          = s.enabled;
}

void applyForceField(const SavedForceField& s, Entity& e) {
    if (!e.hasComponent<ForceFieldComponent>()) {
        e.addComponent<ForceFieldComponent>();
    }
    auto& ff = e.getComponent<ForceFieldComponent>();
    ff.shape = (s.shape == "box")
        ? ForceFieldComponent::Shape::Box
        : ForceFieldComponent::Shape::Sphere;
    ff.mode = (s.type == "directional")
        ? ForceFieldComponent::Mode::Directional
        : ForceFieldComponent::Mode::Radial;
    ff.halfExtents   = s.halfExtents;
    ff.radius        = s.radius;
    ff.direction     = s.direction;
    ff.strength      = s.strength;
    ff.linearFalloff = s.linearFalloff;
    ff.ignoreMass    = s.ignoreMass;
    ff.enabled       = s.enabled;
}

// F3H10: Tier 2 appliers. Mismo patron que Tier 1 — addComponent si falta +
// sobrescribir fields. Resolucion de paths via AssetManager.

void applyMeshRenderer(const SavedMeshRenderer& s, Entity& e,
                          AssetManager& assets) {
    const MeshAssetId meshId = s.meshPath.empty()
        ? assets.missingMeshId()
        : assets.loadMesh(s.meshPath);
    std::vector<MaterialAssetId> mats;
    mats.reserve(s.materials.size());
    for (const auto& matPath : s.materials) {
        mats.push_back(matPath.empty()
            ? assets.missingMaterialId()
            : assets.loadMaterial(matPath));
    }
    if (mats.empty()) {
        mats = assets.createMaterialsForMesh(meshId);
    }
    if (!e.hasComponent<MeshRendererComponent>()) {
        e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
    } else {
        auto& mr = e.getComponent<MeshRendererComponent>();
        mr.mesh        = meshId;
        mr.materials   = std::move(mats);
        mr.subMeshName = s.subMeshName;
    }
}

void applyDialog(const SavedDialog& s, Entity& e) {
    if (!e.hasComponent<DialogComponent>()) {
        e.addComponent<DialogComponent>();
    }
    auto& dc = e.getComponent<DialogComponent>();
    dc.dialogPath          = s.dialogPath;
    dc.autoStartOnInteract = s.autoStartOnInteract;
}

void applyItemPickup(const SavedItemPickup& s, Entity& e) {
    if (!e.hasComponent<ItemPickupComponent>()) {
        e.addComponent<ItemPickupComponent>();
    }
    auto& ip = e.getComponent<ItemPickupComponent>();
    ip.itemPath        = s.itemPath;
    ip.quantity        = s.quantity;
    ip.destroyOnPickup = s.destroyOnPickup;
}

void applyVehicle(const SavedVehicle& s, Entity& e) {
    if (!e.hasComponent<VehicleComponent>()) {
        e.addComponent<VehicleComponent>();
    }
    auto& vc = e.getComponent<VehicleComponent>();
    vc.configPath = s.configPath;
    vc.dirty      = true;  // forzar reload del .moodvehicle al primer frame
}

void applyEnvironment(const SavedEnvironment& s, Entity& e) {
    if (!e.hasComponent<EnvironmentComponent>()) {
        e.addComponent<EnvironmentComponent>();
    }
    auto& env = e.getComponent<EnvironmentComponent>();
    env.skyboxPath     = s.skyboxPath;
    // FogMode: 0=Off, 1=Linear, 2=Exp, 3=Exp2 (raw u32 en el componente).
    if      (s.fogMode == "linear") env.fogMode = 1u;
    else if (s.fogMode == "exp")    env.fogMode = 2u;
    else if (s.fogMode == "exp2")   env.fogMode = 3u;
    else                             env.fogMode = 0u;
    env.fogColor       = s.fogColor;
    env.fogDensity     = s.fogDensity;
    env.fogLinearStart = s.fogLinearStart;
    env.fogLinearEnd   = s.fogLinearEnd;
    env.exposure       = s.exposure;
    // TonemapMode: 0=None, 1=Reinhard, 2=ACES.
    if      (s.tonemapMode == "reinhard") env.tonemapMode = 1u;
    else if (s.tonemapMode == "aces")     env.tonemapMode = 2u;
    else                                   env.tonemapMode = 0u;
    env.iblIntensity   = s.iblIntensity;
    env.bloomEnabled   = s.bloomEnabled;
    env.bloomThreshold = s.bloomThreshold;
    env.bloomIntensity = s.bloomIntensity;
    env.bloomRadius    = s.bloomRadius;
    env.ssaoEnabled    = s.ssaoEnabled;
    env.ssaoRadius     = s.ssaoRadius;
    env.ssaoIntensity  = s.ssaoIntensity;
    env.colorGradingEnabled   = s.colorGradingEnabled;
    env.colorGradingLutPath   = s.colorGradingLutPath;
    env.colorGradingIntensity = s.colorGradingIntensity;
    env.csmCascadeCount = s.csmCascadeCount;
    env.csmSplitLambda  = s.csmSplitLambda;
    env.ssrEnabled    = s.ssrEnabled;
    env.ssrMaxSteps   = s.ssrMaxSteps;
    env.ssrThickness  = s.ssrThickness;
    env.ssrStepSize   = s.ssrStepSize;
    env.ssrIntensity  = s.ssrIntensity;
}

void applyAudio(const SavedAudio& s, Entity& e, AssetManager& assets) {
    if (!e.hasComponent<AudioSourceComponent>()) {
        e.addComponent<AudioSourceComponent>();
    }
    auto& ac = e.getComponent<AudioSourceComponent>();
    ac.clip        = s.clipPath.empty()
        ? assets.missingAudioId()
        : assets.loadAudio(s.clipPath);
    ac.volume      = s.volume;
    ac.loop        = s.loop;
    ac.playOnStart = s.playOnStart;
    ac.is3D        = s.is3D;
}

void applyCamera(const SavedCamera& s, Entity& e) {
    if (!e.hasComponent<CameraComponent>()) {
        e.addComponent<CameraComponent>();
    }
    auto& cc = e.getComponent<CameraComponent>();
    cc.fovDeg    = s.fovDeg;
    cc.nearPlane = s.nearPlane;
    cc.farPlane  = s.farPlane;
}

void applyParticleEmitter(const SavedParticleEmitter& s, Entity& e,
                            AssetManager& assets) {
    if (!e.hasComponent<ParticleEmitterComponent>()) {
        e.addComponent<ParticleEmitterComponent>();
    }
    auto& em = e.getComponent<ParticleEmitterComponent>();
    em.emitRate      = s.emitRate;
    em.lifetimeMin   = s.lifetimeMin;
    em.lifetimeMax   = s.lifetimeMax;
    em.velocityMin   = s.velocityMin;
    em.velocityMax   = s.velocityMax;
    em.sizeStart     = s.sizeStart;
    em.sizeEnd       = s.sizeEnd;
    em.colorStart    = s.colorStart;
    em.colorEnd      = s.colorEnd;
    em.gravityFactor = s.gravityFactor;
    em.maxParticles  = s.maxParticles;
    em.emitting      = s.emitting;
    em.additive      = s.additive;
    em.localSpace    = s.localSpace;
    using ES = ParticleEmitterComponent::EmissionShape;
    if      (s.emissionShape == "box")    em.emissionShape = ES::Box;
    else if (s.emissionShape == "sphere") em.emissionShape = ES::Sphere;
    else if (s.emissionShape == "disc")   em.emissionShape = ES::Disc;
    else if (s.emissionShape == "cone")   em.emissionShape = ES::Cone;
    else                                   em.emissionShape = ES::Point;
    em.emissionShapeSize = s.emissionShapeSize;
    em.emissionConeAxis  = s.emissionConeAxis;
    em.texture = s.texturePath.empty()
        ? assets.missingTextureId()
        : assets.loadTexture(s.texturePath);
}

} // namespace

bool isSupported(const std::string& componentKey) {
    return supportedKeys().count(componentKey) > 0;
}

std::string componentNameKey(const std::string& componentKey) {
    if (componentKey == kKeyLight)           return "component.name.light";
    if (componentKey == kKeyTrigger)         return "component.name.trigger";
    if (componentKey == kKeyForceField)      return "component.name.force_field";
    if (componentKey == kKeyParticleEmitter) return "component.name.particle_emitter";
    // F3H10:
    if (componentKey == kKeyMeshRenderer)    return "component.name.mesh_renderer";
    if (componentKey == kKeyDialog)          return "component.name.dialog";
    if (componentKey == kKeyItemPickup)      return "component.name.item_pickup";
    if (componentKey == kKeyVehicle)         return "component.name.vehicle";
    if (componentKey == kKeyEnvironment)     return "component.name.environment";
    // F3H11:
    if (componentKey == kKeyAudioSource)     return "component.name.audio_source";
    if (componentKey == kKeyCamera)          return "component.name.camera";
    if (componentKey == kKeyBrush)           return "component.name.brush";
    return {};
}

bool entityHasComponent(const std::string& componentKey, const Entity& e) {
    if (!static_cast<bool>(e)) return false;
    if (componentKey == kKeyLight)           return e.hasComponent<LightComponent>();
    if (componentKey == kKeyTrigger)         return e.hasComponent<TriggerComponent>();
    if (componentKey == kKeyForceField)      return e.hasComponent<ForceFieldComponent>();
    if (componentKey == kKeyParticleEmitter) return e.hasComponent<ParticleEmitterComponent>();
    // F3H10:
    if (componentKey == kKeyMeshRenderer)    return e.hasComponent<MeshRendererComponent>();
    if (componentKey == kKeyDialog)          return e.hasComponent<DialogComponent>();
    if (componentKey == kKeyItemPickup)      return e.hasComponent<ItemPickupComponent>();
    if (componentKey == kKeyVehicle)         return e.hasComponent<VehicleComponent>();
    if (componentKey == kKeyEnvironment)     return e.hasComponent<EnvironmentComponent>();
    // F3H11:
    if (componentKey == kKeyAudioSource)     return e.hasComponent<AudioSourceComponent>();
    if (componentKey == kKeyCamera)          return e.hasComponent<CameraComponent>();
    if (componentKey == kKeyBrush)           return e.hasComponent<BrushComponent>();
    return false;
}

nlohmann::json serializeComponent(const std::string& componentKey,
                                    Entity entity,
                                    const AssetManager& assets) {
    if (!static_cast<bool>(entity)) return nullptr;
    if (!entityHasComponent(componentKey, entity)) return nullptr;
    // F3H11: Brush vive en un schema separado (SavedBrush, no SavedEntity).
    // Llamamos directamente al serializer publico — `serializeEntityToJson`
    // no escribe BrushComponent.
    if (componentKey == kKeyBrush) {
        return serializeBrush(entity, assets);
    }
    // Reusa serializeEntityToJson (que escribe TODO el JSON entity-level)
    // y extrae solo el sub-object del componentKey indicado. Mas trabajo
    // que necesario pero cero codigo nuevo y se mantiene en sync con
    // cualquier cambio futuro al schema.
    const auto full = serializeEntityToJson(entity, assets);
    if (!full.contains(componentKey)) return nullptr;
    return full.at(componentKey);
}

bool applyPayload(const std::string& componentKey,
                   const nlohmann::json& payload,
                   Entity entity, AssetManager& assets) {
    if (!isSupported(componentKey)) return false;
    if (!static_cast<bool>(entity)) return false;
    if (payload.is_null()) return false;

    // F3H11: caso especial — Brush usa SavedBrush directo (no SavedEntity
    // wrapper). El applier reusa el helper publico del SceneLoader.
    // Caveat de material indices: si la entity destino YA tenia un
    // BrushComponent con materialPaths distintos, los face.materialIndex
    // del source pueden quedar fuera de rango. `applyBrushFromSaved`
    // sobrescribe `bc.materials` con los del source — no hay clamp
    // explicito porque ambos arrays vienen del mismo SavedBrush
    // (los indices son consistentes consigo mismos).
    if (componentKey == kKeyBrush) {
        SavedBrush sb;
        try {
            sb = parseBrush(payload);
        } catch (...) { return false; }
        if (sb.faces.empty()) return false;
        SceneLoader::applyBrushFromSaved(sb, entity, assets,
                                           /*applyVisGroupMembership=*/false);
        return true;
    }

    const SavedEntity se = parsePayloadAsSavedEntity(componentKey, payload);

    if (componentKey == kKeyLight) {
        if (!se.light.has_value()) return false;
        applyLight(*se.light, entity);
        return true;
    }
    if (componentKey == kKeyTrigger) {
        if (!se.trigger.has_value()) return false;
        applyTrigger(*se.trigger, entity);
        return true;
    }
    if (componentKey == kKeyForceField) {
        if (!se.forceField.has_value()) return false;
        applyForceField(*se.forceField, entity);
        return true;
    }
    if (componentKey == kKeyParticleEmitter) {
        if (!se.particleEmitter.has_value()) return false;
        applyParticleEmitter(*se.particleEmitter, entity, assets);
        return true;
    }
    // F3H10:
    if (componentKey == kKeyMeshRenderer) {
        if (!se.meshRenderer.has_value()) return false;
        applyMeshRenderer(*se.meshRenderer, entity, assets);
        return true;
    }
    if (componentKey == kKeyDialog) {
        if (!se.dialog.has_value()) return false;
        applyDialog(*se.dialog, entity);
        return true;
    }
    if (componentKey == kKeyItemPickup) {
        if (!se.itemPickup.has_value()) return false;
        applyItemPickup(*se.itemPickup, entity);
        return true;
    }
    if (componentKey == kKeyVehicle) {
        if (!se.vehicle.has_value()) return false;
        applyVehicle(*se.vehicle, entity);
        return true;
    }
    if (componentKey == kKeyEnvironment) {
        if (!se.environment.has_value()) return false;
        applyEnvironment(*se.environment, entity);
        return true;
    }
    // F3H11:
    if (componentKey == kKeyAudioSource) {
        if (!se.audio.has_value()) return false;
        applyAudio(*se.audio, entity, assets);
        return true;
    }
    if (componentKey == kKeyCamera) {
        if (!se.camera.has_value()) return false;
        applyCamera(*se.camera, entity);
        return true;
    }
    return false;
}

bool removeComponent(const std::string& componentKey, Entity entity) {
    if (!isSupported(componentKey)) return false;
    if (!static_cast<bool>(entity)) return false;

    if (componentKey == kKeyLight) {
        if (!entity.hasComponent<LightComponent>()) return false;
        entity.removeComponent<LightComponent>();
        return true;
    }
    if (componentKey == kKeyTrigger) {
        if (!entity.hasComponent<TriggerComponent>()) return false;
        entity.removeComponent<TriggerComponent>();
        return true;
    }
    if (componentKey == kKeyForceField) {
        if (!entity.hasComponent<ForceFieldComponent>()) return false;
        entity.removeComponent<ForceFieldComponent>();
        return true;
    }
    if (componentKey == kKeyParticleEmitter) {
        if (!entity.hasComponent<ParticleEmitterComponent>()) return false;
        entity.removeComponent<ParticleEmitterComponent>();
        return true;
    }
    // F3H10:
    if (componentKey == kKeyMeshRenderer) {
        if (!entity.hasComponent<MeshRendererComponent>()) return false;
        entity.removeComponent<MeshRendererComponent>();
        return true;
    }
    if (componentKey == kKeyDialog) {
        if (!entity.hasComponent<DialogComponent>()) return false;
        entity.removeComponent<DialogComponent>();
        return true;
    }
    if (componentKey == kKeyItemPickup) {
        if (!entity.hasComponent<ItemPickupComponent>()) return false;
        entity.removeComponent<ItemPickupComponent>();
        return true;
    }
    if (componentKey == kKeyVehicle) {
        if (!entity.hasComponent<VehicleComponent>()) return false;
        entity.removeComponent<VehicleComponent>();
        return true;
    }
    if (componentKey == kKeyEnvironment) {
        if (!entity.hasComponent<EnvironmentComponent>()) return false;
        entity.removeComponent<EnvironmentComponent>();
        return true;
    }
    // F3H11:
    if (componentKey == kKeyAudioSource) {
        if (!entity.hasComponent<AudioSourceComponent>()) return false;
        entity.removeComponent<AudioSourceComponent>();
        return true;
    }
    if (componentKey == kKeyCamera) {
        if (!entity.hasComponent<CameraComponent>()) return false;
        entity.removeComponent<CameraComponent>();
        return true;
    }
    if (componentKey == kKeyBrush) {
        if (!entity.hasComponent<BrushComponent>()) return false;
        entity.removeComponent<BrushComponent>();
        return true;
    }
    return false;
}

} // namespace Mood::ComponentClipboard
