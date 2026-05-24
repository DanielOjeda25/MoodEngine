#include "engine/scene/entity_type/EntityTypeTable.h"

#include "engine/scene/components/BrushComponent.h"  // no esta en Components.h
#include "engine/scene/components/Components.h"

#include <algorithm>

namespace Mood::EntityTypeTable {

namespace {

// Sentinel: helper para detectar tags auto-generados por GridMap.
// El nombre del tag empieza con "Tile_" o es exactamente "Floor".
bool isAutoGenTileTag(const std::string& tag) {
    if (tag == "Floor") return true;
    return tag.rfind("Tile_", 0) == 0;
}

} // namespace

std::string toString(EntityType t) {
    switch (t) {
        case EntityType::Generic:         return "generic";
        case EntityType::Light:           return "light";
        case EntityType::Camera:          return "camera";
        case EntityType::Audio:           return "audio";
        case EntityType::Trigger:         return "trigger";
        case EntityType::ForceField:      return "force_field";
        case EntityType::ParticleEmitter: return "particle_emitter";
        case EntityType::Environment:     return "environment";
        case EntityType::Npc:             return "npc";
        case EntityType::Pickable:        return "pickable";
        case EntityType::Brush:           return "brush";
        case EntityType::Mesh:            return "mesh";
        case EntityType::Vehicle:         return "vehicle";
        case EntityType::Tile:            return "tile";
    }
    return {};
}

EntityType fromString(const std::string& s) {
    if (s == "light")            return EntityType::Light;
    if (s == "camera")           return EntityType::Camera;
    if (s == "audio")            return EntityType::Audio;
    if (s == "trigger")          return EntityType::Trigger;
    if (s == "force_field")      return EntityType::ForceField;
    if (s == "particle_emitter") return EntityType::ParticleEmitter;
    if (s == "environment")      return EntityType::Environment;
    if (s == "npc")              return EntityType::Npc;
    if (s == "pickable")         return EntityType::Pickable;
    if (s == "brush")            return EntityType::Brush;
    if (s == "mesh")             return EntityType::Mesh;
    if (s == "vehicle")          return EntityType::Vehicle;
    if (s == "tile")             return EntityType::Tile;
    return EntityType::Generic;
}

std::string i18nKey(EntityType t) {
    return std::string("entity_type.") + toString(t);
}

std::vector<std::string> baseComponentKeys(EntityType t) {
    switch (t) {
        case EntityType::Light:           return {"light"};
        case EntityType::Camera:          return {"camera"};
        case EntityType::Audio:           return {"audio_source"};
        case EntityType::Trigger:         return {"trigger"};
        case EntityType::ForceField:      return {"force_field"};
        case EntityType::ParticleEmitter: return {"particle_emitter"};
        case EntityType::Environment:     return {"environment"};
        case EntityType::Npc:             return {"trigger", "dialog"};
        case EntityType::Pickable:        return {"trigger", "item_pickup"};
        case EntityType::Brush:           return {"brush"};
        case EntityType::Mesh:            return {"mesh_renderer"};
        case EntityType::Vehicle:         return {"vehicle"};
        case EntityType::Tile:            return {"mesh_renderer", "rigid_body"};
        case EntityType::Generic:         return {};
    }
    return {};
}

bool isBaseComponent(EntityType t, const std::string& componentKey) {
    const auto bases = baseComponentKeys(t);
    return std::find(bases.begin(), bases.end(), componentKey) != bases.end();
}

// Whitelist de extensions permitidas por type. Listas conservadoras
// alineadas con Hammer (entity classes con allowed keyvalues + add
// components) y Blender (Object Type filtra Properties tabs visibles).
// Generic acepta todo (sin filtro). Tile acepta nada (read-only).
namespace {
const std::vector<std::string>& allowedExtensionsFor(EntityType t) {
    static const std::vector<std::string> kEmpty;
    static const std::vector<std::string> kLight = {
        "script", "rigid_body", "audio_source"};
    static const std::vector<std::string> kCamera = {
        "script", "rigid_body"};
    static const std::vector<std::string> kAudio = {
        "script", "rigid_body", "mesh_renderer"};
    static const std::vector<std::string> kTrigger = {
        "script", "dialog", "item_pickup", "audio_source"};
    static const std::vector<std::string> kForceField = {
        "script"};
    static const std::vector<std::string> kParticleEmitter = {
        "script", "audio_source"};
    static const std::vector<std::string> kEnvironment;
    static const std::vector<std::string> kNpc = {
        "script", "inventory", "animator", "mesh_renderer",
        "rigid_body", "audio_source"};
    static const std::vector<std::string> kPickable = {
        "script", "mesh_renderer", "audio_source"};
    static const std::vector<std::string> kBrush = {
        "rigid_body", "script", "trigger", "audio_source"};
    static const std::vector<std::string> kMesh = {
        "rigid_body", "joint", "animator", "script",
        "particle_emitter", "audio_source", "trigger",
        "force_field", "light", "dialog", "inventory",
        "item_pickup", "ragdoll", "cloth"};
    // Vehicle: el config del .moodvehicle define todo (mesh, fisica de
    // ruedas, steering). Extensiones razonables: script para gameplay,
    // audio para motor/colision, trigger para zonas, animator si el
    // mesh tiene rigging (puertas/luces), mesh_renderer si el spawn no
    // lo agrego (config sin mesh). NO acepta light/environment/dialog/
    // inventory — un auto no es una luz ni un NPC.
    static const std::vector<std::string> kVehicle = {
        "script", "audio_source", "trigger", "animator",
        "mesh_renderer", "rigid_body"};

    switch (t) {
        case EntityType::Light:           return kLight;
        case EntityType::Camera:          return kCamera;
        case EntityType::Audio:           return kAudio;
        case EntityType::Trigger:         return kTrigger;
        case EntityType::ForceField:      return kForceField;
        case EntityType::ParticleEmitter: return kParticleEmitter;
        case EntityType::Environment:     return kEnvironment;
        case EntityType::Npc:             return kNpc;
        case EntityType::Pickable:        return kPickable;
        case EntityType::Brush:           return kBrush;
        case EntityType::Mesh:            return kMesh;
        case EntityType::Vehicle:         return kVehicle;
        case EntityType::Tile:            return kEmpty;
        case EntityType::Generic:         return kEmpty;  // ver canAddComponent abajo
    }
    return kEmpty;
}
} // namespace

bool canAddComponent(EntityType t, const std::string& componentKey) {
    if (componentKey.empty()) return false;
    // Generic: sin filtro — el dev sabe lo que hace en una entity sin type.
    if (t == EntityType::Generic) return true;
    // Tile: read-only.
    if (t == EntityType::Tile) return false;
    // Bases ya vienen con el spawn; no se ofrecen en "Add".
    if (isBaseComponent(t, componentKey)) return false;
    // Resto: consultar whitelist.
    const auto& allowed = allowedExtensionsFor(t);
    return std::find(allowed.begin(), allowed.end(), componentKey)
        != allowed.end();
}

EntityType inferFromEntity(Entity e) {
    if (!static_cast<bool>(e)) return EntityType::Generic;

    // Check Tile primero por tag (auto-gen). El tag suele estar siempre.
    if (e.hasComponent<TagComponent>()) {
        if (isAutoGenTileTag(e.getComponent<TagComponent>().name)) {
            return EntityType::Tile;
        }
    }

    // Orden: del mas especifico al menos. NPC y Pickable requieren 2
    // bases ambos presentes; deben chequearse antes que Trigger solo.
    if (e.hasComponent<BrushComponent>()) return EntityType::Brush;

    const bool hasTrigger = e.hasComponent<TriggerComponent>();
    if (hasTrigger && e.hasComponent<DialogComponent>())
        return EntityType::Npc;
    if (hasTrigger && e.hasComponent<ItemPickupComponent>())
        return EntityType::Pickable;

    if (e.hasComponent<EnvironmentComponent>()) return EntityType::Environment;
    if (e.hasComponent<LightComponent>())       return EntityType::Light;
    if (e.hasComponent<CameraComponent>())      return EntityType::Camera;
    if (e.hasComponent<ParticleEmitterComponent>()) return EntityType::ParticleEmitter;
    if (e.hasComponent<ForceFieldComponent>())  return EntityType::ForceField;
    if (e.hasComponent<AudioSourceComponent>()) return EntityType::Audio;
    if (hasTrigger)                              return EntityType::Trigger;
    // Vehicle antes que Mesh: un vehicle suele tener MeshRendererComponent
    // tambien (visual del chasis) y caeria como Mesh si chequeasemos al
    // reves. VehicleComponent es la mecanica definitoria.
    if (e.hasComponent<VehicleComponent>())      return EntityType::Vehicle;
    if (e.hasComponent<MeshRendererComponent>()) return EntityType::Mesh;

    return EntityType::Generic;
}

EntityType inferFromEntityWithTag(Entity e, const std::string& tagHint) {
    // Si el caller pasa un tagHint distinto del TagComponent (caso edge
    // del SceneLoader donde el TagComponent aun no esta materializado),
    // chequear Tile primero con el hint.
    if (isAutoGenTileTag(tagHint)) return EntityType::Tile;
    return inferFromEntity(e);
}

} // namespace Mood::EntityTypeTable
