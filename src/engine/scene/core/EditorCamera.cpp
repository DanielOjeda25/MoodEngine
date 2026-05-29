#include "engine/scene/core/EditorCamera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

namespace Mood {

namespace {
constexpr float k_rotateSpeed = 0.4f;   // grados por pixel
constexpr float k_zoomStep = 0.4f;      // unidades por "tick" de rueda
constexpr float k_minPitch = -89.0f;
constexpr float k_maxPitch = 89.0f;
constexpr float k_minRadius = 0.5f;
// F3H29: m_maxRadius eliminado del namespace anónimo — pasa a member
// `m_maxRadius` configurable runtime via setMaxOrbitRadius() (default 50.0
// preserva el comportamiento pre-F3H29 si nadie inyecta UserSettings).
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
    m_radius = std::clamp(m_radius * factor, k_minRadius, m_maxRadius);
}

void EditorCamera::applyPan(float dxPixels, float dyPixels) {
    // Basis de la camara en coords de mundo.
    const glm::vec3 pos = position();
    const glm::vec3 forward = glm::normalize(m_target - pos);
    glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::length(right) < 1e-6f) {
        // Vista vertical pura (pitch cerca de +-90): fallback a X mundo.
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        right = glm::normalize(right);
    }
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // Sensibilidad proporcional al radio: a mas zoom-out, mas movimiento
    // por pixel; a zoom-in, menor. Asi el paneo se siente constante.
    const float panSpeed = 0.0015f * m_radius;
    m_target -= right * (dxPixels * panSpeed); // mouse a la derecha -> vista sigue al cursor
    m_target += up    * (dyPixels * panSpeed); // mouse hacia abajo  -> vista hacia arriba
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

void EditorCamera::focusOn(const glm::vec3& worldPos, float objectRadius) {
    m_target = worldPos;
    // Distancia para que `objectRadius` calce en el frustum vertical: el
    // half-FOV vertical define el angulo y `radius / sin(halfFov)` es la
    // distancia minima para que toda la esfera entre. Multiplicamos por 1.6
    // para dejar margen visual, sin que el objeto ocupe toda la pantalla.
    const float halfFov = glm::radians(m_fovDeg * 0.5f);
    const float minDist = objectRadius / std::max(std::sin(halfFov), 1e-3f);
    m_radius = std::clamp(minDist * 1.6f, k_minRadius, m_maxRadius);
}

glm::mat4 EditorCamera::projectionMatrix(float aspectRatio) const {
    if (aspectRatio <= 0.0f) {
        aspectRatio = 1.0f; // fallback para paneles colapsados
    }
    return glm::perspective(glm::radians(m_fovDeg), aspectRatio, m_near, m_far);
}

void EditorCamera::setPose(float yawDeg, float pitchDeg, float radius,
                            const glm::vec3& target) {
    m_yawDeg   = yawDeg;
    m_pitchDeg = std::clamp(pitchDeg, k_minPitch, k_maxPitch);
    m_radius   = std::clamp(radius, k_minRadius, m_maxRadius);
    m_target   = target;
    // cancelar lerp en curso — el setPose es teleport explicito
    m_lerpRemainingSec = 0.0f;
}

void EditorCamera::beginLerpTo(float yawDeg, float pitchDeg, float radius,
                                const glm::vec3& target, int durationMs) {
    // Duracion <= 0: teleport (equivalente al toggle smoothViewEnabled=false).
    if (durationMs <= 0) {
        setPose(yawDeg, pitchDeg, radius, target);
        return;
    }

    // Snapshot del start desde la pose actual.
    m_lerpStart_yaw    = m_yawDeg;
    m_lerpStart_pitch  = m_pitchDeg;
    m_lerpStart_radius = m_radius;
    m_lerpStart_target = m_target;

    // Yaw shortest-path: ajustar end al delta minimo desde start.
    // Sin esto, ir de 350 a 10 grados haria el camino largo (-340).
    float dy = yawDeg - m_yawDeg;
    while (dy > 180.0f)  dy -= 360.0f;
    while (dy < -180.0f) dy += 360.0f;

    m_lerpEnd_yaw    = m_yawDeg + dy;
    m_lerpEnd_pitch  = std::clamp(pitchDeg, k_minPitch, k_maxPitch);
    m_lerpEnd_radius = std::clamp(radius, k_minRadius, m_maxRadius);
    m_lerpEnd_target = target;

    m_lerpDurationSec  = static_cast<float>(durationMs) / 1000.0f;
    m_lerpRemainingSec = m_lerpDurationSec;
}

void EditorCamera::tick(float dtSeconds) {
    if (m_lerpRemainingSec <= 0.0f) return;

    m_lerpRemainingSec -= dtSeconds;

    if (m_lerpRemainingSec <= 0.0f) {
        // Llegamos: setear el end exacto y apagar el lerp.
        m_yawDeg   = m_lerpEnd_yaw;
        m_pitchDeg = m_lerpEnd_pitch;
        m_radius   = m_lerpEnd_radius;
        m_target   = m_lerpEnd_target;
        m_lerpRemainingSec = 0.0f;
        return;
    }

    // t en [0..1] desde el start. Easing smoothstep cubico: 3t^2 - 2t^3.
    // Velocidad cero en ambos extremos -> sensacion natural sin overshoot.
    const float elapsed = m_lerpDurationSec - m_lerpRemainingSec;
    const float tLin = std::clamp(elapsed / m_lerpDurationSec, 0.0f, 1.0f);
    const float t = tLin * tLin * (3.0f - 2.0f * tLin);

    m_yawDeg   = m_lerpStart_yaw    + (m_lerpEnd_yaw    - m_lerpStart_yaw)    * t;
    m_pitchDeg = m_lerpStart_pitch  + (m_lerpEnd_pitch  - m_lerpStart_pitch)  * t;
    m_radius   = m_lerpStart_radius + (m_lerpEnd_radius - m_lerpStart_radius) * t;
    m_target   = m_lerpStart_target + (m_lerpEnd_target - m_lerpStart_target) * t;
}

} // namespace Mood
