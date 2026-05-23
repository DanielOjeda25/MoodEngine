#pragma once

// F2H82 Bloque C: escribe un .moodvehicle (schema v2) combinando la GEOMETRÍA
// detectada (VehicleAnalysis, Bloque A) con el FEEL físico de un preset
// (VehiclePhysicsPreset, presets.h). Es el paso "guardar" del importador.
//
// La geometría sale del modelo; el tuning sale del preset (editable por el dev
// en el modal). El bloque `mesh_wheels` guarda el nombre real de cada sub-mesh
// de rueda + su centroide (hub) para el centrado en runtime del Bloque B.

#include "engine/physics/vehicle/VehicleMeshAnalyzer.h"
#include "engine/physics/vehicle/VehiclePresets.h"

#include <string>

namespace Mood::vehicle {

/// Metadatos que el dev provee (no salen del modelo).
struct VehicleImportMeta {
    std::string displayName;   ///< Nombre legible (tag/browser).
    std::string meshPath;      ///< Path lógico relativo a assets/ del .glb.
    /// F2H82 polish: multiplicador para el mesh visual cuando el GLB esta en
    /// unidades no-metros (cm/mm, tipico Sketchfab). Se guarda como `mesh_scale`
    /// en el JSON y se aplica a `TransformComponent.scale` al spawnear. Los
    /// valores fisicos del JSON (dimensions_mm, attach_y, radius...) van ya en
    /// metros reales — quien los escala es el caller (el modal de import).
    f32 meshScale = 1.0f;
};

/// Construye el texto JSON del .moodvehicle (schema v2) desde análisis+preset.
/// PURO (no toca disco) → testeable headless. `pretty` indenta para lectura.
std::string buildVehicleConfigJson(const VehicleAnalysis& analysis,
                                   const VehiclePhysicsPreset& preset,
                                   const VehicleImportMeta& meta,
                                   bool pretty = true);

/// Escribe el .moodvehicle a `outFsPath`. Devuelve false y puebla `err` si
/// falla la escritura. Crea los directorios padre si no existen.
bool writeVehicleConfigFile(const VehicleAnalysis& analysis,
                            const VehiclePhysicsPreset& preset,
                            const VehicleImportMeta& meta,
                            const std::string& outFsPath,
                            std::string& err);

} // namespace Mood::vehicle
