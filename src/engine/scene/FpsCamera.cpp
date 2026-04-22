#include "engine/scene/FpsCamera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace Mood {

namespace {
constexpr float k_lookSensitivity = 0.1f; // grados por pixel
constexpr float k_minPitch = -89.0f;
constexpr float k_maxPitch = 89.0f;
constexpr glm::vec3 k_worldUp{0.0f, 1.0f, 0.0f};
} // namespace

FpsCamera::FpsCamera(glm::vec3 position, float yawDeg, float pitchDeg)
    : m_position(position), m_yawDeg(yawDeg), m_pitchDeg(pitchDeg) {}

void FpsCamera::applyMouseMove(float dxPixels, float dyPixels) {
    m_yawDeg += dxPixels * k_lookSensitivity;
    // dy positivo (mouse baja) debe bajar la vista -> restamos.
    m_pitchDeg -= dyPixels * k_lookSensitivity;
    m_pitchDeg = std::clamp(m_pitchDeg, k_minPitch, k_maxPitch);
}

glm::vec3 FpsCamera::forward() const {
    const float yaw = glm::radians(m_yawDeg);
    const float pitch = glm::radians(m_pitchDeg);
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)
    ));
}

void FpsCamera::move(const glm::vec3& dir, float dt) {
    if (glm::length(dir) < 1e-4f) return;

    const glm::vec3 fwd = forward();
    // Strafe sobre el plano XZ para que no se mezcle con el pitch.
    const glm::vec3 fwdFlat = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    const glm::vec3 right = glm::normalize(glm::cross(fwdFlat, k_worldUp));

    glm::vec3 delta = right * dir.x + k_worldUp * dir.y + fwdFlat * dir.z;
    if (glm::length(delta) > 1e-4f) {
        delta = glm::normalize(delta) * (m_speed * dt);
        m_position += delta;
    }
}

glm::mat4 FpsCamera::viewMatrix() const {
    return glm::lookAt(m_position, m_position + forward(), k_worldUp);
}

glm::mat4 FpsCamera::projectionMatrix(float aspectRatio) const {
    if (aspectRatio <= 0.0f) aspectRatio = 1.0f;
    return glm::perspective(glm::radians(m_fovDeg), aspectRatio, m_near, m_far);
}

} // namespace Mood
