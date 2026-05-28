#include "editor/commands/UngroupSelectionCommand.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/vec4.hpp>

namespace Mood {

namespace {

void decomposeWorld(const glm::mat4& world,
                     glm::vec3& outPos, glm::vec3& outEuler,
                     glm::vec3& outScale) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat rotation;
    if (!glm::decompose(world, outScale, rotation, outPos, skew, perspective)) {
        outPos = glm::vec3(world[3]);
        outScale = glm::vec3(1.0f);
        outEuler = glm::vec3(0.0f);
        return;
    }
    outEuler = glm::degrees(glm::eulerAngles(rotation));
}

} // namespace

UngroupSelectionCommand::UngroupSelectionCommand(
    Scene* scene, std::vector<entt::entity> children)
    : m_scene(scene) {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    m_pre.reserve(children.size());
    for (entt::entity h : children) {
        if (!reg.valid(h)) continue;
        if (!reg.all_of<TransformComponent>(h)) continue;
        auto& tc = reg.get<TransformComponent>(h);
        if (tc.parent == entt::null) continue;  // ya es root
        PreSnapshot s;
        s.handle         = h;
        s.oldParent      = tc.parent;
        s.oldLocalPos    = tc.position;
        s.oldLocalEuler  = tc.rotationEuler;
        s.oldLocalScale  = tc.scale;
        const glm::mat4 world = m_scene->worldMatrixOf(h);
        decomposeWorld(world, s.newWorldPos, s.newWorldEuler, s.newWorldScale);
        m_pre.push_back(s);
    }
}

void UngroupSelectionCommand::execute() {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    for (auto& s : m_pre) {
        if (!reg.valid(s.handle)) continue;
        if (!reg.all_of<TransformComponent>(s.handle)) continue;
        auto& tc = reg.get<TransformComponent>(s.handle);
        tc.parent = entt::null;
        tc.position = s.newWorldPos;
        tc.rotationEuler = s.newWorldEuler;
        tc.scale = s.newWorldScale;
        tc.useQuaternion = false;
    }
    Log::editor()->info("[parent] Ungroup: {} children -> root", m_pre.size());
}

void UngroupSelectionCommand::undo() {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
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
}

void UngroupSelectionCommand::onEntityRemap(entt::entity oldH, entt::entity newH) {
    for (auto& s : m_pre) {
        if (s.handle == oldH) s.handle = newH;
        if (s.oldParent == oldH) s.oldParent = newH;
    }
}

} // namespace Mood
