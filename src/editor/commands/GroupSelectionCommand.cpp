#include "editor/commands/GroupSelectionCommand.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"   // brushAabbWorld + meshAabbWorld

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <limits>

namespace Mood {

namespace {

void decomposeLocal(const glm::mat4& world, const glm::mat4& parentWorldInv,
                     glm::vec3& outPos, glm::vec3& outEuler,
                     glm::vec3& outScale) {
    const glm::mat4 local = parentWorldInv * world;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat rotation;
    if (!glm::decompose(local, outScale, rotation, outPos, skew, perspective)) {
        outPos = glm::vec3(local[3]);
        outScale = glm::vec3(1.0f);
        outEuler = glm::vec3(0.0f);
        return;
    }
    outEuler = glm::degrees(glm::eulerAngles(rotation));
}

/// Computa el centroide del AABB combinado de un set de entities.
/// Brushes => brushAabbWorld; meshes => meshAabbWorld; sin geometría
/// reconocible => pivote (tform.position). Si el set está vacío,
/// devuelve (0,0,0).
glm::vec3 computeCentroidAabb(Scene& scene, AssetManager* assets,
                                const std::vector<entt::entity>& children) {
    if (children.empty()) return glm::vec3(0.0f);
    glm::vec3 mn(std::numeric_limits<f32>::max());
    glm::vec3 mx(-std::numeric_limits<f32>::max());
    bool hasAny = false;
    auto& reg = scene.registry();
    for (entt::entity h : children) {
        if (!reg.valid(h)) continue;
        if (!reg.all_of<TransformComponent>(h)) continue;
        auto& tc = reg.get<TransformComponent>(h);
        if (reg.all_of<BrushComponent>(h)) {
            const auto& bc = reg.get<BrushComponent>(h);
            const AABB box = brushAabbWorld(tc, bc);
            mn = glm::min(mn, box.min);
            mx = glm::max(mx, box.max);
            hasAny = true;
        } else if (reg.all_of<MeshRendererComponent>(h) && assets != nullptr) {
            auto& mr = reg.get<MeshRendererComponent>(h);
            const AABB box = meshAabbWorld(tc, &mr, assets);
            mn = glm::min(mn, box.min);
            mx = glm::max(mx, box.max);
            hasAny = true;
        } else {
            // Sin geometria identificable — usar pivote como punto.
            mn = glm::min(mn, tc.position);
            mx = glm::max(mx, tc.position);
            hasAny = true;
        }
    }
    if (!hasAny) return glm::vec3(0.0f);
    return (mn + mx) * 0.5f;
}

} // namespace

GroupSelectionCommand::GroupSelectionCommand(Scene* scene, AssetManager* assets,
                                                std::vector<entt::entity> children)
    : m_scene(scene), m_assets(assets),
      m_centroid(0.0f), m_emptyHandle(entt::null),
      m_children(std::move(children)) {
    if (m_scene == nullptr) return;
    if (m_children.empty()) return;

    auto& reg = m_scene->registry();
    // Tag único — "Group_<N>" con N = max existing + 1.
    int maxN = 0;
    reg.view<TagComponent>().each([&](TagComponent& t) {
        if (t.name.size() < 6) return;
        if (t.name.compare(0, 6, "Group_") != 0) return;
        int n = 0;
        if (std::sscanf(t.name.c_str() + 6, "%d", &n) == 1) {
            if (n > maxN) maxN = n;
        }
    });
    m_emptyTag = "Group_" + std::to_string(maxN + 1);

    m_centroid = computeCentroidAabb(*m_scene, m_assets, m_children);

    // Snapshot PRE de cada child.
    m_pre.reserve(m_children.size());
    for (entt::entity h : m_children) {
        if (!reg.valid(h)) continue;
        if (!reg.all_of<TransformComponent>(h)) continue;
        auto& tc = reg.get<TransformComponent>(h);
        PreSnapshot s;
        s.handle         = h;
        s.oldParent      = tc.parent;
        s.oldLocalPos    = tc.position;
        s.oldLocalEuler  = tc.rotationEuler;
        s.oldLocalScale  = tc.scale;
        s.oldWorld       = m_scene->worldMatrixOf(h);
        m_pre.push_back(s);
    }
}

void GroupSelectionCommand::execute() {
    if (m_scene == nullptr) return;
    if (m_pre.empty()) return;

    // Crear el Empty padre.
    Entity emptyE = m_scene->createEntity(m_emptyTag);
    m_emptyHandle = emptyE.handle();
    {
        auto& tc = emptyE.getComponent<TransformComponent>();
        tc.position = m_centroid;
        tc.rotationEuler = glm::vec3(0.0f);
        tc.scale = glm::vec3(1.0f);
        tc.parent = entt::null;
    }

    // Para cada child: setear parent = Empty + recompute local que preserva
    // world (parent es identity + position=centroid, asi que inverse es
    // translate(-centroid)).
    const glm::mat4 emptyWorld = m_scene->worldMatrixOf(m_emptyHandle);
    const glm::mat4 emptyWorldInv = glm::inverse(emptyWorld);
    auto& reg = m_scene->registry();
    for (auto& s : m_pre) {
        if (!reg.valid(s.handle)) continue;
        if (!reg.all_of<TransformComponent>(s.handle)) continue;
        auto& tc = reg.get<TransformComponent>(s.handle);
        glm::vec3 newPos, newEuler, newScale;
        decomposeLocal(s.oldWorld, emptyWorldInv, newPos, newEuler, newScale);
        tc.parent = m_emptyHandle;
        tc.position = newPos;
        tc.rotationEuler = newEuler;
        tc.scale = newScale;
        tc.useQuaternion = false;
    }
    Log::editor()->info("[parent] Group: created '{}' (handle={}) with {} children",
                         m_emptyTag, static_cast<u32>(m_emptyHandle),
                         m_pre.size());
}

void GroupSelectionCommand::undo() {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    // Restaurar el local + parent de cada child.
    for (auto& s : m_pre) {
        if (!reg.valid(s.handle)) continue;
        if (!reg.all_of<TransformComponent>(s.handle)) continue;
        auto& tc = reg.get<TransformComponent>(s.handle);
        tc.parent = s.oldParent;
        tc.position = s.oldLocalPos;
        tc.rotationEuler = s.oldLocalEuler;
        tc.scale = s.oldLocalScale;
        tc.useQuaternion = false;
    }
    // Destruir el Empty creado.
    if (m_emptyHandle != entt::null && reg.valid(m_emptyHandle)) {
        Entity e(m_emptyHandle, m_scene);
        m_scene->destroyEntity(e);
        m_emptyHandle = entt::null;
    }
}

void GroupSelectionCommand::onEntityRemap(entt::entity oldH, entt::entity newH) {
    if (m_emptyHandle == oldH) m_emptyHandle = newH;
    for (auto& s : m_pre) {
        if (s.handle == oldH) s.handle = newH;
        if (s.oldParent == oldH) s.oldParent = newH;
    }
    for (auto& c : m_children) {
        if (c == oldH) c = newH;
    }
}

} // namespace Mood
