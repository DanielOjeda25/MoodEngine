#include "editor/commands/SetParentCommand.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Mood {

namespace {

// Decompone una world matrix en TRS (translation/rotation Euler/scale)
// en LOCAL space relativo a `parentWorldInv`. Para `parentWorldInv ==
// identity` (sin padre), devuelve el TRS world directo.
//
// Convención del Inspector: euler como `rotationEuler.{x,y,z}` en orden
// pitch/yaw/roll (mismo que TransformComponent::worldMatrix() asume).
void decomposeLocal(const glm::mat4& world, const glm::mat4& parentWorldInv,
                     glm::vec3& outPos, glm::vec3& outEuler,
                     glm::vec3& outScale) {
    const glm::mat4 local = parentWorldInv * world;

    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat rotation;
    if (!glm::decompose(local, outScale, rotation, outPos, skew, perspective)) {
        // Fallback: extraer translation de la columna 3, scale como
        // norma de las columnas, rotation identity.
        outPos = glm::vec3(local[3]);
        outScale = glm::vec3(1.0f);
        outEuler = glm::vec3(0.0f);
        return;
    }
    // glm::decompose devuelve quaternion en convencion W=componente real.
    // glm::eulerAngles devuelve XYZ en orden roll-pitch-yaw. Convertimos
    // a la convención euler.{x=pitch, y=yaw, z=roll} en GRADOS.
    const glm::vec3 e = glm::eulerAngles(rotation);  // rad, XYZ
    outEuler = glm::degrees(e);
}

} // namespace

SetParentCommand::SetParentCommand(Scene* scene, entt::entity child,
                                     entt::entity newParent)
    : m_scene(scene), m_child(child),
      m_oldParent(entt::null), m_newParent(newParent),
      m_oldLocalPos(0.0f), m_oldLocalEuler(0.0f), m_oldLocalScale(1.0f),
      m_newLocalPos(0.0f), m_newLocalEuler(0.0f), m_newLocalScale(1.0f) {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    if (!reg.valid(m_child)) return;
    if (!reg.all_of<TransformComponent>(m_child)) return;

    auto& tc = reg.get<TransformComponent>(m_child);
    // Snapshot del local PRE-cambio (es el local respecto al old parent).
    m_oldParent      = tc.parent;
    m_oldLocalPos    = tc.position;
    m_oldLocalEuler  = tc.rotationEuler;
    m_oldLocalScale  = tc.scale;

    // Computar el local POST-cambio que preserva world-space.
    // worldOld = parentOldWorld * localOld
    // queremos: worldNew == worldOld
    // localNew = inverse(parentNewWorld) * worldOld
    const glm::mat4 oldWorld = m_scene->worldMatrixOf(m_child);
    glm::mat4 newParentWorldInv(1.0f);
    if (m_newParent != entt::null && reg.valid(m_newParent)) {
        const glm::mat4 newParentWorld = m_scene->worldMatrixOf(m_newParent);
        newParentWorldInv = glm::inverse(newParentWorld);
    }
    decomposeLocal(oldWorld, newParentWorldInv,
                    m_newLocalPos, m_newLocalEuler, m_newLocalScale);
}

void SetParentCommand::execute() {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    if (!reg.valid(m_child)) return;
    if (!reg.all_of<TransformComponent>(m_child)) return;
    auto& tc = reg.get<TransformComponent>(m_child);
    tc.parent = m_newParent;
    tc.position = m_newLocalPos;
    tc.rotationEuler = m_newLocalEuler;
    tc.scale = m_newLocalScale;
    tc.useQuaternion = false;  // Inspector mode (euler ganan post-reparent).
    Log::editor()->info("[parent] SetParent child={} newParent={}",
                         static_cast<u32>(m_child),
                         m_newParent == entt::null
                             ? std::string{"null"}
                             : std::to_string(static_cast<u32>(m_newParent)));
}

void SetParentCommand::undo() {
    if (m_scene == nullptr) return;
    auto& reg = m_scene->registry();
    if (!reg.valid(m_child)) return;
    if (!reg.all_of<TransformComponent>(m_child)) return;
    auto& tc = reg.get<TransformComponent>(m_child);
    tc.parent = m_oldParent;
    tc.position = m_oldLocalPos;
    tc.rotationEuler = m_oldLocalEuler;
    tc.scale = m_oldLocalScale;
    tc.useQuaternion = false;
}

std::string SetParentCommand::name() const {
    return m_newParent == entt::null ? "Quitar padre" : "Asignar padre";
}

void SetParentCommand::onEntityRemap(entt::entity oldH, entt::entity newH) {
    if (m_child == oldH) m_child = newH;
    if (m_oldParent == oldH) m_oldParent = newH;
    if (m_newParent == oldH) m_newParent = newH;
}

} // namespace Mood
