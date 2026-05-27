#include "engine/render/preview/AssetThumbnailDiskCache.h"

#include "core/Log.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <iomanip>
#include <sstream>

namespace Mood::AssetThumbnailDiskCache {

u64 hashLogicalPath(const std::string& logicalPath) {
    // FNV-1a 64-bit. Mismas constantes que LodCache (spec oficial).
    constexpr u64 k_fnvOffset = 0xcbf29ce484222325ull;
    constexpr u64 k_fnvPrime  = 0x00000100000001B3ull;
    u64 hash = k_fnvOffset;
    for (unsigned char c : logicalPath) {
        hash ^= c;
        hash *= k_fnvPrime;
    }
    return hash;
}

std::filesystem::path pathFor(const std::filesystem::path& cacheRoot,
                                const std::string& prefix,
                                const std::string& logicalPath,
                                u32 size) {
    const u64 h = hashLogicalPath(logicalPath);
    std::ostringstream oss;
    oss << prefix << "_"
        << std::hex << std::setw(16) << std::setfill('0') << h
        << "_" << std::dec << size << ".png";
    std::error_code ec;
    std::filesystem::create_directories(cacheRoot, ec);
    // Si fallo, igual devolvemos el path — el caller hara I/O y propagara
    // el error de manera natural.
    return cacheRoot / oss.str();
}

bool tryLoad(const std::filesystem::path& cachePath,
              const std::filesystem::path& assetSourcePath,
              std::vector<u8>& outRgba,
              u32& outW, u32& outH) {
    std::error_code ec;

    if (!std::filesystem::exists(cachePath, ec)) return false;

    // Mtime check: el cache vale si el PNG es al menos tan reciente como
    // el asset source. Si assetSourcePath esta vacio o no existe (path
    // resuelto a algo invalido), asumimos valido — el caller decide.
    if (!assetSourcePath.empty() &&
        std::filesystem::exists(assetSourcePath, ec)) {
        const auto cacheTime  = std::filesystem::last_write_time(cachePath, ec);
        if (ec) return false;
        const auto sourceTime = std::filesystem::last_write_time(assetSourcePath, ec);
        if (ec) return false;
        if (cacheTime < sourceTime) return false;  // stale
    }

    int w = 0, h = 0, comps = 0;
    // Garantizar GL convention (origin bottom-left): los renderers uploadean
    // los bytes directo a una FBO color texture que luego ImGui muestra con
    // uv flip (0,1)-(1,0). El flag global de stbi suele estar en true (lo
    // setea OpenGLTexture), pero lo forzamos explicito por si algun load
    // previo lo dejo en false.
    stbi_set_flip_vertically_on_load(true);
    // Forzar RGBA para upload directo a GL como GL_RGBA / GL_UNSIGNED_BYTE.
    u8* pixels = stbi_load(cachePath.string().c_str(), &w, &h, &comps, 4);
    if (pixels == nullptr) {
        Log::engine()->warn("[thumb-cache] PNG corrupto '{}' — regenerar",
                              cachePath.generic_string());
        return false;
    }

    outRgba.assign(pixels, pixels + (w * h * 4));
    outW = static_cast<u32>(w);
    outH = static_cast<u32>(h);
    stbi_image_free(pixels);
    return true;
}

bool store(const std::filesystem::path& cachePath,
            const u8* rgba, u32 w, u32 h) {
    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (ec) {
        Log::engine()->warn("[thumb-cache] no se pudo crear '{}': {}",
                              cachePath.parent_path().generic_string(),
                              ec.message());
        return false;
    }

    // stride = w * 4 (RGBA8 packed). PNG default compression.
    const int ok = stbi_write_png(
        cachePath.string().c_str(),
        static_cast<int>(w),
        static_cast<int>(h),
        4,
        rgba,
        static_cast<int>(w) * 4);

    if (!ok) {
        Log::engine()->warn("[thumb-cache] stbi_write_png fallo en '{}'",
                              cachePath.generic_string());
        return false;
    }
    return true;
}

} // namespace Mood::AssetThumbnailDiskCache
