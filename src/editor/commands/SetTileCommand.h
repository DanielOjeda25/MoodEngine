#pragma once

// F2H16: comando undoable para cambios de tile en el GridMap
// (drop de textura sobre el suelo, o futuras herramientas de
// pintado de tiles).
//
// Snapshot por (x, y) capturando TileType + TextureAssetId pre.
// La entidad-tile correspondiente se sincroniza via callback.

#include "editor/commands/Command.h"
#include "engine/assets/manager/AssetManager.h"  // TextureAssetId
#include "engine/world/grid/GridMap.h"           // TileType

#include <functional>
#include <string>

namespace Mood {

class GridMap;

class SetTileCommand : public ICommand {
public:
    /// @brief Callback invocado tras setTile en execute/undo para
    ///        sincronizar la entidad-tile en el Scene. Firma:
    ///        `void(x, y, texture)`. EditorApplication pasa
    ///        `&EditorApplication::updateTileEntity` bind.
    using TileSyncFn = std::function<void(u32, u32, TextureAssetId)>;

    SetTileCommand(GridMap* map,
                    TileSyncFn sync,
                    u32 x, u32 y,
                    TileType oldType, TextureAssetId oldTex,
                    TileType newType, TextureAssetId newTex,
                    std::string label = "Pintar tile");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    GridMap* m_map;
    TileSyncFn m_sync;
    u32 m_x, m_y;
    TileType m_oldType, m_newType;
    TextureAssetId m_oldTex, m_newTex;
    std::string m_label;

    void apply(TileType type, TextureAssetId tex);
};

} // namespace Mood
