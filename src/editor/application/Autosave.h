#pragma once

// F3H25 — Estado del autosave del editor: timer + path resolution +
// escritura atómica. No acopla a `EditorApplication`: el caller le pasa
// el callback que serializa (`writeFn`) cuando el timer dispara y la
// fuente del dirty flag.
//
// Trigger policy (decidido en F3H25):
//   "Solo si dirty + N min" — el timer acumula dt sin parar, pero el
//   write sólo dispara si `dirty()` devuelve true Y han pasado N min
//   desde el último write o desde el arranque. Si no hay cambios, skip
//   silencioso (sin reset del timer — el próximo cambio que toque
//   `markDirty` ya verá el timer maduro).
//
// Atomic write:
//   El callback `writeFn` escribe directamente al path final que le
//   demos; la atomicidad la garantizamos con un `<final>.tmp` que luego
//   renombramos. En Windows `std::filesystem::rename` es atómico si
//   origen y destino están en el mismo volumen.

#include "core/Types.h"

#include <filesystem>
#include <functional>

namespace Mood {

class Autosave {
public:
    /// @brief Función que serializa la escena al path dado. Debe lanzar
    ///        std::exception si falla; Autosave la captura y emite toast
    ///        de error sin reset del timer (para reintentar en el próximo
    ///        intervalo).
    using WriteFn = std::function<void(const std::filesystem::path& targetPath)>;

    /// @brief Función que devuelve true si hay cambios sin guardar
    ///        desde la última vez que se guardó (manual o auto).
    using DirtyFn = std::function<bool()>;

    Autosave() = default;

    /// @brief Configura el módulo. Llamar al abrir un proyecto. Resetea
    ///        el timer interno. Si el writeFn o dirtyFn son nulos, el
    ///        tick es no-op.
    void setup(const std::filesystem::path& projectRoot,
               const std::filesystem::path& currentMapRelPath,
               WriteFn writeFn,
               DirtyFn dirtyFn);

    /// @brief Desactiva el módulo. Llamar al cerrar un proyecto. Los
    ///        próximos `tick()` son no-op hasta el próximo `setup()`.
    void teardown();

    /// @brief Incrementa el timer interno. Si el dev tiene dirty Y han
    ///        pasado los N min configurados en UserSettings, dispara
    ///        write (vía writeFn) al path autosave. Llamar 1× por frame.
    void tick(f32 dtMs);

    /// @brief Borra el archivo de autosave + el .tmp si existe. Llamar
    ///        tras `handleSave` (el .moodmap canónico ya tiene la data).
    void clearOnDisk();

    /// @brief Path absoluto donde el autosave guardaría el mapa actual
    ///        (`<root>/.autosave/<mapname>.moodmap`). Devuelve vacío si
    ///        no hay setup activo.
    std::filesystem::path targetPath() const;

    /// @brief True si en `<root>/.autosave/<mapname>.moodmap` hay un
    ///        archivo más reciente que el `.moodmap` canónico. Llamar
    ///        antes de cargar el mapa para decidir si ofrecer recovery.
    bool autosaveMoreRecentThanCanonical() const;

    /// @brief Path al `.moodmap` canónico (root + relPath). Vacío si no
    ///        hay setup activo. Útil para tests.
    std::filesystem::path canonicalMapPath() const;

    /// @brief Millis acumulados desde el último write/setup. Útil para tests.
    f32 timerMs() const { return m_timerMs; }

    /// @brief Setea el timer manualmente. Útil para tests (saltar al
    ///        umbral sin esperar N minutos reales).
    void setTimerMs(f32 ms) { m_timerMs = ms; }

private:
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_mapRelPath;
    WriteFn m_writeFn;
    DirtyFn m_dirtyFn;
    f32 m_timerMs = 0.0f;
    bool m_active = false;
};

} // namespace Mood
