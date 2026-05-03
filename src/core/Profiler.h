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

#if defined(TRACY_ENABLE)

#include <tracy/Tracy.hpp>

#define MOOD_PROFILE_FRAME()           FrameMark
#define MOOD_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
#define MOOD_PROFILE_SCOPE(name)       ZoneScopedN(name)
#define MOOD_PROFILE_FUNCTION()        ZoneScoped
#define MOOD_PROFILE_PLOT(name, value) TracyPlot(name, value)
#define MOOD_PROFILE_MESSAGE(msg)      TracyMessageL(msg)

#else

#define MOOD_PROFILE_FRAME()           do {} while(0)
#define MOOD_PROFILE_FRAME_NAMED(name) do { (void)(name); } while(0)
#define MOOD_PROFILE_SCOPE(name)       do { (void)(name); } while(0)
#define MOOD_PROFILE_FUNCTION()        do {} while(0)
#define MOOD_PROFILE_PLOT(name, value) do { (void)(name); (void)(value); } while(0)
#define MOOD_PROFILE_MESSAGE(msg)      do { (void)(msg); } while(0)

#endif
