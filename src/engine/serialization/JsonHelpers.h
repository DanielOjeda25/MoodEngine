#pragma once

// Adaptadores de `nlohmann::json` para tipos propios del motor + constantes
// de version de los formatos de archivo. Header-only: incluirlo es suficiente
// para que `json j = someVec3;` funcione.
//
// Estrategia de versionado (ver DECISIONS.md):
//   - Entero monotonico por tipo de archivo (`k_MoodmapFormatVersion`,
//     `k_MoodprojFormatVersion`).
//   - Al cargar: si el archivo declara una version MAYOR a la soportada,
//     fallo explicito (no adivinar). Versiones MENORES se migran en el
//     serializer si es posible, o fallan con mensaje util.
//   - Bump cuando cambia la semantica (campo nuevo opcional no requiere bump).

#include "core/Types.h"
#include "core/math/AABB.h"
#include "engine/world/GridMap.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace Mood {

// --- Versiones de formato ---
// Incrementar SOLO cuando cambia la forma en que se serializa (no al agregar
// campos opcionales con default al leer).

// v2 (Hito 10): agrega seccion `entities` al .moodmap para persistir
// entidades no-tile (modelos importados via drag & drop). Archivos v1 se
// leen sin cambios (entities queda vacio).
// v3 (Hito 11): SavedEntity gana campo opcional `light` ({type, color,
// intensity, direction?, radius?, enabled}). Archivos v2 sin `light` se
// leen igual.
// v4 (Hito 12): SavedEntity gana campo opcional `rigid_body` ({type, shape,
// half_extents, mass}). Archivos v3 sin `rigid_body` se leen igual.
constexpr int k_MoodmapFormatVersion = 4;
constexpr int k_MoodprojFormatVersion = 1;

/// @brief Verifica que la version declarada en un archivo sea legible.
///        Lanza `std::runtime_error` con mensaje util si no.
inline void checkFormatVersion(const nlohmann::json& j, int supportedVersion,
                               const std::string& fileKind) {
    if (!j.contains("version")) {
        throw std::runtime_error(fileKind + ": falta campo 'version'");
    }
    const int v = j.at("version").get<int>();
    if (v > supportedVersion) {
        throw std::runtime_error(
            fileKind + ": version " + std::to_string(v) +
            " no soportada (maxima: " + std::to_string(supportedVersion) + ")");
    }
    // Versiones menores se aceptan; los serializers deciden como migrar.
}

} // namespace Mood

// --- Adaptadores de ADL para nlohmann::json ---
// Usamos la especializacion de `adl_serializer` en vez de colgar `to_json/
// from_json` en `namespace glm`: mas explicito y no contamina glm.

namespace nlohmann {

template<>
struct adl_serializer<glm::vec2> {
    static void to_json(json& j, const glm::vec2& v) {
        j = json::array({v.x, v.y});
    }
    static void from_json(const json& j, glm::vec2& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
    }
};

template<>
struct adl_serializer<glm::vec3> {
    static void to_json(json& j, const glm::vec3& v) {
        j = json::array({v.x, v.y, v.z});
    }
    static void from_json(const json& j, glm::vec3& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }
};

template<>
struct adl_serializer<glm::vec4> {
    static void to_json(json& j, const glm::vec4& v) {
        j = json::array({v.x, v.y, v.z, v.w});
    }
    static void from_json(const json& j, glm::vec4& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
};

template<>
struct adl_serializer<Mood::AABB> {
    static void to_json(json& j, const Mood::AABB& a) {
        j = json{{"min", a.min}, {"max", a.max}};
    }
    static void from_json(const json& j, Mood::AABB& a) {
        a.min = j.at("min").get<glm::vec3>();
        a.max = j.at("max").get<glm::vec3>();
    }
};

} // namespace nlohmann

// TileType como string (robusto a renumeracion del enum). La macro tiene
// que estar en el mismo namespace que el enum para que ADL la encuentre.
namespace Mood {
NLOHMANN_JSON_SERIALIZE_ENUM(TileType, {
    {TileType::Empty,     "empty"},
    {TileType::SolidWall, "solid_wall"},
})
} // namespace Mood
