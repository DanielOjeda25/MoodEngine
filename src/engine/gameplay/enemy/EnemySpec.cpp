#include "engine/gameplay/enemy/EnemySpec.h"

#include "core/Log.h"

#include <algorithm>
#include <fstream>

namespace Mood::Enemy {

// =============================================================
// Serializacion
// =============================================================

nlohmann::json Spec::toJson() const {
    return nlohmann::json{
        {"_version",       k_schemaVersion},
        {"displayName",    displayName},

        {"health",         health},

        {"aggroRange",     aggroRange},
        {"attackRange",    attackRange},

        {"moveSpeed",      moveSpeed},

        {"damage",         damage},
        {"attackCooldown", attackCooldown},

        {"painThreshold",  painThreshold},
        {"painDuration",   painDuration},

        {"viewmodelMesh",  viewmodelMesh},
        {"hitSound",       hitSound},
        {"deathSound",     deathSound},
    };
}

Spec Spec::fromJson(const nlohmann::json& j) {
    Spec s;
    if (!j.is_object()) {
        Log::engine()->error("[EnemySpec] fromJson: JSON no es objeto");
        return s;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[EnemySpec] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return s;
    }

    s.displayName     = j.value("displayName",    std::string{});

    s.health          = j.value("health",         50.0f);

    s.aggroRange      = j.value("aggroRange",     12.0f);
    s.attackRange     = j.value("attackRange",    2.0f);

    s.moveSpeed       = j.value("moveSpeed",      4.0f);

    s.damage          = j.value("damage",         15.0f);
    s.attackCooldown  = j.value("attackCooldown", 1.0f);

    s.painThreshold   = j.value("painThreshold",  10.0f);
    s.painDuration    = j.value("painDuration",   0.3f);

    s.viewmodelMesh   = j.value("viewmodelMesh",  std::string{});
    s.hitSound        = j.value("hitSound",       std::string{});
    s.deathSound      = j.value("deathSound",     std::string{});

    // Clamps de sanidad — el loader no puede confiar en que el .moodenemy
    // sea valido. Valores fuera de rango pueden romper el EnemySystem
    // (dist negativa, cooldown=0 dispara cada frame, etc.).
    if (s.health < 1.0f)             s.health = 1.0f;
    if (s.aggroRange < 0.01f)        s.aggroRange = 0.01f;
    if (s.attackRange < 0.01f)       s.attackRange = 0.01f;
    if (s.moveSpeed < 0.0f)          s.moveSpeed = 0.0f;
    if (s.moveSpeed > 50.0f)         s.moveSpeed = 50.0f;
    if (s.damage < 0.0f)             s.damage = 0.0f;
    if (s.attackCooldown < 0.05f)    s.attackCooldown = 0.05f;
    if (s.painThreshold < 0.0f)      s.painThreshold = 0.0f;
    if (s.painDuration < 0.05f)      s.painDuration = 0.05f;

    return s;
}

// =============================================================
// I/O de disco
// =============================================================

std::optional<Spec> Spec::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in.is_open()) {
        Log::engine()->warn("[EnemySpec] no se pudo abrir '{}'",
                            fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[EnemySpec] parse error en '{}': {}",
                             fsPath.generic_string(), e.what());
        return std::nullopt;
    }
    return fromJson(j);
}

bool Spec::saveToFile(const std::filesystem::path& fsPath) const {
    std::error_code ec;
    std::filesystem::create_directories(fsPath.parent_path(), ec);
    std::ofstream out(fsPath);
    if (!out.is_open()) {
        Log::engine()->error("[EnemySpec] no se pudo abrir '{}' para escritura",
                             fsPath.generic_string());
        return false;
    }
    try {
        out << toJson().dump(2);
    } catch (const std::exception& e) {
        Log::engine()->error("[EnemySpec] error al escribir '{}': {}",
                             fsPath.generic_string(), e.what());
        return false;
    }
    return true;
}

} // namespace Mood::Enemy
