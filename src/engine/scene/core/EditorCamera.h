#pragma once

// Camara orbital de editor: rota alrededor de un punto (target) con dos
// angulos (yaw, pitch) y un radio. Entrada esperada: click-drag con boton
// derecho para rotar, rueda para zoom.

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class EditorCamera {
public:
    EditorCamera(float yawDeg = 45.0f, float pitchDeg = 30.0f, float radius = 30.0f);

    /// @brief Aplica un delta en pixeles del mouse right-drag a yaw/pitch.
    void applyMouseDrag(float dxPixels, float dyPixels);

    /// @brief Aplica un delta de la rueda del mouse al radio (zoom).
    ///        Positivo = acercarse.
    void applyWheel(float deltaSteps);

    /// @brief Paneo estilo Blender: middle-drag mueve el target de la camara
    ///        en el plano perpendicular al view direction (right y up de la
    ///        camara). Sensibilidad escalada por el radio para que sea
    ///        comparable con la escena a distintos zooms.
    void applyPan(float dxPixels, float dyPixels);

    glm::vec3 position() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;

    /// @brief "Frame selected" estilo Blender (tecla `.`): apunta el target
    ///        a `worldPos` y ajusta el radius para que un objeto del tamano
    ///        `objectRadius` quede comodamente en cuadro. Mantiene yaw/pitch
    ///        actuales para no desorientar al usuario. `objectRadius` puede
    ///        venir del bounding sphere del mesh; para entidades sin mesh
    ///        (Light/Audio) usar un default chico (~1.0).
    void focusOn(const glm::vec3& worldPos, float objectRadius = 1.0f);

    float fovDeg() const { return m_fovDeg; }

    /// F3H21: vista actual como (yaw,pitch,radius,target). Util para
    /// snapshot pre-lerp y para tests headless del helper numpad.
    float yawDeg() const { return m_yawDeg; }
    float pitchDeg() const { return m_pitchDeg; }
    float radius() const { return m_radius; }
    const glm::vec3& target() const { return m_target; }

    /// F3H21: pone yaw/pitch/radius/target directo (sin lerp). Usado por
    /// numpad cuando el dev tiene smoothView OFF, o como teleport interno
    /// del lerp al llegar al frame final.
    void setPose(float yawDeg, float pitchDeg, float radius, const glm::vec3& target);

    /// F3H21: arranca una transicion animada hacia la pose target. El
    /// `durationMs <= 0` equivale a setPose() (teleport). El driver vive
    /// en tick(dt) — el caller solo invoca beginLerpTo + tick por frame.
    /// Easing: smoothstep cubico (3t^2 - 2t^3), arranca y termina en
    /// velocidad cero — sensacion natural sin overshoot.
    void beginLerpTo(float yawDeg, float pitchDeg, float radius,
                     const glm::vec3& target, int durationMs);

    /// F3H21: avanza el lerp activo (si lo hay) por `dtSeconds`. NO-op si
    /// no hay lerp. Llamado por el caller del editor en su loop por frame
    /// con `ImGui::GetIO().DeltaTime`.
    void tick(float dtSeconds);

    /// F3H21: true si hay un lerp en curso. Util para test + para que el
    /// caller pueda saltear input de cam (rotate/pan/wheel) mientras lerp
    /// para no romper el "viaje" — convencion Blender.
    bool isLerping() const { return m_lerpRemainingSec > 0.0f; }

private:
    float m_yawDeg;
    float m_pitchDeg;
    float m_radius;
    glm::vec3 m_target{0.0f};
    float m_fovDeg = 60.0f;
    float m_near = 0.1f;
    float m_far = 100.0f;

    // F3H21: state del lerp numpad (Blender Smooth View). Si
    // m_lerpRemainingSec > 0, tick(dt) interpola yaw/pitch/radius/target
    // desde m_lerp{Start,End}_X hacia m_lerpEnd_X usando smoothstep.
    float m_lerpRemainingSec = 0.0f;
    float m_lerpDurationSec  = 0.0f;
    float m_lerpStart_yaw    = 0.0f;
    float m_lerpStart_pitch  = 0.0f;
    float m_lerpStart_radius = 0.0f;
    glm::vec3 m_lerpStart_target{0.0f};
    float m_lerpEnd_yaw      = 0.0f;
    float m_lerpEnd_pitch    = 0.0f;
    float m_lerpEnd_radius   = 0.0f;
    glm::vec3 m_lerpEnd_target{0.0f};
};

} // namespace Mood
