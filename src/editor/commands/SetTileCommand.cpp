#include "editor/commands/SetTileCommand.h"

#include <utility>

namespace Mood {

SetTileCommand::SetTileCommand(
    GridMap* map,
    TileSyncFn sync,
    u32 x, u32 y,
    TileType oldType, TextureAssetId oldTex,
    TileType newType, TextureAssetId newTex,
    std::string label)
    : m_map(map),
      m_sync(std::move(sync)),
      m_x(x), m_y(y),
      m_oldType(oldType), m_newType(newType),
      m_oldTex(oldTex), m_newTex(newTex),
      m_label(std::move(label)) {}

void SetTileCommand::execute() {
    apply(m_newType, m_newTex);
}

void SetTileCommand::undo() {
    apply(m_oldType, m_oldTex);
}

void SetTileCommand::apply(TileType type, TextureAssetId tex) {
    if (m_map == nullptr) return;
    m_map->setTile(m_x, m_y, type, tex);
    if (m_sync) {
        m_sync(m_x, m_y, tex);
    }
}

} // namespace Mood
