#include "engine/world/GridMap.h"

namespace Mood {

GridMap::GridMap(u32 width, u32 height, f32 tileSize)
    : m_width(width),
      m_height(height),
      m_tileSize(tileSize),
      m_tiles(static_cast<usize>(width) * height, static_cast<u8>(TileType::Empty)) {}

TileType GridMap::tileAt(u32 x, u32 y) const {
    if (x >= m_width || y >= m_height) {
        return TileType::SolidWall;
    }
    return static_cast<TileType>(m_tiles[y * m_width + x]);
}

bool GridMap::isSolid(u32 x, u32 y) const {
    return tileAt(x, y) == TileType::SolidWall;
}

void GridMap::setTile(u32 x, u32 y, TileType type) {
    if (x >= m_width || y >= m_height) return;
    m_tiles[y * m_width + x] = static_cast<u8>(type);
}

AABB GridMap::aabbOfTile(u32 x, u32 y) const {
    const f32 fx = static_cast<f32>(x) * m_tileSize;
    const f32 fz = static_cast<f32>(y) * m_tileSize;
    return AABB{
        glm::vec3(fx,            0.0f,      fz),
        glm::vec3(fx + m_tileSize, m_tileSize, fz + m_tileSize),
    };
}

usize GridMap::solidCount() const {
    usize n = 0;
    for (u8 t : m_tiles) {
        if (static_cast<TileType>(t) == TileType::SolidWall) ++n;
    }
    return n;
}

} // namespace Mood
