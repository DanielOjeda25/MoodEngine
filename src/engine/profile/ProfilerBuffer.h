#pragma once

// F3H23: ring buffer in-engine de timings por scope. Hooked desde
// `MOOD_PROFILE_SCOPE` en core/Profiler.h (ver MoodProfilerScope RAII).
// Vive aparte de Tracy: cuando Tracy está OFF, este buffer sigue
// funcionando — el ProfilerPanel del editor lee de acá.
//
// Modelo:
//   - Un único frame "actual" en construcción (m_currentFrame).
//   - Cada scope cerrado pushea {nameKey, ms} al frame actual.
//   - `endFrame()` (llamado una vez por main loop) snapshot del frame
//     al ring + reset del frame actual.
//   - El panel lee `framesRingBufferSnapshot()` para mostrar avg/min/max
//     + histograma.
//
// Single-thread: pensado para el main thread del editor (todos los
// MOOD_PROFILE_SCOPE existentes corren en main). Tracy ya cubre
// per-thread; no se duplica acá.

#include "core/Types.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mood {

class ProfilerBuffer {
public:
    /// @brief Entry single de un scope dentro de un frame.
    struct ScopeSample {
        const char* name;  // statically-allocated string literal (no copy).
        f32 milliseconds;
    };

    /// @brief Snapshot por scope agregando los últimos N frames.
    struct ScopeAggregate {
        const char* name = nullptr;
        f32 avgMs = 0.0f;
        f32 minMs = 0.0f;
        f32 maxMs = 0.0f;
        f32 lastMs = 0.0f;  // valor del frame más reciente
        u32 hitsPerFrame = 0;  // promedio de hits/frame del scope
    };

    /// @brief Constructor. `frameCapacity` clampeado a [60, 1200] — fuera
    ///        del rango, queda en 240 (default).
    explicit ProfilerBuffer(u32 frameCapacity = 240);

    /// @brief Redimensiona el ring buffer manteniendo los frames más
    ///        recientes. Llamado cuando el dev cambia
    ///        `UserSettings.editor.profilerFrameCount`.
    void resize(u32 frameCapacity);

    u32 frameCapacity() const { return m_capacity; }

    /// @brief Push de un scope cerrado al frame en construcción. Llamado
    ///        desde el destructor del RAII MoodProfilerScope (Profiler.h).
    ///        `name` debe ser un string literal con lifetime estático.
    void pushScope(const char* name, f32 milliseconds);

    /// @brief Cierra el frame actual y lo agrega al ring. Llamar UNA vez
    ///        al final del frame del editor (junto al `MOOD_PROFILE_FRAME()`
    ///        de Tracy).
    void endFrame();

    /// @brief Vista del frame más reciente (los samples crudos del último
    ///        `endFrame()`). Útil para el histograma "este frame".
    const std::vector<ScopeSample>& lastFrame() const { return m_lastFrame; }

    /// @brief Agrega los últimos N frames por nombre de scope. N se
    ///        clampea al capacity actual.
    std::vector<ScopeAggregate> aggregate(u32 lastNFrames) const;

    /// @brief Total time del frame más reciente (suma de todos los
    ///        scopes del top-level — útil para % frame). NO es lo mismo
    ///        que `dt`: cuenta solo lo que está instrumentado.
    f32 lastFrameTotalMs() const;

    /// @brief Cantidad de frames con data en el ring (sube hasta capacity
    ///        y se mantiene ahí — el ring es circular).
    u32 framesRecorded() const { return m_framesRecorded; }

private:
    u32 m_capacity = 240;
    u32 m_framesRecorded = 0;
    u32 m_writeIndex = 0;  // próxima slot del ring

    // Frame en construcción + el último cerrado (para `lastFrame()` sin
    // tocar el ring).
    std::vector<ScopeSample> m_currentFrame;
    std::vector<ScopeSample> m_lastFrame;

    // Ring de frames cerrados. Cada slot es el vector de scopes de ese
    // frame. El reserve agresivo evita realloc en hot path — capacidades
    // razonables (60-1200 * scopes_por_frame).
    std::vector<std::vector<ScopeSample>> m_ring;
};

/// @brief Acceso al ProfilerBuffer global del proceso. Single instancia
///        (single-thread). Inicializada lazy en el primer call — el
///        editor lo "calienta" en `EditorApplication::init` setteándole
///        el capacity de UserSettings.
ProfilerBuffer& profilerBuffer();

} // namespace Mood
