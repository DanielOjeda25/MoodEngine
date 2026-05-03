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

private:
    float m_yawDeg;
    float m_pitchDeg;
    float m_radius;
    glm::vec3 m_target{0.0f};
    float m_fovDeg = 60.0f;
    float m_near = 0.1f;
    float m_far = 100.0f;
};

} // namespace Mood
