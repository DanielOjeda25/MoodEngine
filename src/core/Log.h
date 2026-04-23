#pragma once

// Wrapper sobre spdlog. Expone loggers con nombre por subsistema y un conjunto
// de macros breves para uso comun. Formato detallado configurado en Log.cpp.

#include <memory>
#include <spdlog/spdlog.h>

namespace Mood {
class LogRingSink;
namespace Log {

/// @brief Inicializa sinks (consola + archivo rotatorio) y crea los loggers.
///        Llamar una sola vez al arrancar la aplicacion.
void init();

/// @brief Cierra spdlog limpiamente. Llamar al apagar la aplicacion.
void shutdown();

/// @brief Logger principal del motor (`engine`).
std::shared_ptr<spdlog::logger>& engine();

/// @brief Logger del editor (`editor`).
std::shared_ptr<spdlog::logger>& editor();

/// @brief Logger del subsistema de render (`render`).
std::shared_ptr<spdlog::logger>& render();

/// @brief Logger del mundo/nivel (`world`): mapas, tiles, colisiones.
std::shared_ptr<spdlog::logger>& world();

/// @brief Logger de gestion de assets (`assets`): AssetManager, VFS,
///        fallbacks, cache hits/misses.
std::shared_ptr<spdlog::logger>& assets();

/// @brief Sink en memoria con las últimas 512 entradas de log. El
///        `ConsolePanel` del editor lo consume para mostrar los logs en vivo.
///        `nullptr` si no se llamó a `init()` aún.
LogRingSink* ringSink();

} // namespace Log
} // namespace Mood

// Macros de conveniencia para el logger `engine`.
#define MOOD_LOG_TRACE(...)    ::Mood::Log::engine()->trace(__VA_ARGS__)
#define MOOD_LOG_DEBUG(...)    ::Mood::Log::engine()->debug(__VA_ARGS__)
#define MOOD_LOG_INFO(...)     ::Mood::Log::engine()->info(__VA_ARGS__)
#define MOOD_LOG_WARN(...)     ::Mood::Log::engine()->warn(__VA_ARGS__)
#define MOOD_LOG_ERROR(...)    ::Mood::Log::engine()->error(__VA_ARGS__)
#define MOOD_LOG_CRITICAL(...) ::Mood::Log::engine()->critical(__VA_ARGS__)
