#pragma once

// MaterialAsset (Hito 17): describe un material PBR metallic-roughness.
//
// Convencion glTF para la textura "MR packed" (un solo PNG con 3 canales
// usados):
//   R = ambient occlusion (opcional; si no hay AO map, se ignora)
//   G = roughness
//   B = metallic
// Esta convencion permite combinar AO + roughness + metallic en una sola
// textura, ahorrando samples + memoria. Si una textura no esta presente
// (path vacio), el shader usa el fallback escalar correspondiente.
//
// Ownership: las texturas referenciadas viven en `AssetManager` (el material
// solo guarda los `TextureAssetId`). El AssetManager catalogue las cargo al
// resolver el `.material` JSON.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

using TextureAssetId = u32;

/// F2H63: modos de blending del material. Convencion industria
/// (Unity URP / Unreal Material `Blend Mode` / Godot StandardMaterial3D).
///   - **Opaque**: blending disabled, depth-write enabled. Path default.
///   - **Translucent**: SRC_ALPHA / ONE_MINUS_SRC_ALPHA, depth-test ON,
///     depth-write OFF. Sort back-to-front (F2H63) -> OIT (F2H64). El
///     mas comun para vidrios, agua, hologramas.
///   - **Additive**: SRC_ALPHA / ONE, depth-test ON, depth-write OFF.
///     Para emissive (glow, fuego, magia). No sortea (commutative).
enum class BlendMode : u8 {
    Opaque      = 0,
    Translucent = 1,
    Additive    = 2,
};

struct MaterialAsset {
    /// Path logico usado por el serializer (p.ej. "materials/brass.material").
    std::string logicalPath;

    // --- Texturas (slots; 0 = no asignada) ---
    /// Albedo (color base). Si no hay textura, se usa solo `albedoTint`.
    TextureAssetId albedo = 0;
    /// Metallic-Roughness packed (G=rough, B=metal). Si no hay textura, se
    /// usan los multiplicadores escalares.
    TextureAssetId metallicRoughness = 0;
    /// Normal map en tangent space. Si no hay, se usa la normal del vertex.
    TextureAssetId normal = 0;
    /// Ambient occlusion. Opcional; si esta seteada gana sobre el canal R
    /// del MR packed (si tambien hubiera AO ahi).
    TextureAssetId ao = 0;

    // --- Multiplicadores escalares ---
    /// Tinte multiplicativo del albedo (ej. (1,1,1) = sin tinte).
    glm::vec3 albedoTint{1.0f};
    /// Multiplicador del canal metallic. Cuando no hay textura MR, este es
    /// el valor final usado por el shader.
    f32 metallicMult = 0.0f;
    /// Multiplicador del canal roughness.
    f32 roughnessMult = 0.5f;
    /// Multiplicador del canal AO (1.0 = sin oclusion).
    f32 aoMult = 1.0f;

    /// Indica al renderer si debe samplear `albedo`. Distingue "material
    /// quiere tint puro" (false, ej. oro_pulido sin textura) de "material
    /// pide warning visual" (true sobre `albedo=0` => muestra el patron
    /// magenta de missing.png en el material default cuando una entidad
    /// no tiene material asignado).
    bool useAlbedoMap = false;

    /// F2H62 Bloque D: path logico a un `.moodshader` que reemplaza el
    /// shader PBR estandar para este material. Empty = usar pbr.frag
    /// del motor (camino default). Cuando esta presente, el SceneRenderer
    /// compila el shader graph + cachea el resultado y lo usa para
    /// drawear las entidades con este material. La compilacion on-the-fly
    /// + cache + fallback al PBR si falla son responsabilidad del Bloque E.
    std::string shaderGraphPath;

    // --- F2H63: transparencia + refraccion ---

    /// Modo de blending. `Opaque` = path tradicional (default, back-compat).
    /// `Translucent` / `Additive` activan el translucent pass del SceneRenderer.
    BlendMode blendMode = BlendMode::Opaque;

    /// Alpha base del material (0-1). Solo aplica si `blendMode != Opaque`.
    /// El shader combina este escalar con un Fresnel automatico (bordes mas
    /// opacos, frente mas transparente) para dar el look "vidrio real".
    f32 opacity = 1.0f;

    /// Indice de refraccion (1.0 = sin refraccion). Presets fisicos:
    ///   Aire = 1.00, Agua = 1.33, Vidrio = 1.50, Diamante = 2.42.
    /// Solo aplica a `Translucent` (no a Additive). El shader convierte
    /// (ior - 1.0) en magnitud de offset screen-space sobre el backbuffer.
    f32 ior = 1.0f;

    /// Multiplicador del offset de refraccion (0-1). 0 = sin distorsion
    /// (igual que IOR=1). Permite "refraccion suave" en materiales con
    /// IOR fisico alto sin distorsionar tanto. Solo `Translucent`.
    f32 refractionStrength = 0.0f;
};

} // namespace Mood
