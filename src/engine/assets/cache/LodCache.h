#pragma once

// Cache en disco de los LODs generados por meshoptimizer (F2H6).
// Vive lateral al formato `.moodmap` — borrarlo NO rompe el proyecto, solo
// fuerza re-generacion la proxima vez que se cargue el mesh.
//
// Layout:
//   assets/.cache/lods/<hash>.moodlod
// donde `<hash>` es FNV-1a 64-bit hex del logical path del mesh
// (ej. "meshes/Fox.glb" -> "1a2b3c4d5e6f7890.moodlod").
//
// Formato binario little-endian:
//   - magic                  : 4 bytes ASCII "MLOD"
//   - version                : u32 (=1)
//   - source_mtime_ns        : u64 (filesystem mtime del source en nanosegundos)
//   - source_size            : u64 (bytes del source — segundo check)
//   - num_lods               : u32 (1 o 2 — siempre se cachean ambos juntos)
//   - per cada LOD:
//       - num_submeshes      : u32
//       - per submesh:
//           - vertex_count   : u32
//           - material_index : u32
//           - data           : f32[vertex_count * 19]  (layout estandar)
//
// El layout de 19 floats es el mismo del MeshLoader: pos(3) + color(3) +
// uv(2) + normal(3) + boneIds(4) + boneWeights(4). Para meshes sin
// esqueleto, los 8 floats finales van en 0.

#include "core/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Mood::LodCache {

/// @brief Una "vista" en CPU de un submesh listo para uploadear como
///        IMesh. Contiene los floats interleaved + metadatos.
struct LodSubmeshData {
    std::vector<f32> vertices;  // size = vertexCount * 19
    u32 vertexCount{0};
    u32 materialIndex{0};
};

/// @brief Resultado de cargar un cache hit: las listas de submeshes
///        para LOD 1 y LOD 2 (ambos pueden ser vacios si el mesh
///        original no tenia LODs generables).
struct LodCacheEntry {
    std::vector<LodSubmeshData> lod1;
    std::vector<LodSubmeshData> lod2;
};

/// @brief Path del cache para un logical path dado. Crea el directorio
///        padre si no existe.
std::filesystem::path pathFor(const std::string& logicalPath);

/// @brief FNV-1a 64-bit del input. Estable y rapido. Usado como nombre
///        del archivo de cache.
u64 hashLogicalPath(const std::string& logicalPath);

/// @brief Intenta cargar el cache. Devuelve true si el archivo existe,
///        tiene magic + version validos, y los metadatos de source
///        (mtime + size) coinciden con los pasados. False = miss
///        (archivo no existe, corrupto, o source cambio).
///
///        Si false, el `out` no se toca.
bool tryLoad(const std::filesystem::path& cachePath,
              u64 sourceMtimeNs,
              u64 sourceSizeBytes,
              LodCacheEntry& out);

/// @brief Escribe el cache. Sobrescribe si existe. Lanza en errores
///        de I/O — el caller decide si es fatal o solo warning.
void save(const std::filesystem::path& cachePath,
           u64 sourceMtimeNs,
           u64 sourceSizeBytes,
           const LodCacheEntry& entry);

} // namespace Mood::LodCache
