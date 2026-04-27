#pragma once

// Forward+ light grid (Hito 18). CPU-side: para cada point light de la
// escena, calcula a que tiles de la pantalla afecta y arma una flat
// list de indices que el shader leera por tile.
//
// Tile size: 16x16 px. Para un viewport 1280x720 son 80x45 = 3600 tiles.
// Por luz se calcula su AABB en screen-space (proyectando el bounding
// sphere = posicion + radius), se clampa al viewport, y se marcan los
// tiles que cubre. La cantidad esperada de tiles por luz es chica (las
// luces no afectan toda la pantalla excepto en zoom-out total), asi que
// el tamano del flat array de indices crece linealmente con N_luces.
//
// Output (consumido por SSBOs en el render pass):
//   - tileData[tilesX * tilesY]: por tile, (offset, count) en lightIndices.
//   - lightIndices[]: flat list, indices al array de PointLights.
//
// La directional NO entra al grid: se sigue subiendo como uniform escalar
// (no tiene radius — afecta toda la pantalla por definicion). Cada
// fragment evalua la directional siempre + las point lights de su tile.

#include "core/Types.h"
#include "systems/LightSystem.h" // LightFrameData, PointLightData

#include <glm/mat4x4.hpp>

#include <vector>

namespace Mood {

/// @brief Tile size en pixeles. Constante para que CPU y shader compartan
///        la division. 16x16 es el sweet spot empirico de las refs
///        (Forward+ original Harada 2012 + UE/Unity).
constexpr u32 k_LightGridTileSize = 16u;

/// @brief Por tile, offset+count en el flat array `lightIndices`.
///        Layout std430-friendly (uvec2 == 8 bytes alineado).
struct LightTileData {
    u32 offset = 0;
    u32 count = 0;
};

class LightGrid {
public:
    /// @brief Recompute desde cero el grid para un frame.
    /// @param lights         frame data del LightSystem (point lights subset).
    /// @param view           matriz de vista de la camara activa.
    /// @param projection     matriz de proyeccion.
    /// @param viewportWidth  ancho del scene FB en pixeles.
    /// @param viewportHeight alto del scene FB en pixeles.
    /// @note El metodo es CPU-only (sin OpenGL). El render pass
    ///       toma `tileData()` + `lightIndices()` y las sube via SSBO.
    void compute(const LightFrameData& lights,
                 const glm::mat4& view,
                 const glm::mat4& projection,
                 u32 viewportWidth,
                 u32 viewportHeight);

    u32 tilesX() const { return m_tilesX; }
    u32 tilesY() const { return m_tilesY; }
    u32 tileCount() const { return m_tilesX * m_tilesY; }

    const std::vector<LightTileData>& tileData() const { return m_tileData; }
    const std::vector<u32>& lightIndices() const { return m_lightIndices; }

    /// @brief Cantidad total de inserciones (suma de counts por tile).
    ///        Util para debug / status bar / tests.
    u32 totalAssignments() const {
        return static_cast<u32>(m_lightIndices.size());
    }

private:
    u32 m_tilesX = 0;
    u32 m_tilesY = 0;
    std::vector<LightTileData> m_tileData;
    std::vector<u32> m_lightIndices;
};

/// @brief Helper testeable (sin estado). Calcula el AABB 2D en
///        coordenadas de tile que cubre la sphere `center, radius` dada
///        la matriz `viewProj` y el tamano del viewport en pixeles.
///        Devuelve `false` si la sphere queda completamente detras de
///        la camara o fuera del viewport (no marcar nada).
///        Output: `outMinTile` y `outMaxTile` inclusive (range valido en
///        [0, tilesX-1] x [0, tilesY-1]). Llamar a `tileCountX/Y` para
///        validar bounds en el caller.
struct TileRange { u32 minX, minY, maxX, maxY; };

bool projectSphereToTileRange(const glm::vec3& centerWorld,
                              f32 radius,
                              const glm::mat4& view,
                              const glm::mat4& projection,
                              u32 viewportWidth,
                              u32 viewportHeight,
                              TileRange& outRange);

} // namespace Mood
