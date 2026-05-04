#include "engine/assets/cache/LodCache.h"

#include "core/Log.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Mood::LodCache {

namespace {

// Valor del header del archivo de cache.
constexpr char k_magic[4] = {'M', 'L', 'O', 'D'};
constexpr u32 k_version = 1;
// Layout estandar del MeshLoader. Si cambia (bone count, color removed,
// tangents agregados), bumpear k_version para invalidar caches viejos.
constexpr u32 k_strideFloats = 19;

// Lee N bytes a un buffer; devuelve false si EOF antes de tiempo.
bool readBytes(std::istream& is, void* buffer, size_t n) {
    is.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(n));
    return is.good() && static_cast<size_t>(is.gcount()) == n;
}

// Escribe N bytes desde un buffer.
void writeBytes(std::ostream& os, const void* buffer, size_t n) {
    os.write(reinterpret_cast<const char*>(buffer),
              static_cast<std::streamsize>(n));
}

bool readSubmeshList(std::ifstream& is, std::vector<LodSubmeshData>& out) {
    u32 count = 0;
    if (!readBytes(is, &count, sizeof(count))) return false;
    out.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        LodSubmeshData sm;
        if (!readBytes(is, &sm.vertexCount, sizeof(sm.vertexCount))) return false;
        if (!readBytes(is, &sm.materialIndex, sizeof(sm.materialIndex))) return false;
        const u64 floatsToRead =
            static_cast<u64>(sm.vertexCount) * k_strideFloats;
        // Sanity: 100M floats = 400MB. Mucho mas suena a archivo corrupto.
        if (floatsToRead > 100'000'000ull) {
            Log::assets()->warn(
                "[lod cache] vertexCount * stride = {} suena corrupto",
                floatsToRead);
            return false;
        }
        sm.vertices.resize(floatsToRead);
        if (!readBytes(is, sm.vertices.data(),
                        floatsToRead * sizeof(f32))) return false;
        out.push_back(std::move(sm));
    }
    return true;
}

void writeSubmeshList(std::ofstream& os,
                       const std::vector<LodSubmeshData>& list) {
    const u32 count = static_cast<u32>(list.size());
    writeBytes(os, &count, sizeof(count));
    for (const auto& sm : list) {
        writeBytes(os, &sm.vertexCount, sizeof(sm.vertexCount));
        writeBytes(os, &sm.materialIndex, sizeof(sm.materialIndex));
        writeBytes(os, sm.vertices.data(),
                    sm.vertices.size() * sizeof(f32));
    }
}

} // namespace

u64 hashLogicalPath(const std::string& logicalPath) {
    // FNV-1a 64-bit. Constants: offset basis + prime de la spec.
    constexpr u64 k_fnvOffset = 0xcbf29ce484222325ull;
    constexpr u64 k_fnvPrime  = 0x00000100000001B3ull;
    u64 hash = k_fnvOffset;
    for (unsigned char c : logicalPath) {
        hash ^= c;
        hash *= k_fnvPrime;
    }
    return hash;
}

std::filesystem::path pathFor(const std::string& logicalPath) {
    const u64 h = hashLogicalPath(logicalPath);
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    const auto dir = std::filesystem::path("assets") / ".cache" / "lods";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    // Si fallo crear el dir, igual devolvemos el path — el caller hara
    // I/O y propagara el error de manera natural.
    return dir / (oss.str() + ".moodlod");
}

bool tryLoad(const std::filesystem::path& cachePath,
              u64 sourceMtimeNs,
              u64 sourceSizeBytes,
              LodCacheEntry& out) {
    std::ifstream is(cachePath, std::ios::binary);
    if (!is.is_open()) return false;

    char magic[4];
    if (!readBytes(is, magic, 4)) return false;
    if (std::memcmp(magic, k_magic, 4) != 0) {
        Log::assets()->warn("[lod cache] magic invalido en '{}'",
                              cachePath.generic_string());
        return false;
    }

    u32 version = 0;
    if (!readBytes(is, &version, sizeof(version))) return false;
    if (version != k_version) return false; // version mismatch = miss silent

    u64 cachedMtime = 0;
    u64 cachedSize  = 0;
    if (!readBytes(is, &cachedMtime, sizeof(cachedMtime))) return false;
    if (!readBytes(is, &cachedSize,  sizeof(cachedSize)))  return false;
    if (cachedMtime != sourceMtimeNs || cachedSize != sourceSizeBytes) {
        // Source cambio — re-generar.
        return false;
    }

    u32 numLods = 0;
    if (!readBytes(is, &numLods, sizeof(numLods))) return false;
    if (numLods != 2) return false; // siempre cacheamos LOD1 + LOD2 juntos

    if (!readSubmeshList(is, out.lod1)) return false;
    if (!readSubmeshList(is, out.lod2)) return false;
    return true;
}

void save(const std::filesystem::path& cachePath,
           u64 sourceMtimeNs,
           u64 sourceSizeBytes,
           const LodCacheEntry& entry) {
    std::ofstream os(cachePath, std::ios::binary | std::ios::trunc);
    if (!os.is_open()) {
        Log::assets()->warn("[lod cache] no se pudo abrir '{}' para escribir",
                              cachePath.generic_string());
        return;
    }
    writeBytes(os, k_magic, 4);
    writeBytes(os, &k_version, sizeof(k_version));
    writeBytes(os, &sourceMtimeNs,    sizeof(sourceMtimeNs));
    writeBytes(os, &sourceSizeBytes,  sizeof(sourceSizeBytes));
    const u32 numLods = 2;
    writeBytes(os, &numLods, sizeof(numLods));
    writeSubmeshList(os, entry.lod1);
    writeSubmeshList(os, entry.lod2);
}

} // namespace Mood::LodCache
