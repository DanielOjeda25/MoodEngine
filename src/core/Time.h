#pragma once

// Utilidades de tiempo: cronometro de alta resolucion y contador de FPS con
// promedio movil.

#include "core/Types.h"

#include <array>
#include <chrono>

namespace Mood {

/// @brief Cronometro simple de alta resolucion.
class Timer {
public:
    Timer() { reset(); }

    /// @brief Reinicia el punto de referencia a `ahora`.
    void reset() { m_start = std::chrono::steady_clock::now(); }

    /// @brief Segundos transcurridos desde el ultimo reset (f64 por precision).
    f64 elapsedSeconds() const {
        using namespace std::chrono;
        return duration<f64>(steady_clock::now() - m_start).count();
    }

    /// @brief Milisegundos transcurridos desde el ultimo reset.
    f64 elapsedMillis() const { return elapsedSeconds() * 1000.0; }

private:
    std::chrono::steady_clock::time_point m_start;
};

/// @brief Contador de FPS con promedio movil sobre los ultimos N frames.
///        N se fija en compile-time (k_SampleCount).
class FpsCounter {
public:
    /// @brief Registra un frame con su duracion en segundos y actualiza el
    ///        promedio. Devuelve el FPS suavizado.
    f32 tick(f64 deltaSeconds);

    /// @brief Ultimo FPS suavizado calculado.
    f32 fps() const { return m_fps; }

private:
    static constexpr Mood::usize k_SampleCount = 60;

    std::array<f64, k_SampleCount> m_samples{};
    Mood::usize m_nextIndex = 0;
    Mood::usize m_filled = 0;
    f32 m_fps = 0.0f;
};

} // namespace Mood
