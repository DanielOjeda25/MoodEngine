#pragma once

// Wrapper de macros sobre Tracy (F2H2). Cuando `TRACY_ENABLE` esta definido
// (lo activa el target Tracy::TracyClient via CMake con MOOD_PROFILE=ON), las
// macros expanden a las primitivas reales de Tracy. Cuando no, son no-op total
// — coste cero en release builds sin profiling.
//
// Uso tipico:
//   void MySystem::update() {
//       MOOD_PROFILE_FUNCTION();          // zona = "MySystem::update"
//       for (...) {
//           MOOD_PROFILE_SCOPE("inner");  // sub-zona con nombre fijo
//           ...
//       }
//   }
//
// Marcar fin de frame (una vez al final del loop principal):
//   MOOD_PROFILE_FRAME();
//
// Plotear un valor numerico cada frame (apareceran como graficos en Tracy):
//   MOOD_PROFILE_PLOT("FPS", currentFps);
//
// F3H23: MOOD_PROFILE_SCOPE además del Tracy zone alimenta el
// ProfilerBuffer in-engine (RAII timer en `Mood::detail::ScopeTimer`)
// — así el ProfilerPanel del editor ve los mismos ~50 scopes ya
// instrumentados sin agregar otra macro nueva. Cuando TRACY_ENABLE
// está OFF, solo corre el ring buffer (Tracy macros caen a no-op).
// Cuando ambos están off (release sin profiling), `MOOD_PROFILE=OFF`
// en CMake → todo no-op.

#include "core/Types.h"

#include <chrono>

namespace Mood::detail {

/// RAII timer que pushea el scope al ProfilerBuffer global en su
/// destructor. La declaración del push vive en
/// `engine/profile/ProfilerBuffer.h` para evitar incluir el header
/// pesado desde core/Profiler.h (que está en casi todos los .cpp).
class ScopeTimer {
public:
    explicit ScopeTimer(const char* name) noexcept
        : m_name(name)
        , m_start(std::chrono::steady_clock::now()) {}
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    const char* m_name;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace Mood::detail

// Macro helpers para concatenar __LINE__ al identificador del RAII.
#define MOOD_PROFILER_CONCAT_INNER(a, b) a##b
#define MOOD_PROFILER_CONCAT(a, b) MOOD_PROFILER_CONCAT_INNER(a, b)

#if defined(TRACY_ENABLE)

#include <tracy/Tracy.hpp>

#define MOOD_PROFILE_FRAME()           FrameMark
#define MOOD_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
#define MOOD_PROFILE_SCOPE(name)                                            \
    ::Mood::detail::ScopeTimer MOOD_PROFILER_CONCAT(_mood_scope_, __LINE__)(name); \
    ZoneScopedN(name)
#define MOOD_PROFILE_FUNCTION()                                             \
    ::Mood::detail::ScopeTimer MOOD_PROFILER_CONCAT(_mood_scope_, __LINE__)(__FUNCTION__); \
    ZoneScoped
#define MOOD_PROFILE_PLOT(name, value) TracyPlot(name, value)
#define MOOD_PROFILE_MESSAGE(msg)      TracyMessageL(msg)

#else

#define MOOD_PROFILE_FRAME()           do {} while(0)
#define MOOD_PROFILE_FRAME_NAMED(name) do { (void)(name); } while(0)
#define MOOD_PROFILE_SCOPE(name)                                            \
    ::Mood::detail::ScopeTimer MOOD_PROFILER_CONCAT(_mood_scope_, __LINE__)(name)
#define MOOD_PROFILE_FUNCTION()                                             \
    ::Mood::detail::ScopeTimer MOOD_PROFILER_CONCAT(_mood_scope_, __LINE__)(__FUNCTION__)
#define MOOD_PROFILE_PLOT(name, value) do { (void)(name); (void)(value); } while(0)
#define MOOD_PROFILE_MESSAGE(msg)      do { (void)(msg); } while(0)

#endif
