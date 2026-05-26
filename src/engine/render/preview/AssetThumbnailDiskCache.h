#pragma once

// F3H14: cache en disco de thumbnails de assets generados off-screen
// (meshes desde F3H14, materiales desde F3H15). Persistente entre
// sesiones — al reabrir el editor, los PNGs en `<proyecto>/.cache/thumbs/`
// se cargan en vez de re-rendear cada asset.
//
// Layout:
//   <projectRoot>/.cache/thumbs/<prefix>_<hash>_<size>.png
//
// donde `<prefix>` es el tipo de asset (ej. "mesh", "mat") usado para
// diferenciar caches en el mismo directorio, `<hash>` es FNV-1a 64-bit
// hex del logical path (ej. "meshes/Fox.glb" -> "1a2b3c4d5e6f7890") y
// `<size>` es la resolucion (ej. "128"). Incluir el size en el filename
// hace que un cambio de resolucion en UserSettings NO invalide thumbs
// viejos — coexisten en disco hasta que se borre la cache manualmente.
//
// Invalidacion por mtime: tryLoad chequea que `last_write_time(cachePng)
// >= last_write_time(assetSource)`. Si el source (.moodmesh/.material/etc)
// se modifico despues del PNG, miss -> regen. Sin sidecar `.meta` — el
// mtime del propio PNG basta.

#include "core/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Mood::AssetThumbnailDiskCache {

/// @brief FNV-1a 64-bit del input. Mismo algoritmo que LodCache —
///        estable y rapido, no criptografico.
u64 hashLogicalPath(const std::string& logicalPath);

/// @brief Path del PNG en disco para un (prefix, logicalPath, size, root).
///        Crea el directorio padre si no existe (no falla si ya esta).
///        `cacheRoot` tipicamente `<projectRoot>/.cache/thumbs/`,
///        `prefix` es "mesh"/"mat"/etc para discriminar en el mismo dir.
std::filesystem::path pathFor(const std::filesystem::path& cacheRoot,
                                const std::string& prefix,
                                const std::string& logicalPath,
                                u32 size);

/// @brief Carga un thumbnail cacheado si esta valido (existe + mtime
///        del PNG >= mtime del asset source). Devuelve true en HIT,
///        false en MISS (archivo no existe, mtime stale, o PNG
///        corrupto). En HIT llena `outRgba` con bytes RGBA8 + `outW`,
///        `outH` con las dimensiones leidas.
///
///        Si `assetSourcePath` esta vacio o no existe, asume que el
///        cache es valido (no podemos comparar mtimes) — el caller
///        decide si querer regenerar.
bool tryLoad(const std::filesystem::path& cachePath,
              const std::filesystem::path& assetSourcePath,
              std::vector<u8>& outRgba,
              u32& outW, u32& outH);

/// @brief Escribe el thumbnail a disco como PNG RGBA8. Sobrescribe si
///        existe. Devuelve true en exito, false si stbi_write_png
///        fallo o no se pudo crear el directorio. No-fatal — el caller
///        sigue mostrando el thumb (lo tiene en GPU); solo se pierde
///        la persistencia entre sesiones.
bool store(const std::filesystem::path& cachePath,
            const u8* rgba, u32 w, u32 h);

} // namespace Mood::AssetThumbnailDiskCache
