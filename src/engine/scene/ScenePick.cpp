#include "engine/scene/ScenePick.h"

#include "core/math/AABB.h"
#include "engine/scene/Components.h"
#include "engine/scene/Scene.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Mood {

namespace {

constexpr f32 k_iconPickRadius = 0.6f; // esfera pickable para entidades sin mesh

/// @brief Ray-AABB slab test. Devuelve `t` de entrada si hubo hit, -1 si no.
///        Ray: origin + t * dir (dir no necesariamente unitario).
f32 rayAABB(const glm::vec3& origin, const glm::vec3& dir, const AABB& box) {
    f32 tmin = 0.0f;
    f32 tmax = std::numeric_limits<f32>::max();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-8f) {
            if (origin[i] < box.min[i] || origin[i] > box.max[i]) return -1.0f;
            continue;
        }
        const f32 invD = 1.0f / dir[i];
        f32 t1 = (box.min[i] - origin[i]) * invD;
        f32 t2 = (box.max[i] - origin[i]) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return -1.0f;
    }
    return tmin;
}

/// @brief Ray-Sphere. Devuelve `t` de entrada si hay hit, -1 si no.
f32 raySphere(const glm::vec3& origin, const glm::vec3& dir,
              const glm::vec3& center, f32 radius) {
    const glm::vec3 oc = origin - center;
    const f32 a = glm::dot(dir, dir);
    const f32 b = 2.0f * glm::dot(oc, dir);
    const f32 c = glm::dot(oc, oc) - radius * radius;
    const f32 disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return -1.0f;
    const f32 sq = std::sqrt(disc);
    const f32 t = (-b - sq) / (2.0f * a);
    if (t < 0.0f) {
        // rayo desde adentro de la esfera: devolver el t de salida
        return (-b + sq) / (2.0f * a);
    }
    return t;
}

/// @brief Construye el AABB world-space aproximado del MeshRenderer:
///        asume mesh local en [-0.5, 0.5]^3 (cubo y demas primitivos
///        escalan a ese rango). Meshes importados grandes dan un AABB
///        demasiado chico — pendiente agregar bounding box por MeshAsset.
AABB meshAabbWorld(const TransformComponent& t) {
    // Bounding box locale del cubo unitario escalado por scale.
    const glm::vec3 half = 0.5f * t.scale;
    return AABB{ t.position - half, t.position + half };
}

} // namespace

ScenePickResult pickEntity(Scene& scene,
                            const glm::mat4& view,
                            const glm::mat4& projection,
                            const glm::vec2& ndc) {
    // Construccion del rayo: misma receta que pickTile — unproyectar dos
    // puntos en z=-1 (near) y z=+1 (far) del NDC.
    const glm::mat4 invVP = glm::inverse(projection * view);
    const glm::vec4 nearH = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    const glm::vec4 farH  = invVP * glm::vec4(ndc.x, ndc.y, +1.0f, 1.0f);
    if (nearH.w == 0.0f || farH.w == 0.0f) return {};
    const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
    const glm::vec3 farW  = glm::vec3(farH)  / farH.w;

    const glm::vec3 origin = nearW;
    const glm::vec3 dir = glm::normalize(farW - nearW);

    ScenePickResult best{};
    f32 bestT = std::numeric_limits<f32>::max();

    scene.forEach<TransformComponent>([&](Entity e, TransformComponent& t) {
        // 3 targets posibles, en orden de preferencia (mesh gana):
        //   a) MeshRenderer: AABB del mesh.
        //   b) Light / AudioSource sin mesh: esfera del icono.
        f32 t_hit = -1.0f;
        if (e.hasComponent<MeshRendererComponent>()) {
            t_hit = rayAABB(origin, dir, meshAabbWorld(t));
        } else if (e.hasComponent<LightComponent>() ||
                   e.hasComponent<AudioSourceComponent>()) {
            t_hit = raySphere(origin, dir, t.position, k_iconPickRadius);
        } else {
            return; // entidad sin target pickable
        }
        if (t_hit < 0.0f || t_hit >= bestT) return;
        bestT = t_hit;
        best.entity = e;
        best.distance = t_hit;
        best.worldPoint = origin + dir * t_hit;
    });

    return best;
}

} // namespace Mood
