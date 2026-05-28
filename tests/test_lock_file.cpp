// F3H25 — Tests del LockFile del proyecto.
//
// Hot-paths cubiertos sin tocar procesos reales del SO:
//   - check sin lock → Clean.
//   - acquire + check → Clean (PID propio se trata como Clean para no
//     auto-disparar recovery sobre el mismo proceso).
//   - release borra el archivo + es idempotente.
//   - Lock con PID huérfano (PID=1 en Windows que no existe a nivel
//     usuario) → Orphaned.
//   - Lock con PID propio → Clean.
//   - Lock con JSON malformado → Clean (sin crash).
//
// Filesystem usa un tmpDir único por test (path con timestamp + PID
// para evitar colisiones si se corre la suite en paralelo).

#include <doctest/doctest.h>

#include "editor/application/LockFile.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace Mood;

namespace {

std::filesystem::path makeTmpDir(const std::string& tag) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto p = std::filesystem::temp_directory_path()
             / ("moodlock_" + tag + "_" + std::to_string(now)
                + "_" + std::to_string(LockFile::currentPid()));
    std::filesystem::create_directories(p);
    return p;
}

void writeRawLock(const std::filesystem::path& projectRoot,
                  const std::string& content) {
    std::ofstream out(LockFile::pathFor(projectRoot), std::ios::trunc);
    out << content;
}

} // namespace

TEST_CASE("F3H25 LockFile: check sin lock → Clean") {
    const auto root = makeTmpDir("clean");
    CHECK(LockFile::check(root) == LockFile::Status::Clean);
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: acquire crea el archivo + check con PID propio → Clean") {
    const auto root = makeTmpDir("acquire");
    REQUIRE(LockFile::acquire(root));
    CHECK(std::filesystem::exists(LockFile::pathFor(root)));
    // El check con el PID propio se trata como Clean (no es un crash de
    // otra sesión) — evita auto-disparar recovery sobre nosotros mismos.
    CHECK(LockFile::check(root) == LockFile::Status::Clean);
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: release borra el archivo") {
    const auto root = makeTmpDir("release");
    REQUIRE(LockFile::acquire(root));
    REQUIRE(std::filesystem::exists(LockFile::pathFor(root)));
    LockFile::release(root);
    CHECK_FALSE(std::filesystem::exists(LockFile::pathFor(root)));
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: release sin archivo es idempotente") {
    const auto root = makeTmpDir("release_noop");
    LockFile::release(root);  // no debe lanzar ni warnear catastróficamente
    CHECK_FALSE(std::filesystem::exists(LockFile::pathFor(root)));
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: PID huérfano → Orphaned") {
    const auto root = makeTmpDir("orphan");
    // PID=999999 no existe a nivel usuario en Windows ni en Linux. Hace
    // que isProcessAlive devuelva false → status Orphaned.
    nlohmann::json j;
    j["pid"] = 999999;
    j["started_at"] = "2026-01-01T00:00:00Z";
    j["engine_version"] = "v2.0.0";
    writeRawLock(root, j.dump());
    CHECK(LockFile::check(root) == LockFile::Status::Orphaned);
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: JSON malformado → Clean (no crash)") {
    const auto root = makeTmpDir("malformed");
    writeRawLock(root, "{ this is not valid json");
    CHECK(LockFile::check(root) == LockFile::Status::Clean);
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: JSON sin pid → Clean") {
    const auto root = makeTmpDir("no_pid");
    nlohmann::json j;
    j["started_at"] = "2026-01-01T00:00:00Z";
    writeRawLock(root, j.dump());
    CHECK(LockFile::check(root) == LockFile::Status::Clean);
    std::filesystem::remove_all(root);
}

TEST_CASE("F3H25 LockFile: isProcessAlive del PID propio = true, PID=0 = false") {
    CHECK(LockFile::isProcessAlive(LockFile::currentPid()) == true);
    CHECK(LockFile::isProcessAlive(0) == false);
    CHECK(LockFile::isProcessAlive(-1) == false);
}
