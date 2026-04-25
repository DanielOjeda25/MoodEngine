#pragma once

// ScenePick (Hito 13 Bloque 1): dispara un rayo desde la camara a traves
// del cursor (NDC) y devuelve la entidad mas cercana intersectada. Cubre
// entidades con MeshRenderer (via AABB world) y entidades sin mesh visible
// (Light, AudioSource) via una esfera chica en su Transform.position.
//
// No depende de Jolt — usa math glm directo. La version "Jolt raycast" del
// mismo concepto vendra cuando `PhysicsWorld::rayCast` exista (Hito 13+).

#include "core/Types.h"
#include "engine/scene/Entity.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace Mood {

class Scene;
class AssetManager;

struct ScenePickResult {
    Entity entity{};               // default-constructed = sin hit
    glm::vec3 worldPoint{0.0f};    // punto del hit
    f32 distance = 0.0f;           // t del rayo (near a lejos)

    explicit operator bool() const { return static_cast<bool>(entity); }
};

/// @brief Dispara un rayo desde la camara a traves de `ndc` y devuelve la
///        entidad mas cercana. Entidades con `MeshRendererComponent` se
///        testean contra un AABB construido desde Transform + cubo unitario
///        (para el cubo primitivo; los meshes importados usan su mesh box
///        simplificada a scale 1). Entidades sin mesh (solo Light o
///        AudioSource) se testean contra una esfera de radio fijo alrededor
///        del Transform.position (iconos pickables, Bloque 2.5).
///
/// @param scene       Scene sobre la que iterar.
/// @param view,proj   Matrices activas de la camara.
/// @param ndc         Coord del cursor en NDC [-1, 1] con Y+ arriba.
/// @return            `ScenePickResult` con entidad valida si hubo hit;
///                    default-constructed (entity falsy) si no.
ScenePickResult pickEntity(Scene& scene,
                            const glm::mat4& view,
                            const glm::mat4& projection,
                            const glm::vec2& ndc);

} // namespace Mood
