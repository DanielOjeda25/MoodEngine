#pragma once

// Camara FPS: posicion libre + yaw/pitch. Usada en Play Mode.
// Entrada esperada: WASD traslacion en plano XZ relativo al yaw,
// espacio/shift para subir/bajar en Y mundo, movimiento relativo del mouse
// para rotar (capturado con SDL_SetRelativeMouseMode).

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class FpsCamera {
public:
    FpsCamera(glm::vec3 position = {0.0f, 1.0f, 3.0f},
              float yawDeg = -90.0f, float pitchDeg = 0.0f);

    /// @brief Aplica un movimiento relativo del mouse (pixeles) a yaw/pitch.
    void applyMouseMove(float dxPixels, float dyPixels);

    /// @brief Avanza la posicion segun la direccion de movimiento solicitada
    ///        (valores -1/0/+1 en cada eje) durante el tiempo `dt`.
    ///        +X = strafe derecha, +Y = subir (world), +Z = adelante.
    void move(const glm::vec3& dir, float dt);

    glm::vec3 position() const { return m_position; }
    glm::vec3 forward() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;

    float fovDeg() const { return m_fovDeg; }

private:
    glm::vec3 m_position;
    float m_yawDeg;
    float m_pitchDeg;
    float m_speed = 3.0f;          // unidades por segundo
    float m_fovDeg = 70.0f;
    float m_near = 0.1f;
    float m_far = 100.0f;
};

} // namespace Mood
