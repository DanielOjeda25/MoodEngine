#pragma once

// F3H18: Validador de assets rotos. Escaneo lateral (no en hot path) que
// recorre la Scene + el cache de Materials del AssetManager y reporta
// referencias que no resuelven en disco o que apuntan al fallback "missing"
// del AssetManager (load failed silencioso). Pensado para que el dev abra
// un proyecto que estuvo dormido o que recibio cambios externos y vea
// inmediatamente que falto referencia mover, renombrar o restaurar.
//
// Cobertura inicial (Tier 1):
//   - Refs muertas: campos `path` string que no existen en disco
//     (ScriptComponent / DialogComponent / ItemPickupComponent /
//     VehicleComponent / EnvironmentComponent skyboxPath).
//   - Refs muertas indirectas via AssetId: el id no es 0 pero su
//     `pathOf(id)` resuelve a un path que no existe (texturas en
//     MaterialAsset, meshes y materials de MeshRenderer/Brush, audio
//     clip de AudioSource, animation clips de Animator, texture de
//     ParticleEmitter).
//
// Diferidos (futuros hitos):
//   - Schema mismatch (`.moodmap` viejo sin upgrader).
//   - Oversized files (> N MB configurable).
//   - Refs muertas dentro de Materials a shadergraphs.
//   - Refs muertas en Prefabs cacheados (sin spawnear).

#include "core/Types.h"
#include "engine/scene/core/Entity.h"

#include <string>
#include <vector>

namespace Mood {

class Scene;
class AssetManager;

namespace asset_validation {

/// @brief Categoria del issue. Determina el icon + el filtro de UI.
enum class IssueKind : u8 {
    /// @brief Referencia a un asset cuyo path NO existe en disco
    ///        (renombrado, movido, borrado externamente).
    BrokenRef = 0,
    /// @brief Referencia que el AssetManager resolvio al fallback
    ///        (slot 0 / __missing_X) — el archivo existe pero el load
    ///        en runtime fallo (corrupto, formato no soportado, etc).
    LoadFailed = 1,
};

/// @brief Un issue detectado por el validator. Pensado para listing UI:
///        `assetPath` se muestra como titulo, `detail` como explicacion,
///        `usedBy` indica quien refiere al asset roto, y `entity` permite
///        "go to" al click (seleccionar en Hierarchy/Inspector).
struct AssetIssue {
    IssueKind kind = IssueKind::BrokenRef;

    /// @brief Path logico que no resuelve (ej. "textures/missing_grid.png").
    ///        Si la fuente es un componente con AssetId, se llena con
    ///        `pathOf(id)`. Si el path original se perdio (id apunta a
    ///        slot 0 sin string asociado), queda vacio.
    std::string assetPath;

    /// @brief Descripcion humana del problema (i18n key resuelto). Ej.
    ///        "Script no encontrado", "Material referencia textura
    ///        borrada", etc.
    std::string detail;

    /// @brief Quien refiere al asset roto. Para entities: tag.name (o
    ///        "(sin tag)"). Para Materials: "Material: <path>".
    std::string usedBy;

    /// @brief Entity fuente del issue (si aplica). Falsy cuando el issue
    ///        viene de un Material cacheado sin entity asociada. Permite
    ///        al panel "go to" la entity en Hierarchy + Inspector.
    Entity entity{};
};

/// @brief Recorre la Scene + el cache de Materials del AssetManager y
///        devuelve la lista de issues detectados. Pensado para ejecutarse
///        on-demand (al abrir proyecto, F5 en el panel, post-rename).
///        Costo: O(entities + materials) — tipicamente < 5 ms para
///        proyectos medianos. NO mantiene cache; cada llamada re-escanea.
///
///        El orden del output es estable: primero issues por entity (en
///        orden de iteracion del registry), luego issues por material (en
///        orden de slot del AssetManager). Stable order = la UI no
///        re-scrollea cuando el dev resuelve uno.
std::vector<AssetIssue> validateProject(Scene& scene,
                                          const AssetManager& assets);

}  // namespace asset_validation
}  // namespace Mood
