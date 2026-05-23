#pragma once

// F2H82 Bloque A: análisis de un modelo de vehículo para detectar ruedas/chasis
// y medir su geometría — la base del importador de vehículos. Sigue la
// convención de `docs/conventions/vehiculos.md`:
//   - Name-first: si los nodos siguen la convención (`wheel_FL/FR/RL/RR`) o un
//     alias conocido (Rockstar `wheel_lf`, Unreal `wheel_BL`, etc.), se confía
//     en el nombre.
//   - Geometry-fallback: si no se identifican 4 ruedas por nombre, se buscan
//     por posición (nodos chicos, Y bajo, en las 4 esquinas) y se clasifican
//     FL/FR/RL/RR por el signo de X/Z del centroide.
//
// El motor NO deriva números físicos (peso/motor/freno) de la malla — eso va
// por presets (Bloque C). Acá solo geometría.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <array>
#include <string>
#include <vector>

namespace Mood::vehicle {

/// Rol de una rueda. El índice numérico coincide con `vehicle::WheelIndex`
/// (FL=0, FR=1, RL=2, RR=3) para mapear directo al `VehicleConfig`.
enum class WheelRole : int { Unknown = -1, FL = 0, FR = 1, RL = 2, RR = 3 };

/// Una parte del modelo (un nodo con geometría) ya en world-space.
struct MeshPart {
    std::string nodeName;
    glm::vec3 center{0.0f};   // centroide del AABB world-space
    glm::vec3 aabbMin{0.0f};
    glm::vec3 aabbMax{0.0f};
    u32 vertexCount = 0;
    glm::vec3 size() const { return aabbMax - aabbMin; }
};

/// Una rueda detectada (parte + rol + radio estimado).
struct DetectedWheel {
    MeshPart part;
    WheelRole role = WheelRole::Unknown;
    f32 radius = 0.0f;      // estimado del AABB de la rueda
    f32 width  = 0.0f;
    bool byName = false;    // detectada por nombre (true) o por geometría (false)
};

/// Resultado del análisis.
struct VehicleAnalysis {
    bool ok = false;
    std::string error;

    // Chasis = unión de todas las partes que NO son rueda.
    glm::vec3 chassisAabbMin{0.0f};
    glm::vec3 chassisAabbMax{0.0f};
    glm::vec3 chassisCenter{0.0f};

    // Modelo completo (chasis + ruedas).
    glm::vec3 overallAabbMin{0.0f};
    glm::vec3 overallAabbMax{0.0f};

    /// Ruedas indexadas por rol (FL=0..RR=3). `role==Unknown` = no encontrada.
    std::array<DetectedWheel, 4> wheels{};
    int wheelsFound = 0;
    bool wheelsByName = false;   // true si las 4 salieron por nombre

    // Derivados geométricos (para precargar el VehicleConfig).
    glm::vec3 chassisHalfExtents{0.0f};
    glm::vec3 centerOfMassLocal{0.0f};
    f32 trackFront = 0.0f;   // distancia X entre FL y FR
    f32 trackRear  = 0.0f;   // distancia X entre RL y RR
    f32 wheelbase  = 0.0f;   // distancia Z entre eje delantero y trasero
    f32 wheelRadius = 0.0f;  // promedio de las ruedas detectadas
    f32 wheelWidth  = 0.0f;
    f32 suggestedYawOffsetDeg = 0.0f;

    bool allFourWheels() const { return wheelsFound == 4; }
};

/// Normaliza el nombre de un nodo a un `WheelRole` por la tabla de alias de la
/// convención. Devuelve `Unknown` si no parece una rueda. PURO (sin assimp) —
/// testeable headless.
WheelRole wheelRoleFromName(const std::string& nodeName);

/// Clasifica una rueda por la posición de su centroide relativo al centro del
/// modelo: X<0 = izquierda, Z>0 = frente (convención +Z forward). PURO.
WheelRole wheelRoleFromPosition(const glm::vec3& wheelCenter,
                                const glm::vec3& modelCenter);

/// Analiza un modelo (.glb/.fbx). Abre el archivo con assimp, recorre los nodos
/// acumulando transforms, detecta ruedas (name-first → geometry-fallback) y
/// mide la geometría. `error` poblado si falla.
VehicleAnalysis analyzeVehicleMesh(const std::string& filesystemPath);

} // namespace Mood::vehicle
