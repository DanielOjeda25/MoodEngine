#include "engine/scene/EditorCamera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

namespace Mood {

namespace {
constexpr float k_rotateSpeed = 0.4f;   // grados por pixel
constexpr float k_zoomStep = 0.4f;      // unidades por "tick" de rueda
constexpr float k_minPitch = -89.0f;
constexpr float k_maxPitch = 89.0f;
constexpr float k_minRadius = 0.5f;
constexpr float k_maxRadius = 50.0f;
} // namespace

EditorCamera::EditorCamera(float yawDeg, float pitchDeg, float radius)
    : m_yawDeg(yawDeg), m_pitchDeg(pitchDeg), m_radius(radius) {}

void EditorCamera::applyMouseDrag(float dxPixels, float dyPixels) {
    m_yawDeg -= dxPixels * k_rotateSpeed;
    m_pitchDeg += dyPixels * k_rotateSpeed;
    m_pitchDeg = std::clamp(m_pitchDeg, k_minPitch, k_maxPitch);
}

void EditorCamera::applyWheel(float deltaSteps) {
    // Zoom multiplicativo: cada tick encoge/estira el radio por un factor.
    const float factor = (deltaSteps > 0.0f)
        ? (1.0f - k_zoomStep * deltaSteps)
        : (1.0f - k_zoomStep * deltaSteps); // misma formula; signo importa
    m_radius = std::clamp(m_radius * factor, k_minRadius, k_maxRadius);
}

glm::vec3 EditorCamera::position() const {
    const float yaw = glm::radians(m_yawDeg);
    const float pitch = glm::radians(m_pitchDeg);
    const float cp = std::cos(pitch);
    return m_target + glm::vec3(
        m_radius * cp * std::sin(yaw),
        m_radius * std::sin(pitch),
        m_radius * cp * std::cos(yaw)
    );
}

glm::mat4 EditorCamera::viewMatrix() const {
    return glm::lookAt(position(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 EditorCamera::projectionMatrix(float aspectRatio) const {
    if (aspectRatio <= 0.0f) {
        aspectRatio = 1.0f; // fallback para paneles colapsados
    }
    return glm::perspective(glm::radians(m_fovDeg), aspectRatio, m_near, m_far);
}

} // namespace Mood
