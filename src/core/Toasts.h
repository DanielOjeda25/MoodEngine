#pragma once

// F3H24: sistema de toasts (notificaciones efímeras no-modales) para el
// editor. Estilo VSCode — chips en la esquina inferior derecha, slide-in
// desde fuera de pantalla, fade-out al expirar.
//
// Modelo:
//   - Cola global thread-safe (mutex) con push() desde cualquier código.
//   - El editor llama `snapshot()` cada frame para renderizar y
//     `tick(dtMs)` para envejecer los toasts (descarta los expirados).
//   - Lifetime per-toast (default 3000 ms desde UserSettings, override
//     opcional al push).
//   - 4 niveles de severidad (Info / Success / Warn / Error) que el
//     overlay mapea a color de fondo.
//
// Convención de uso (de los 4 sitios autorizados por el dev):
//   Toasts::pushSuccess("Proyecto guardado");
//   Toasts::pushInfo("Texturas: 5 cargadas");
//   Toasts::pushWarn("Shader: 2 warnings");
//   Toasts::pushError("Error: no se pudo abrir <path>");

#include "core/Types.h"

#include <string>
#include <vector>

namespace Mood::Toasts {

enum class Severity : u8 {
    Info    = 0,
    Success = 1,
    Warn    = 2,
    Error   = 3,
};

struct Toast {
    Severity severity = Severity::Info;
    std::string message;
    /// Milisegundos restantes hasta expirar. Se decrementa cada `tick`.
    f32 remainingMs = 3000.0f;
    /// Lifetime inicial — útil para el overlay (calcular % vida → alpha
    /// del fade-out final).
    f32 totalMs = 3000.0f;
    /// F3H26: ID estable y monótonamente creciente asignado en `push`.
    /// El overlay lo usa como ID de la ventana ImGui (sin esto, ImGui ve
    /// un ID diferente por frame y recrea la ventana sin cache de size,
    /// causando flicker en el primer frame de aparición).
    u64 id = 0;
};

/// @brief Push un toast a la cola. Lifetime 0 = usa el default de
///        UserSettings (`toastsLifetimeMs`, clampeado 500-10000).
///        Thread-safe.
void push(Severity sev, std::string message, f32 lifetimeMs = 0.0f);

/// @brief Helpers convenientes para los 4 niveles.
inline void pushInfo(std::string m, f32 lt = 0.0f)    { push(Severity::Info,    std::move(m), lt); }
inline void pushSuccess(std::string m, f32 lt = 0.0f) { push(Severity::Success, std::move(m), lt); }
inline void pushWarn(std::string m, f32 lt = 0.0f)    { push(Severity::Warn,    std::move(m), lt); }
inline void pushError(std::string m, f32 lt = 0.0f)   { push(Severity::Error,   std::move(m), lt); }

/// @brief Copia los toasts vivos (no expirados). Llamado por el overlay
///        cada frame para renderizar. Orden = orden de push (más viejos
///        primero — el overlay los stackea con el más nuevo abajo).
std::vector<Toast> snapshot();

/// @brief Decrementa `remainingMs` de cada toast por `dtMs` y descarta
///        los expirados (≤ 0). Llamar UNA vez por frame desde
///        EditorApplication.
void tick(f32 dtMs);

/// @brief Vacia la cola (útil para tests o para limpiar al abrir
///        proyecto). El editor no lo llama en run-time.
void clear();

/// @brief Total de toasts vivos en la cola (sin filtrar). Útil para
///        tests + dev-tools.
usize size();

} // namespace Mood::Toasts
