#include "systems/physics/ClothSystem.h"

#include "engine/physics/cloth/ClothLayout.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Mood::ClothSystem {

namespace {

// Cache de topologia por entity. La grilla (particulas + edges + triangulos)
// solo cambia si el dev edita las dimensiones/resolucion/anclaje; cachearla
// evita reconstruirla cada frame (tick + previewRest). Keyed por handle
// raw de la entity. Crece on-demand; entradas stale (entity borrada) son
// chicas y no molestan.
struct CachedLayout {
    cloth::ClothLayout layout;
    f32 w = 0.0f, h = 0.0f;
    int rx = 0, ry = 0;
    ClothComponent::Anchor anchor = ClothComponent::Anchor::TopEdge;
};

std::unordered_map<u32, CachedLayout>& layoutCache() {
    static std::unordered_map<u32, CachedLayout> s;
    return s;
}

cloth::AnchorEdge toAnchorEdge(ClothComponent::Anchor a) {
    switch (a) {
        case ClothComponent::Anchor::None:       return cloth::AnchorEdge::None;
        case ClothComponent::Anchor::TopEdge:    return cloth::AnchorEdge::TopEdge;
        case ClothComponent::Anchor::TopCorners: return cloth::AnchorEdge::TopCorners;
        case ClothComponent::Anchor::LeftEdge:   return cloth::AnchorEdge::LeftEdge;
    }
    return cloth::AnchorEdge::TopEdge;
}

// Devuelve el layout cacheado de la entity, reconstruyendolo si los params
// geometricos del componente cambiaron.
const cloth::ClothLayout& ensureLayout(u32 handle, const ClothComponent& cl) {
    CachedLayout& c = layoutCache()[handle];
    if (c.layout.empty() || c.w != cl.width || c.h != cl.height
        || c.rx != cl.resX || c.ry != cl.resY || c.anchor != cl.anchor) {
        c.layout = cloth::buildGridCloth(cl.width, cl.height, cl.resX, cl.resY,
                                          toAnchorEdge(cl.anchor));
        c.w = cl.width; c.h = cl.height;
        c.rx = cl.resX; c.ry = cl.resY;
        c.anchor = cl.anchor;
    }
    return c.layout;
}

// stiffness [0,1] -> compliance de Jolt (inverso de la rigidez). 1 = rigido
// (compliance 0); 0 = muy elastico. Escala empirica conservadora.
f32 stiffnessToCompliance(f32 stiffness) {
    constexpr f32 k_maxCompliance = 1e-3f;
    return (1.0f - std::clamp(stiffness, 0.0f, 1.0f)) * k_maxCompliance;
}

// Expande los triangulos del layout en un buffer interleaved (pos.xyz +
// normal.xyz por vertice) con normales suaves (promedio de las caras
// adyacentes). `worldPos` esta indexado 1-a-1 con las particulas del layout.
void buildRenderBuffer(const std::vector<glm::vec3>& worldPos,
                        const std::vector<u32>& indices,
                        std::vector<f32>& out) {
    const usize n = worldPos.size();
    if (n == 0 || indices.size() < 3) {
        out.clear();
        return;
    }

    // 1) Normales por vertice: acumular las normales de cara y normalizar.
    std::vector<glm::vec3> normals(n, glm::vec3(0.0f));
    for (usize t = 0; t + 2 < indices.size(); t += 3) {
        const u32 ia = indices[t];
        const u32 ib = indices[t + 1];
        const u32 ic = indices[t + 2];
        if (ia >= n || ib >= n || ic >= n) continue;
        const glm::vec3 faceN = glm::cross(worldPos[ib] - worldPos[ia],
                                            worldPos[ic] - worldPos[ia]);
        normals[ia] += faceN;
        normals[ib] += faceN;
        normals[ic] += faceN;
    }
    for (auto& nrm : normals) {
        const f32 len2 = glm::dot(nrm, nrm);
        nrm = (len2 > 1e-12f) ? (nrm / std::sqrt(len2))
                              : glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // 2) Expandir triangulos a vertices planos (pos + normal interleaved).
    out.clear();
    out.reserve(indices.size() * 6);
    for (const u32 i : indices) {
        if (i >= n) continue;
        const glm::vec3& p = worldPos[i];
        const glm::vec3& nrm = normals[i];
        out.push_back(p.x);   out.push_back(p.y);   out.push_back(p.z);
        out.push_back(nrm.x); out.push_back(nrm.y); out.push_back(nrm.z);
    }
}

} // namespace

void tick(Scene& scene, PhysicsWorld& physicsWorld) {
    std::vector<glm::vec3> worldPos;  // scratch reusado entre telas

    scene.forEach<ClothComponent, TransformComponent>(
        [&](Entity e, ClothComponent& cl, TransformComponent& tf) {
            const u32 handle = static_cast<u32>(e.handle());
            const cloth::ClothLayout& layout = ensureLayout(handle, cl);

            // Re-materializacion: si el dev edito params (dirty) con la tela
            // ya viva, destruimos para recrear con los nuevos valores.
            if (cl.dirty && cl.clothId != 0) {
                physicsWorld.destroyCloth(cl.clothId);
                cl.clothId = 0;
            }
            // Materializacion lazy.
            if (cl.dirty && cl.clothId == 0) {
                cl.clothId = physicsWorld.createCloth(
                    layout, tf.worldMatrix(), cl.totalMass,
                    stiffnessToCompliance(cl.stiffness), cl.damping,
                    cl.useGravity);
                cl.dirty = false;
            }
            if (cl.clothId == 0) return;

            // Sync: leer posiciones world del soft body -> render buffer.
            if (physicsWorld.readClothVertices(cl.clothId, worldPos)) {
                buildRenderBuffer(worldPos, layout.triangleIndices,
                                   cl.renderVertices);
            }
        });
}

void previewRest(Scene& scene) {
    std::vector<glm::vec3> worldPos;  // scratch

    scene.forEach<ClothComponent, TransformComponent>(
        [&](Entity e, ClothComponent& cl, TransformComponent& tf) {
            const u32 handle = static_cast<u32>(e.handle());
            const cloth::ClothLayout& layout = ensureLayout(handle, cl);
            if (layout.empty()) return;

            // Pose de reposo: posiciones locales del layout llevadas a world
            // por el Transform (sin fisica). Muestra la tela plana en el
            // editor en su orientacion/posicion real.
            const glm::mat4 world = tf.worldMatrix();
            worldPos.resize(layout.particles.size());
            for (usize i = 0; i < layout.particles.size(); ++i) {
                const glm::vec4 w =
                    world * glm::vec4(layout.particles[i].localPos, 1.0f);
                worldPos[i] = glm::vec3(w);
            }
            buildRenderBuffer(worldPos, layout.triangleIndices,
                               cl.renderVertices);
        });
}

} // namespace Mood::ClothSystem
