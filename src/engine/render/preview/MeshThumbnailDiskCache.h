#pragma once

// F3H14: cache en disco de los thumbnails de meshes generados por el
// MeshThumbnailRenderer. Persistente entre sesiones — al reabrir el editor,
// los PNGs en `<proyecto>/.cache/thumbs/` se cargan en vez de re-rendear
// todos los meshes (overhead 1-2s con muchos meshes pasaba en F2H80).
//
// Layout:
//   <projectRoot>/.cache/thumbs/mesh_<hash>_<size>.png
//
// donde `<hash>` es FNV-1a 64-bit hex del logical path del mesh
// (ej. "meshes/Fox.glb" -> "1a2b3c4d5e6f7890") y `<size>` es la resolucion
// (ej. "128"). Incluir el size en el filename hace que un cambio de
// resolucion en UserSettings NO invalide thumbs viejos — coexisten en
// disco hasta que se borre la cache manualmente.
//
// Invalidacion por mtime: tryLoad chequea que `last_write_time(cachePng)
// >= last_write_time(meshSource)`. Si el .moodmesh/.glb/.fbx se modifico
// despues del PNG, miss -> regen. Sin sidecar `.meta` — el mtime del
// propio PNG basta.

#include "core/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Mood::MeshThumbnailDiskCache {

/// @brief FNV-1a 64-bit del input. Mismo algoritmo que LodCache —
///        estable y rapido, no criptografico.
u64 hashLogicalPath(const std::string& logicalPath);

/// @brief Path del PNG en disco para un (logicalPath, size, cacheRoot).
///        Crea el directorio padre si no existe (no falla si ya esta).
///        `cacheRoot` tipicamente `<projectRoot>/.cache/thumbs/`.
std::filesystem::path pathFor(const std::filesystem::path& cacheRoot,
                                const std::string& logicalPath,
                                u32 size);

/// @brief Carga un thumbnail cacheado si esta valido (existe + mtime
///        del PNG >= mtime del mesh source). Devuelve true en HIT,
///        false en MISS (archivo no existe, mtime stale, o PNG
///        corrupto). En HIT llena `outRgba` con bytes RGBA8 + `outW`,
///        `outH` con las dimensiones leidas.
///
///        Si `meshSourcePath` esta vacio o no existe, asume que el
///        cache es valido (no podemos comparar mtimes) — el caller
///        decide si querer regenerar.
bool tryLoad(const std::filesystem::path& cachePath,
              const std::filesystem::path& meshSourcePath,
              std::vector<u8>& outRgba,
              u32& outW, u32& outH);

/// @brief Escribe el thumbnail a disco como PNG RGBA8. Sobrescribe si
///        existe. Devuelve true en exito, false si stbi_write_png
///        fallo o no se pudo crear el directorio. No-fatal — el caller
///        sigue mostrando el thumb (lo tiene en GPU); solo se pierde
///        la persistencia entre sesiones.
bool store(const std::filesystem::path& cachePath,
            const u8* rgba, u32 w, u32 h);

} // namespace Mood::MeshThumbnailDiskCache
