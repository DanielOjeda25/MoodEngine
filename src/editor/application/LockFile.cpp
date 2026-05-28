#include "editor/application/LockFile.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <cerrno>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <signal.h>
#  include <unistd.h>
#endif

namespace Mood::LockFile {

namespace {

constexpr const char* kLockFilename = ".moodproj.lock";

std::string isoUtcNow() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace

std::filesystem::path pathFor(const std::filesystem::path& projectRoot) {
    return projectRoot / kLockFilename;
}

int currentPid() {
#ifdef _WIN32
    return static_cast<int>(::GetCurrentProcessId());
#else
    return static_cast<int>(::getpid());
#endif
}

bool isProcessAlive(int pid) {
    if (pid <= 0) return false;
#ifdef _WIN32
    // OpenProcess con SYNCHRONIZE es suficiente para chequear existencia
    // (no requiere PROCESS_QUERY_INFORMATION, que falla en procesos de
    // otra session). Si el PID está libre, OpenProcess devuelve NULL.
    HANDLE h = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) return false;
    // Verificación adicional: el handle puede abrirse sobre un proceso
    // que terminó hace poco (PID en estado "zombie"). WaitForSingleObject
    // con timeout 0 retorna WAIT_OBJECT_0 si ya terminó.
    DWORD wait = ::WaitForSingleObject(h, 0);
    ::CloseHandle(h);
    return wait != WAIT_OBJECT_0;
#else
    // POSIX: kill(pid, 0) no envía señal, sólo chequea permisos +
    // existencia. errno=ESRCH si el PID no existe; ESRCH es el único
    // caso donde podemos afirmar "muerto".
    if (::kill(pid, 0) == 0) return true;
    return errno != 0 && errno != 3 /* ESRCH */;
#endif
}

Status check(const std::filesystem::path& projectRoot) {
    const auto p = pathFor(projectRoot);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return Status::Clean;

    std::ifstream in(p);
    if (!in.is_open()) {
        Log::editor()->warn("[lock] no se pudo abrir '{}' — tratando como Clean",
                            p.generic_string());
        return Status::Clean;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::editor()->warn("[lock] parse error en '{}': {} — tratando como Clean",
                            p.generic_string(), e.what());
        return Status::Clean;
    }
    if (!j.is_object() || !j.contains("pid") || !j.at("pid").is_number_integer()) {
        return Status::Clean;
    }
    const int pid = j.at("pid").get<int>();
    if (pid == currentPid()) {
        // Caso raro: el mismo proceso quedó con el lock viejo (debug,
        // reimporto del proyecto en la misma sesión). Tratarlo como
        // Clean (no es un crash, es nuestro propio PID).
        return Status::Clean;
    }
    if (isProcessAlive(pid)) {
        Log::editor()->warn("[lock] proyecto abierto en otro proceso (PID={}) — overwrite",
                            pid);
        return Status::InUse;
    }
    Log::editor()->info("[lock] PID huérfano {} detectado en '{}' — recovery candidate",
                        pid, p.generic_string());
    return Status::Orphaned;
}

bool acquire(const std::filesystem::path& projectRoot) {
    std::error_code ec;
    std::filesystem::create_directories(projectRoot, ec);
    if (ec) {
        Log::editor()->warn("[lock] no se pudo crear '{}': {}",
                            projectRoot.generic_string(), ec.message());
        return false;
    }
    const auto p = pathFor(projectRoot);
    std::ofstream out(p, std::ios::trunc);
    if (!out.is_open()) {
        Log::editor()->warn("[lock] no se pudo escribir '{}'", p.generic_string());
        return false;
    }
    nlohmann::json j;
    j["pid"] = currentPid();
    j["started_at"] = isoUtcNow();
    // Engine version: hoy hardcodeada al tag actual en CMake; preferimos
    // string corto a leer el VERSION file (que no existe en este repo).
    j["engine_version"] = "v2.24.x";
    out << j.dump() << "\n";
    return true;
}

void release(const std::filesystem::path& projectRoot) {
    std::error_code ec;
    std::filesystem::remove(pathFor(projectRoot), ec);
    // No log si no existe — es idempotente.
}

} // namespace Mood::LockFile
