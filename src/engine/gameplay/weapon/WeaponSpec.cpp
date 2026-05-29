#include "engine/gameplay/weapon/WeaponSpec.h"

#include "core/Log.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Mood::Weapon {

// =============================================================
// Serializacion
// =============================================================

nlohmann::json Spec::toJson() const {
    return nlohmann::json{
        {"_version",          k_schemaVersion},
        {"displayName",       displayName},
        {"category",          category},

        {"damage",            damage},
        {"range",             range},
        {"pellets",           pellets},
        {"spreadDeg",         spreadDeg},

        {"fireRatePerSec",    fireRatePerSec},
        {"magazineSize",      magazineSize},
        {"reloadTimeSec",     reloadTimeSec},

        {"viewmodelMesh",     viewmodelMesh},
        {"viewmodelMaterial", viewmodelMaterial},
        {"fireSound",         fireSound},
        {"impactSound",       impactSound},
        {"muzzleVfx",         muzzleVfx},
        {"impactVfx",         impactVfx},

        {"ignoreOwner",       ignoreOwner},
    };
}

Spec Spec::fromJson(const nlohmann::json& j) {
    Spec s;
    if (!j.is_object()) {
        Log::engine()->error("[WeaponSpec] fromJson: JSON no es objeto");
        return s;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[WeaponSpec] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return s;
    }

    s.displayName       = j.value("displayName",       std::string{});
    s.category          = j.value("category",          std::string{"hitscan"});

    s.damage            = j.value("damage",            10.0f);
    s.range             = j.value("range",             50.0f);
    s.pellets           = j.value("pellets",           1u);
    s.spreadDeg         = j.value("spreadDeg",         0.0f);

    s.fireRatePerSec    = j.value("fireRatePerSec",    1.0f);
    s.magazineSize      = j.value("magazineSize",      10u);
    s.reloadTimeSec     = j.value("reloadTimeSec",     1.0f);

    s.viewmodelMesh     = j.value("viewmodelMesh",     std::string{});
    s.viewmodelMaterial = j.value("viewmodelMaterial", std::string{});
    s.fireSound         = j.value("fireSound",         std::string{});
    s.impactSound       = j.value("impactSound",       std::string{});
    s.muzzleVfx         = j.value("muzzleVfx",         std::string{});
    s.impactVfx         = j.value("impactVfx",         std::string{});

    s.ignoreOwner       = j.value("ignoreOwner",       true);

    // Clamps de sanidad — el loader no puede confiar en que el .moodweapon
    // sea valido. Valores fuera de rango pueden romper el WeaponSystem
    // (division por cero en fireRate, raycast hacia atras, etc.).
    if (s.damage < 0.0f) s.damage = 0.0f;
    if (s.range < 0.0f)  s.range  = 0.0f;
    if (s.pellets < 1u)  s.pellets = 1u;
    if (s.spreadDeg < 0.0f) s.spreadDeg = 0.0f;
    if (s.spreadDeg > 89.0f) s.spreadDeg = 89.0f;
    if (s.fireRatePerSec <= 0.0f) s.fireRatePerSec = 0.001f;
    if (s.magazineSize < 1u) s.magazineSize = 1u;
    if (s.reloadTimeSec < 0.0f) s.reloadTimeSec = 0.0f;

    return s;
}

// =============================================================
// I/O de disco
// =============================================================

std::optional<Spec> Spec::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in.is_open()) {
        Log::engine()->warn("[WeaponSpec] no se pudo abrir '{}'",
                              fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[WeaponSpec] parse error en '{}': {}",
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
        Log::engine()->error("[WeaponSpec] no se pudo abrir '{}' para escritura",
                              fsPath.generic_string());
        return false;
    }
    try {
        out << toJson().dump(2);
    } catch (const std::exception& e) {
        Log::engine()->error("[WeaponSpec] error al escribir '{}': {}",
                              fsPath.generic_string(), e.what());
        return false;
    }
    return true;
}

} // namespace Mood::Weapon
