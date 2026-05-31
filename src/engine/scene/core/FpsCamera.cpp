#include "engine/scene/core/FpsCamera.h"

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
    // F4H6: pitch incluye el pain offset (camera mira hacia arriba al recibir
    // damage). El offset se suma al pitch logico ANTES de la conversion a vec.
    const float pitch = glm::radians(m_pitchDeg + m_painPitchOffset);
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)
    ));
}

glm::vec3 FpsCamera::computeMoveDelta(const glm::vec3& dir, float dt) const {
    if (glm::length(dir) < 1e-4f) return glm::vec3(0.0f);

    const glm::vec3 fwd = forward();
    // Strafe sobre el plano XZ para que no se mezcle con el pitch.
    const glm::vec3 fwdFlat = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    const glm::vec3 right = glm::normalize(glm::cross(fwdFlat, k_worldUp));

    glm::vec3 delta = right * dir.x + k_worldUp * dir.y + fwdFlat * dir.z;
    if (glm::length(delta) < 1e-4f) return glm::vec3(0.0f);
    return glm::normalize(delta) * (m_speed * dt);
}

void FpsCamera::translate(const glm::vec3& delta) {
    m_position += delta;
}

void FpsCamera::move(const glm::vec3& dir, float dt) {
    translate(computeMoveDelta(dir, dt));
}

glm::mat4 FpsCamera::viewMatrix() const {
    // F4H6: shake position offset + pain roll. forward() ya incluye el
    // pain pitch offset (vease arriba).
    const glm::vec3 eye = m_position + m_shakePosOffset;
    const glm::vec3 fwd = forward();
    // Pain roll: rotamos el "up" alrededor del forward. Roll en grados.
    const float rollRad = glm::radians(m_painRollOffset);
    const float cr = std::cos(rollRad);
    const float sr = std::sin(rollRad);
    const glm::vec3 right = glm::normalize(glm::cross(fwd, k_worldUp));
    const glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    const glm::vec3 rolledUp = glm::normalize(up * cr + right * sr);
    return glm::lookAt(eye, eye + fwd, rolledUp);
}

glm::mat4 FpsCamera::projectionMatrix(float aspectRatio) const {
    if (aspectRatio <= 0.0f) aspectRatio = 1.0f;
    return glm::perspective(glm::radians(m_fovDeg), aspectRatio, m_near, m_far);
}

} // namespace Mood
