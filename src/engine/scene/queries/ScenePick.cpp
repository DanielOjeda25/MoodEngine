#include "engine/scene/queries/ScenePick.h"

#include "core/math/AABB.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/VisGroup.h"  // F2H33: hide gate
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

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

} // namespace (anonymous)

/// @brief Construye el AABB world-space del MeshRenderer. Si hay
///        AssetManager + MeshAsset valido, usa el AABB del bind pose
///        local del mesh (`MeshAsset::aabbMin/aabbMax`) y lo proyecta al
///        mundo via los 8 corners (soporta rotaciones del Transform).
///        Sin assets -> fallback al cubo unitario (legacy / tests).
///        F2H31 Bloque B: expuesto al header para reuso por marquee select.
AABB meshAabbWorld(const TransformComponent& t,
                    const MeshRendererComponent* mr,
                    const AssetManager* assets) {
    glm::vec3 localMin(-0.5f), localMax(0.5f);
    if (assets != nullptr && mr != nullptr) {
        // getMesh devuelve siempre un mesh valido (slot 0 = cubo) o null.
        if (const MeshAsset* asset = const_cast<AssetManager*>(assets)->getMesh(mr->mesh)) {
            // Si el AABB del asset esta inicializado (min <= max en los 3 ejes)
            // lo usamos. Sino dejamos el cubo unitario default.
            if (asset->aabbMin.x <= asset->aabbMax.x &&
                asset->aabbMin.y <= asset->aabbMax.y &&
                asset->aabbMin.z <= asset->aabbMax.z) {
                localMin = asset->aabbMin;
                localMax = asset->aabbMax;
            }
        }
    }

    // Proyectar 8 esquinas a world-space y reconstruir AABB axis-aligned.
    // Cubre rotaciones; con scale uniforme da un AABB un poco holgado
    // respecto al OBB exacto, suficiente para picking.
    const glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    };
    const glm::mat4 model = t.worldMatrix();
    glm::vec3 wMin( std::numeric_limits<f32>::max());
    glm::vec3 wMax(-std::numeric_limits<f32>::max());
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 w = model * glm::vec4(corners[i], 1.0f);
        wMin = glm::min(wMin, glm::vec3(w));
        wMax = glm::max(wMax, glm::vec3(w));
    }
    return AABB{wMin, wMax};
}

/// @brief F2H11: AABB world-space de un brush. Toma `bc.brush.localAabb`
///        (computado por makeBoxBrush / load) y lo proyecta via los 8
///        corners — mismo flow que meshAabbWorld pero sin MeshAsset.
AABB brushAabbWorld(const TransformComponent& t, const BrushComponent& bc) {
    const glm::vec3 localMin = bc.brush.localAabb.min;
    const glm::vec3 localMax = bc.brush.localAabb.max;
    const glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    };
    const glm::mat4 model = t.worldMatrix();
    glm::vec3 wMin( std::numeric_limits<f32>::max());
    glm::vec3 wMax(-std::numeric_limits<f32>::max());
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 w = model * glm::vec4(corners[i], 1.0f);
        wMin = glm::min(wMin, glm::vec3(w));
        wMax = glm::max(wMax, glm::vec3(w));
    }
    return AABB{wMin, wMax};
}

ScenePickResult pickEntityFromRay(Scene& scene,
                                   const glm::vec3& origin,
                                   const glm::vec3& dir,
                                   const AssetManager* assets) {
    ScenePickResult best{};
    f32 bestT = std::numeric_limits<f32>::max();

    scene.forEach<TransformComponent>([&](Entity e, TransformComponent& t) {
        // F2H33: skipear entities en VisGroups hidden — no son pickables
        // (alineado con render). Un brush oculto no responde al click
        // del editor.
        if (isEntityHiddenByVisGroup(scene, e)) return;

        // Targets posibles, en orden de preferencia (mesh gana):
        //   a) MeshRenderer: AABB del mesh.
        //   b) Brush (F2H11): AABB del brush.
        //   c) Light / AudioSource sin mesh: esfera del icono.
        f32 t_hit = -1.0f;
        if (e.hasComponent<MeshRendererComponent>()) {
            const auto& mr = e.getComponent<MeshRendererComponent>();
            t_hit = rayAABB(origin, dir, meshAabbWorld(t, &mr, assets));
        } else if (e.hasComponent<BrushComponent>()) {
            const auto& bc = e.getComponent<BrushComponent>();
            t_hit = rayAABB(origin, dir, brushAabbWorld(t, bc));
        } else if (e.hasComponent<LightComponent>()         ||
                   e.hasComponent<AudioSourceComponent>()   ||
                   e.hasComponent<TriggerComponent>()       ||  // F2H35
                   e.hasComponent<CameraComponent>()        ||  // F2H35
                   e.hasComponent<ParticleEmitterComponent>()) {  // F2H35
            // F2H35: extender pick a todos los point entities. Pre-F2H35
            // solo Light/Audio eran pickables; Trigger/Camera/Particle
            // tenian icono visible (Bloque D) pero no respondian al
            // click — el dev tenia que ir al panel Hierarchy del
            // workspace Layout para seleccionarlos. Ahora todos usan
            // la misma sphere pickable de radio k_iconPickRadius.
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

ScenePickResult pickEntity(Scene& scene,
                            const glm::mat4& view,
                            const glm::mat4& projection,
                            const glm::vec2& ndc,
                            const AssetManager* assets) {
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

    return pickEntityFromRay(scene, origin, dir, assets);
}

} // namespace Mood
