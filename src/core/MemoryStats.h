#pragma once

// F3H23: helpers para leer working set (RSS) del proceso del editor.
// Usado por el stats overlay y el ProfilerPanel para mostrar consumo de
// memoria. Implementación per-plataforma — Windows usa
// `GetProcessMemoryInfo`, otros plataformas devuelven 0 (no fallar).

#include "core/Types.h"

namespace Mood::MemoryStats {

/// @brief Working set (RSS) del proceso en bytes. En Windows usa
///        `GetProcessMemoryInfo`. Retorna 0 si la API falló o la
///        plataforma no está implementada (silencioso — el caller
///        muestra "—" en la UI).
u64 getRssBytes();

} // namespace Mood::MemoryStats
