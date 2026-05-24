#include "editor/components/ComponentClipboard.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/serialization/EntitySerializer.h"

#include <unordered_set>

namespace Mood::ComponentClipboard {

namespace {

const std::unordered_set<std::string>& supportedKeys() {
    static const std::unordered_set<std::string> k = {
        kKeyLight, kKeyTrigger, kKeyForceField, kKeyParticleEmitter,
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
    return {};
}

bool entityHasComponent(const std::string& componentKey, const Entity& e) {
    if (!static_cast<bool>(e)) return false;
    if (componentKey == kKeyLight)           return e.hasComponent<LightComponent>();
    if (componentKey == kKeyTrigger)         return e.hasComponent<TriggerComponent>();
    if (componentKey == kKeyForceField)      return e.hasComponent<ForceFieldComponent>();
    if (componentKey == kKeyParticleEmitter) return e.hasComponent<ParticleEmitterComponent>();
    return false;
}

nlohmann::json serializeComponent(const std::string& componentKey,
                                    Entity entity,
                                    const AssetManager& assets) {
    if (!static_cast<bool>(entity)) return nullptr;
    if (!entityHasComponent(componentKey, entity)) return nullptr;
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
    return false;
}

} // namespace Mood::ComponentClipboard
