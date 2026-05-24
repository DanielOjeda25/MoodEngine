#pragma once

// EntityType (F3H9): tipo fijo de una entidad. Modelo Blender / Hammer /
// Unreal — cada entidad tiene UN type que define cual es su componente
// base (no quitable) y limita que extensions tiene sentido agregar.
//
// Storage: campo `entityType` en TagComponent (co-localizado con el
// nombre — ambos son metadata de identidad de la entidad). NO es un
// componente separado para evitar proliferacion.
//
// Persistencia: explicita en `.moodmap` como `"entity_type": "light"`.
// Pre-F3H9 maps no tienen la key — al cargar se infiere de los
// componentes presentes (ver EntityTypeTable::inferFromComponents).
//
// Convencion (alineada con Blender Object Type + Hammer entity class):
//   - Type fijo al spawnear. Cambiarlo requiere el "convert_entity_modal"
//     (Stage 8) que destruye base components viejos y agrega los nuevos.
//   - El base component NO se puede quitar via Inspector. Para "quitar"
//     la luz, borras la entity entera (la entity ES la luz).
//   - Add Component popup filtra por que extensions son validas para el
//     type (Light NO puede agregar Environment, NPC SI puede agregar
//     Inventory, etc).

#include "core/Types.h"

namespace Mood {

/// Type-tag para entidades. Orden alineado con la convencion de spawn
/// del editor ("+ Crear Entidad" / Convert kits).
enum class EntityType : u8 {
    // Sin tipo asignado. Default para entidades pre-F3H9 que no
    // matchean ningun infer + para tools que crean entidades raw.
    Generic = 0,

    // Tipos primarios (1 base component cada uno).
    Light,
    Camera,
    Audio,
    Trigger,
    ForceField,
    ParticleEmitter,
    Environment,

    // Tipos compuestos (2 bases cada uno — la modal convert los aplica).
    Npc,       // Trigger + Dialog
    Pickable,  // Trigger + ItemPickup

    // Tipos geometricos.
    Brush,     // BrushComponent (CSG)
    Mesh,      // MeshRendererComponent (modelo 3D)

    // Mecanica especifica del engine — el config define mesh + fisica
    // de ruedas + steering. Spawn via drop de .moodvehicle del Asset
    // Browser. Whitelist restringida (no acepta Light/Environment/etc).
    Vehicle,   // VehicleComponent

    // Auto-generado por GridMap (Floor / Tile_X_Y). El Inspector lo
    // muestra read-only — no es responsabilidad del dev editarlo.
    Tile,
};

} // namespace Mood
