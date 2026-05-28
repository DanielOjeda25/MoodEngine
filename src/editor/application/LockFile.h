#pragma once

// F3H25 — Lock file del proyecto activo. Detecta crashes huérfanos
// comparando el PID guardado contra los procesos vivos. Convivenc/race
// con otros editores (proyecto abierto en 2 instancias a la vez) queda
// out-of-scope: el lock no impide la apertura concurrente, sólo registra
// "MoodEditor PID N tenía el proyecto abierto cuando dejó de existir".
//
// Formato del archivo (JSON one-liner para grep humano):
//   { "pid": 12345, "started_at": "2026-05-28T15:32:10Z", "engine_version": "v2.24.0" }
//
// Ubicación: `<projectRoot>/.moodproj.lock` — al lado del `.moodproj`,
// invisible salvo si el dev abre el File Explorer con hidden files. El
// lock se borra en `handleCloseProject` (cierre limpio) y se sobrescribe
// con un PID nuevo en `tryOpenProjectPath` (apertura nueva).

#include <filesystem>

namespace Mood::LockFile {

/// Resultado del chequeo previo a abrir un proyecto.
enum class Status {
    /// No hay lock file en disco; arranque limpio.
    Clean = 0,
    /// Lock file existe y el PID guardado YA no corresponde a un proceso
    /// vivo → cierre no-limpio (crash detectado). El caller decide qué
    /// hacer (modal de recuperación si hay autosave más reciente).
    Orphaned = 1,
    /// Lock file existe y el PID guardado SI corresponde a un proceso
    /// vivo → otro editor tiene este proyecto abierto. Hoy lo tratamos
    /// como Clean (overwrite del lock) — el flujo de "dos editores en
    /// paralelo" no está soportado y mostraría el modal de recovery
    /// equivocadamente.
    InUse = 2,
};

/// @brief Lee el lock file en `<projectRoot>/.moodproj.lock`, parsea el
///        PID y verifica si el proceso sigue vivo. Si no hay archivo,
///        retorna Clean. Si el JSON es inválido o no contiene PID,
///        retorna Clean (el archivo se sobrescribirá al hacer acquire).
Status check(const std::filesystem::path& projectRoot);

/// @brief Escribe el lock file con el PID actual + timestamp UTC ISO8601 +
///        versión del engine. Crea la carpeta del proyecto si no existe
///        (caso raro: la carpeta debería existir si vamos a abrirla).
///        Retorna true si la escritura fue OK.
bool acquire(const std::filesystem::path& projectRoot);

/// @brief Borra el lock file. Idempotente (no error si no existe).
///        Llamar en handleCloseProject y al salir del editor.
void release(const std::filesystem::path& projectRoot);

/// @brief Path absoluto al lock file de un proyecto. Útil para tests.
std::filesystem::path pathFor(const std::filesystem::path& projectRoot);

/// @brief PID del proceso actual. Util para tests que quieren simular
///        un PID propio (asegura que `check` lo detecte como vivo).
int currentPid();

/// @brief True si el PID corresponde a un proceso vivo en el sistema.
///        Útil para tests: pasarle el PID propio debe dar true; pasarle
///        un PID claramente inexistente (1 en Windows, 0) debe dar false.
bool isProcessAlive(int pid);

} // namespace Mood::LockFile
