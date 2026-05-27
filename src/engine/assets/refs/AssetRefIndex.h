#pragma once

// F3H19: indice de referencias a un asset path. Pensado para alimentar
// el rename con cascada (`RenameAssetCommand`). Reusa el patron de walk
// del AssetValidator (F3H18) pero acumula RefSite especificos: NO solo
// "esta roto", sino "esta ref vive aca y como reemplazarla".
//
// Cobertura:
//   - String paths en componentes:
//       ScriptComponent.path
//       DialogComponent.dialogPath
//       ItemPickupComponent.itemPath
//       VehicleComponent.configPath
//       EnvironmentComponent.skyboxPath
//       EnvironmentComponent.colorGradingLutPath
//       PrefabLinkComponent.path
//   - AssetId indirectos (la ref vive como id pero apunta al path):
//       AudioSourceComponent.clip
//       MeshRendererComponent.mesh
//       MeshRendererComponent.materials[slot]
//       AnimatorComponent.externalClips[alias]
//       ParticleEmitterComponent.texture
//       BrushComponent.materials[slot]
//   - Texturas referenciadas por Material cargados:
//       MaterialAsset.albedo / metallicRoughness / normal / ao

#include "core/Types.h"
#include "engine/scene/core/Entity.h"

#include <string>
#include <vector>

namespace Mood {

class Scene;
class AssetManager;

namespace asset_refs {

/// @brief Qué tipo de ref encontramos. Determina cómo reemplazar (rename) el
///        path: las refs string-path se reescriben directo en el campo; las
///        refs id-based no se reescriben (el AssetManager mantiene el path
///        interno y al renombrarlo todas las refs por id siguen apuntando
///        bien). Las refs Material*-Texture se reescriben tocando el campo
///        del MaterialAsset cacheado.
enum class RefKind : u8 {
    // ----- string path en componentes (necesita reescritura directa) -----
    ScriptPath = 0,
    DialogPath,
    ItemPath,
    VehiclePath,
    SkyboxPath,
    ColorGradingLutPath,
    PrefabLinkPath,

    // ----- id-based en componentes (el path vive en el AssetManager) -----
    AudioClipId,
    MeshId,
    MeshMaterialSlot,
    AnimatorExternalClip,
    ParticleTextureId,
    BrushMaterialSlot,

    // ----- texturas referenciadas por MaterialAsset cargado -----
    MaterialAlbedo,
    MaterialMetallicRoughness,
    MaterialNormal,
    MaterialAO,
};

/// @brief Un site individual donde un asset path es referenciado. Pensado
///        para que el caller (RenameAssetCommand) sepa exactamente qué
///        campo reescribir + para que la UI muestre una lista clara al dev
///        antes de confirmar el rename.
struct RefSite {
    RefKind kind = RefKind::ScriptPath;

    /// @brief Entity fuente del RefSite. Falsy cuando el ref vive en un
    ///        Material cacheado (RefKind::Material*Texture) — `materialPath`
    ///        lleva el path del material.
    Entity entity{};

    /// @brief Path del Material que contiene la ref. Solo valido para
    ///        RefKind::MaterialAlbedo / MetallicRoughness / Normal / AO.
    std::string materialPath;

    /// @brief Para refs en arrays (MeshRenderer.materials[N], Brush.materials[N]):
    ///        el indice del slot. 0 en el resto de los casos.
    usize slotIndex = 0;

    /// @brief Para AnimatorComponent.externalClips: el alias del clip
    ///        (key del map). Vacio en el resto de los casos.
    std::string animAlias;
};

/// @brief Normaliza un path lógico (forward slashes, sin prefijo "assets/",
///        case lowercased solo en la EXTENSION). Útil para comparar paths
///        en el index. Si querés comparación case-insensitive completa,
///        debe hacerse upstream por el caller.
std::string normalizePath(const std::string& path);

/// @brief Devuelve todas las refs a `assetLogicalPath` en la Scene + los
///        Materials cargados. El path debe estar normalizado a forward
///        slashes y SIN prefijo "assets/". Comparación case-sensitive
///        (assumimos consistencia del codebase — paths se guardan como
///        el dev los escribe).
///
///        Costo: O(entities + materials), mismo orden que el AssetValidator.
///        Stable order: primero refs por entity (orden del registry), luego
///        refs por material (orden de slot del AssetManager).
std::vector<RefSite> findRefs(Scene& scene, const AssetManager& assets,
                                const std::string& assetLogicalPath);

}  // namespace asset_refs
}  // namespace Mood
