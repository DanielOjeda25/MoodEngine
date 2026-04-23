#pragma once

// Axis-Aligned Bounding Box. Primitiva de colision y debug draw del motor
// previa a integrar Jolt (Hito 12). Header-only: las operaciones son triviales
// y nos interesa que inlineen libremente.
//
// Los tests unitarios viven en tests/test_aabb.cpp (Hito 4 Bloque 3).

#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace Mood {

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    /// @brief Centro geometrico de la caja.
    glm::vec3 center() const { return 0.5f * (min + max); }

    /// @brief Dimensiones por eje (size.x = ancho, .y = alto, .z = profundo).
    glm::vec3 size() const { return max - min; }

    /// @brief Semi-extensiones: mitad de size(). Comodo para SAT / proyeccion.
    glm::vec3 extents() const { return 0.5f * size(); }

    /// @brief True si la caja tiene volumen estricto (min < max en los 3 ejes).
    bool isValid() const {
        return min.x < max.x && min.y < max.y && min.z < max.z;
    }
};

/// @brief Devuelve true si los volumenes de a y b se tocan o se solapan.
///        Contacto borde-con-borde cuenta como interseccion.
inline bool intersects(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x
        && a.min.y <= b.max.y && a.max.y >= b.min.y
        && a.min.z <= b.max.z && a.max.z >= b.min.z;
}

/// @brief Devuelve true si el punto p cae dentro o en el borde de a.
inline bool contains(const AABB& a, const glm::vec3& p) {
    return p.x >= a.min.x && p.x <= a.max.x
        && p.y >= a.min.y && p.y <= a.max.y
        && p.z >= a.min.z && p.z <= a.max.z;
}

/// @brief Devuelve una copia de a expandida simetricamente en cada eje por
///        `margin`. Util para tests de colision con tolerancia.
inline AABB expanded(const AABB& a, float margin) {
    return AABB{
        a.min - glm::vec3(margin),
        a.max + glm::vec3(margin),
    };
}

/// @brief AABB minima que contiene a ambos operandos.
inline AABB merge(const AABB& a, const AABB& b) {
    return AABB{
        glm::min(a.min, b.min),
        glm::max(a.max, b.max),
    };
}

} // namespace Mood
