#pragma once

// WeaponSpec (F4H2): asset .moodweapon — definicion declarativa de un
// arma. Lo consume el Weapon Browser (autoria), el WeaponComponent
// (instancias en mundo), el WeaponSystem (logica del disparo) y los
// bindings Lua weapon.* (scripting).
//
// Filosofia engine-grade: el motor NO conoce "shotgun" / "pistol" /
// "rifle" como categorias hardcoded. Solo `category` (string libre)
// + un set de campos numericos comunes a todas las armas hitscan/
// projectile. El dev del juego define su propia ontologia.
//
// El motor entiende SOLO `category == "hitscan"` en F4H2. Otros
// valores (`projectile`, `melee`, etc.) hacen no-op en el `fire`
// pero el asset se carga igual — para forward compat con hitos
// siguientes (F4H3+ proyectiles, F4H7+ melee).
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "displayName": "Escopeta",
//     "category": "hitscan",
//
//     "damage": 15.0,
//     "range": 25.0,
//     "pellets": 8,
//     "spreadDeg": 6.0,
//
//     "fireRatePerSec": 1.5,
//     "magazineSize": 6,
//     "reloadTimeSec": 1.8,
//
//     "viewmodelMesh": "viewmodel/shotgun.obj",
//     "viewmodelMaterial": "materials/shotgun.moodmaterial",
//
//     "fireSound": "sfx/shotgun_fire.ogg",
//     "impactSound": "sfx/bullet_impact.ogg",
//     "muzzleVfx": "vfx/muzzle_flash.moodvfx",
//     "impactVfx": "vfx/bullet_impact.moodvfx",
//
//     "ignoreOwner": true
//   }
//
// Convenciones (no-enforcement del motor):
// - Asset refs son paths logicos (relativos a la raiz del proyecto).
//   El AssetManager los resuelve via VFS en runtime; vacio = no asset.
// - `pellets == 1 && spreadDeg == 0` -> arma precisa (pistola).
// - `pellets > 1 || spreadDeg > 0`   -> arma con dispersion (escopeta).
// - Campos desconocidos en el JSON se ignoran (forward compat).
//
// Este modulo NO depende de ImGui ni del AssetManager — solo nlohmann.
// Es testeable en mood_tests sin contexto GL.

#include "core/Types.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace Mood::Weapon {

/// @brief Extension del filesystem para los archivos de arma.
inline constexpr const char* k_fileExtension = ".moodweapon";

/// @brief Spec declarativo del arma — datos puros, sin runtime.
class Spec {
public:
    Spec() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Campos (acceso directo — struct-like) -----

    std::string displayName;             // mostrado en HUD / Inspector
    std::string category = "hitscan";    // hitscan | projectile | melee | ...

    // Daño + alcance
    f32 damage    = 10.0f;               // dano por pellet (clamp >= 0)
    f32 range     = 50.0f;               // distancia maxima del raycast (m)
    u32 pellets   = 1;                   // raycasts por disparo (clamp >= 1)
    f32 spreadDeg = 0.0f;                // semi-angulo del cono de dispersion (deg)

    // Cadencia + munición
    f32 fireRatePerSec = 1.0f;           // disparos por segundo (clamp > 0)
    u32 magazineSize   = 10;             // capacidad del mag (clamp >= 1)
    f32 reloadTimeSec  = 1.0f;           // segundos del reload (clamp >= 0)

    // Asset refs (paths logicos; vacio = no asset)
    std::string viewmodelMesh;           // mesh del viewmodel (1ra persona)
    std::string viewmodelMaterial;       // material del viewmodel
    std::string fireSound;               // sonido al disparar
    std::string impactSound;             // sonido al impactar
    std::string muzzleVfx;               // particula en la boca del arma
    std::string impactVfx;               // particula en el punto de impacto

    // Comportamiento
    bool ignoreOwner = true;             // ignora el body del owner en el raycast

    // ----- F4H5: Proyectiles (solo aplica si category == "projectile") -----
    //
    // Bloque opcional. Si el JSON no trae `projectile`, los campos quedan
    // en defaults razonables (cubo placeholder + speed 20 m/s + splash 2m
    // + 30 dmg). Si category != "projectile", el bloque se ignora en runtime
    // (Weapon::fire decide por category, no por presencia del bloque).
    struct ProjectileParams {
        // Visual del proyectil flotando — fallback al missingMesh si vacio.
        std::string meshPath;
        std::string materialPath;

        // Cinematica.
        f32 speed         = 20.0f;       // m/s — vel inicial al spawn
        f32 gravity       = 0.0f;        // 0 = sin gravedad; >0 = arc-throw (granada)
        int bounceCount   = 0;           // 0 = no rebota; granada usa 3-4
        f32 bounceFactor  = 0.6f;        // coef de restitucion del rebote (0..1)
        f32 lifetimeSec   = 5.0f;        // si no impacta, explota al expirar

        // Damage.
        f32 directDamage  = 30.0f;       // a la entity con impacto directo
        f32 splashRadius  = 2.0f;        // metros del falloff lineal
        f32 splashDamage  = 30.0f;       // dmg en el centro; falloff lineal hasta 0 en borde
    };
    ProjectileParams projectile;

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + todos los campos).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna spec con defaults sanos + loggea warn. Clampea
    ///        valores fuera de rango (damage<0, fireRate<=0, pellets<1).
    static Spec fromJson(const nlohmann::json& j);

    // ----- I/O de disco -----

    /// @brief Carga un asset desde un archivo `.moodweapon`. Retorna
    ///        nullopt si el archivo no existe / no parsea / schema
    ///        version incompatible. Loggea al canal `assets`.
    static std::optional<Spec> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el spec al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;
};

} // namespace Mood::Weapon
