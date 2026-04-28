#pragma once

// Sistema que mueve entidades con NavAgentComponent hacia su `target`
// world-space siguiendo paths A* sobre el GridMap (Hito 23 Bloque 2).
//
// Por frame:
//   1. Por cada NavAgent activo: si el target se movio > tileSize
//      desde el ultimo path o el throttle (0.5s) vencio, recomputa.
//   2. Si tiene path, avanza hacia el proximo waypoint a `speed` m/s
//      en plano XZ. Y queda fijo (no salta paredes).
//   3. moveAndSlide contra el grid igual que el player — agente no
//      atraviesa muros.
//   4. Cuando llega al ultimo waypoint, queda quieto.
//
// El caller (EditorApplication / PlayerApplication) actualiza el
// `target` de los agentes con la posicion del FpsCamera antes de
// invocar `update`. Tambien lo puede setear un script Lua si en el
// futuro se exponen los componentes a sol2.

#include "core/Types.h"

#include <glm/vec3.hpp>

namespace Mood {

class GridMap;
class Scene;

class NavSystem {
public:
    /// @brief Mueve los NavAgents activos en `scene` segun A*. El AABB
    ///        de colision usa el `agentHalfExtents` (default = mismo
    ///        que el player: 0.3 x 0.9 x 0.3 m).
    void update(Scene& scene,
                const GridMap& map,
                const glm::vec3& mapWorldOrigin,
                f32 dt);

    /// @brief AABB half-extents de los NavAgents para `moveAndSlide`.
    ///        Lo expone para que tests puedan ajustarlo.
    void setAgentHalfExtents(const glm::vec3& he) { m_halfExtents = he; }
    const glm::vec3& agentHalfExtents() const { return m_halfExtents; }

private:
    glm::vec3 m_halfExtents{0.3f, 0.9f, 0.3f};

    static constexpr f32 k_repathInterval = 0.5f; // s entre repaths
    static constexpr f32 k_repathTargetEpsilon = 1.0f; // m: si target se movio mas, repath ya
};

} // namespace Mood
