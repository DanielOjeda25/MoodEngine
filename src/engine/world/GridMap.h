#pragma once

// Grilla 2D de tiles que se renderiza como cubos 3D ("Wolfenstein simulado").
// Cada celda es un u8 que mapea a `TileType`. El ancho y alto son en tiles
// (eje X y eje Z del mundo); el eje Y es constante (altura de pared = tileSize).
//
// Coordenadas:
//   - x crece hacia +X (derecha)
//   - y  -> aqui es el indice de fila, que en mundo mapea al eje Z
//   - origen (x=0, y=0) en el rincon -X / -Z del mundo
//
// El jugador camina sobre el plano XZ con Y=0 (nivel de piso).

#include "core/Types.h"
#include "core/math/AABB.h"

#include <vector>

namespace Mood {

/// @brief Tipos de tile posibles. Extendible: SolidWall2, Door, etc. entraran
///        en hitos posteriores cuando haya assets y comportamiento.
enum class TileType : u8 {
    Empty     = 0,
    SolidWall = 1,
};

class GridMap {
public:
    /// @brief Construye un mapa de width*height tiles, todos Empty. Default
    ///        tileSize = 3 m (convencion SI del motor, ver DECISIONS.md).
    GridMap(u32 width, u32 height, f32 tileSize = 3.0f);

    u32 width()    const { return m_width; }
    u32 height()   const { return m_height; }
    f32 tileSize() const { return m_tileSize; }

    /// @brief Devuelve el tipo de tile. Fuera de rango devuelve SolidWall
    ///        (los limites del mapa se tratan como paredes solidas).
    TileType tileAt(u32 x, u32 y) const;

    /// @brief Conveniencia: true si el tile es solido. Mismas reglas de borde
    ///        que tileAt (fuera del mapa = solido).
    bool isSolid(u32 x, u32 y) const;

    /// @brief Setea el tipo de un tile. Silenciosamente ignorado si (x,y)
    ///        esta fuera del mapa (nunca alocamos fuera).
    void setTile(u32 x, u32 y, TileType type);

    /// @brief AABB del cubo que ocupa el tile (x,y) en coordenadas de mundo.
    ///        Centro en (x*tileSize + tileSize/2, tileSize/2, y*tileSize + tileSize/2).
    AABB aabbOfTile(u32 x, u32 y) const;

    /// @brief Cuenta cuantos tiles solidos hay. Util para logging al cargar.
    usize solidCount() const;

private:
    u32 m_width;
    u32 m_height;
    f32 m_tileSize;
    std::vector<u8> m_tiles; // row-major: tiles[y * width + x]
};

} // namespace Mood
