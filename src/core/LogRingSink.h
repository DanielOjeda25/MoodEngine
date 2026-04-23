#pragma once

// Sink custom de spdlog que acumula las últimas N entradas en memoria. Sirve
// de fuente de datos para el `ConsolePanel` del editor (Hito 5 Bloque 6).
//
// Uso: instanciarlo una vez al arrancar (en `Mood::Log::init`), registrarlo
// como sink adicional de cada logger. El panel llama `snapshot()` por frame
// y renderiza el resultado.

#include "core/Types.h"

#include <mutex>
#include <spdlog/common.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <vector>

namespace Mood {

class LogRingSink : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
public:
    struct Entry {
        std::string channel;                 // nombre del logger ("engine", ...)
        std::string text;                    // payload (sin formatear el prefijo)
        spdlog::level::level_enum level = spdlog::level::info;
    };

    /// @brief Construye un ring buffer con capacidad fija. 512 por defecto es
    ///        suficiente para un par de minutos de logs a volumen normal.
    explicit LogRingSink(usize capacity = 512);

    /// @brief Copia el contenido actual del ring en orden cronológico (más
    ///        viejo primero). Thread-safe.
    std::vector<Entry> snapshot() const;

    /// @brief Vacía el ring buffer. Thread-safe.
    void clear();

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    mutable std::mutex m_mutex;
    std::vector<Entry> m_ring;   // storage circular; m_count entradas validas
    usize m_capacity;
    usize m_head = 0;            // proxima posicion a escribir
    usize m_count = 0;
};

} // namespace Mood
