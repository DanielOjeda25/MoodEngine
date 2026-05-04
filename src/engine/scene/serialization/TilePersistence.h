#pragma once

// Helper para decidir si una entidad `Tile_X_Y` (creada por
// `rebuildSceneFromMap`) fue MODIFICADA por el user respecto a su estado
// default. Solo los tiles modificados se persisten al `.moodmap`; los
// no modificados se reconstruyen al cargar desde el `GridMap` para no
// inflar el JSON con datos triviales.
//
// "Modificado" = al menos UNA de las siguientes:
//   - Transform.scale != (tileSize, tileSize, tileSize)
//   - materials.size() != 1
//   - albedo del material != tileTextureAt(x, y) del grid
//   - tiene componentes que un tile default NO tiene
//     (Light, Script con path, ParticleEmitter, Trigger).
//
// NOTA: position NO se chequea — cambiar la posición de un tile saca al
// tile de su slot lógico del grid; si ese caso emerge, se trata como
// "tile suelto" via re-tag (fuera de scope v1).
//
// El helper es PURO (no toca ImGui ni GL). Testeado en
// `tests/test_tile_persistence.cpp`.

#include "core/Types.h"

namespace Mood {

class AssetManager;
class Entity;
class GridMap;

/// @brief Devuelve true si el tile fue modificado por el user y debe
///        persistirse al `.moodmap`. Devuelve false si el tile sigue
///        en su estado default (no necesita persistencia explícita).
///
///        Si el tag no parsea como "Tile_X_Y", devuelve true (defensivo:
///        mejor persistir de más que perder data).
bool isTileModified(Entity tile, const GridMap& map, const AssetManager& assets);

/// @brief Parsea el sufijo X_Y de un tag "Tile_X_Y" en `outX`/`outY`.
///        Devuelve true si el parseo fue exitoso. Sin "Tile_" como
///        prefijo o con dígitos inválidos → false.
bool parseTileCoords(const char* tag, u32& outX, u32& outY);

} // namespace Mood
