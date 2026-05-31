#pragma once

// EnemySpec (F4H7): asset .moodenemy — definicion declarativa de un
// enemigo. Lo consume el Inspector (autoria), el EnemyComponent (estado
// runtime) y el EnemySystem (state machine).
//
// Filosofia engine-grade: el motor NO conoce "grunt" / "imp" / "demon"
// como categorias hardcoded. Solo un set de campos numericos comunes a
// todos los enemigos basicos del genero FPS (HP, rangos, velocidad,
// daño, cooldowns). El dev del juego define su propia ontologia via
// `displayName` y crea N .moodenemy en `assets/enemies/`.
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "displayName": "Grunt",
//     "health": 50.0,
//
//     "aggroRange":  12.0,
//     "attackRange": 2.0,
//     "moveSpeed":   4.0,
//
//     "damage":           15.0,
//     "attackCooldown":   1.0,
//     "painThreshold":    10.0,
//     "painDuration":     0.3,
//
//     "viewmodelMesh": "meshes/enemies/grunt.glb",
//     "hitSound":      "sfx/grunt_hit.ogg",
//     "deathSound":    "sfx/grunt_death.ogg"
//   }
//
// Convenciones (no-enforcement del motor):
// - Asset refs son paths logicos (relativos a la raiz del proyecto).
// - `painThreshold` filtra hits que NO entran a Pain (hits de pellets
//   muy chicos no interrumpen el chase). Si dmg incoming >= threshold,
//   transition a Pain state.
// - F4H7 NO usa `moveSpeed` ni `damage` ni `attackCooldown` directamente
//   (Chase/Attack quedan no-op hasta F4H8/F4H9). Pero los campos viven
//   en el spec desde dia 1 para que F4H8/F4H9 no toquen el schema.
//
// Este modulo NO depende de ImGui ni del AssetManager — solo nlohmann.
// Es testeable en mood_tests sin contexto GL.

#include "core/Types.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace Mood::Enemy {

/// @brief Extension del filesystem para los archivos de enemigo.
inline constexpr const char* k_fileExtension = ".moodenemy";

/// @brief Spec declarativo del enemigo — datos puros, sin runtime.
class Spec {
public:
    Spec() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Campos -----

    std::string displayName;             // mostrado en HUD / Inspector

    // Salud
    f32 health         = 50.0f;          // HP iniciales (clamp >= 1)

    // Rangos
    f32 aggroRange     = 12.0f;          // metros — entra Alert si dist <= este
    f32 attackRange    = 2.0f;           // metros — entra Attack si dist <= este

    // Movimiento (F4H8 consume; F4H7 no-op)
    f32 moveSpeed      = 4.0f;           // m/s del chase

    // Ataque (F4H9 consume; F4H7 no-op)
    f32 damage         = 15.0f;          // HP por hit al player
    f32 attackCooldown = 1.0f;           // segundos entre golpes

    // Pain / stagger
    f32 painThreshold  = 10.0f;          // dmg minimo para entrar Pain
    f32 painDuration   = 0.3f;           // segundos del stagger

    // Asset refs (paths logicos; vacio = no asset)
    std::string viewmodelMesh;           // mesh del enemigo (Sub-fase 4.3)
    std::string hitSound;                // sonido on damage
    std::string deathSound;              // sonido on death

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + todos los campos).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna spec con defaults sanos + loggea warn. Clampea
    ///        valores fuera de rango.
    static Spec fromJson(const nlohmann::json& j);

    // ----- I/O de disco -----

    /// @brief Carga un asset desde un archivo `.moodenemy`. Retorna
    ///        nullopt si el archivo no existe / no parsea / schema
    ///        version incompatible. Loggea al canal `assets`.
    static std::optional<Spec> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el spec al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;
};

} // namespace Mood::Enemy
