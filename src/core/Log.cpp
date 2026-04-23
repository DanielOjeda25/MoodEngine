#include "core/Log.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <vector>

namespace Mood::Log {

namespace {

std::shared_ptr<spdlog::logger> s_engine;
std::shared_ptr<spdlog::logger> s_editor;
std::shared_ptr<spdlog::logger> s_render;
std::shared_ptr<spdlog::logger> s_world;
std::shared_ptr<spdlog::logger> s_assets;

std::shared_ptr<spdlog::logger> makeLogger(const std::string& name,
                                           const std::vector<spdlog::sink_ptr>& sinks) {
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);
    spdlog::register_logger(logger);
    return logger;
}

} // namespace

void init() {
    // La carpeta logs/ se ignora en git pero se crea en runtime si no existe.
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] [%n] [%l] %v%$");

    // 5 MB por archivo, hasta 3 archivos rotatorios.
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/engine.log", 1024 * 1024 * 5, 3);
    fileSink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");

    const std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};

    s_engine = makeLogger("engine", sinks);
    s_editor = makeLogger("editor", sinks);
    s_render = makeLogger("render", sinks);
    s_world  = makeLogger("world",  sinks);
    s_assets = makeLogger("assets", sinks);
}

void shutdown() {
    spdlog::shutdown();
    s_engine.reset();
    s_editor.reset();
    s_render.reset();
    s_world.reset();
    s_assets.reset();
}

std::shared_ptr<spdlog::logger>& engine() { return s_engine; }
std::shared_ptr<spdlog::logger>& editor() { return s_editor; }
std::shared_ptr<spdlog::logger>& render() { return s_render; }
std::shared_ptr<spdlog::logger>& world()  { return s_world; }
std::shared_ptr<spdlog::logger>& assets() { return s_assets; }

} // namespace Mood::Log
