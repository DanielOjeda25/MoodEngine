#pragma once

// F2H70 schema v2 parser, expuesto para tests. La implementación vive en
// AssetManager_Vehicle.cpp (junto al cache/load del AssetManager). Se declara
// acá para poder round-trip-testear el writer del importador (F2H82 Bloque C):
//   analysis + preset → buildVehicleConfigJson → parseVehicleConfigJson → cfg.

#include "engine/physics/vehicle/VehicleConfig.h"

#include <nlohmann/json_fwd.hpp>

namespace Mood {

/// Parsea un .moodvehicle ya cargado en JSON a `VehicleConfig` (dispatcher por
/// `schemaVersion`). NO toca disco ni cache.
vehicle::VehicleConfig parseVehicleConfigJson(const nlohmann::json& j);

} // namespace Mood
