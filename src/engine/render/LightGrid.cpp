#include "engine/render/LightGrid.h"

#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace Mood {

bool projectSphereToTileRange(const glm::vec3& centerWorld,
                              f32 radius,
                              const glm::mat4& view,
                              const glm::mat4& projection,
                              u32 viewportWidth,
                              u32 viewportHeight,
                              TileRange& outRange) {
    if (viewportWidth == 0 || viewportHeight == 0 || radius <= 0.0f) {
        return false;
    }

    // Tile counts (ceiling div: una sphere puede tocar el ultimo tile
    // parcial al borde del viewport).
    const u32 tilesX = (viewportWidth  + k_LightGridTileSize - 1) / k_LightGridTileSize;
    const u32 tilesY = (viewportHeight + k_LightGridTileSize - 1) / k_LightGridTileSize;

    // Approach: tomar las 8 esquinas del AABB world-space del sphere
    // (center +/- radius en cada eje), proyectar cada una a NDC, y
    // tomar min/max. Es conservativo (la AABB es mas grande que la
    // sphere) pero asegura que ningun tile valido se pierda.
    //
    // Caso clip: si TODAS las esquinas tienen w<=0 (atras de la camara)
    // descartamos. Si alguna esta delante y otras atras, tomamos solo
    // las delante con clamp del rango — la sphere se interseca con la
    // camara y deberia iluminar TODO el viewport en NDC.
    constexpr glm::vec3 kCorners[8] = {
        {-1, -1, -1}, { 1, -1, -1}, {-1,  1, -1}, { 1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, {-1,  1,  1}, { 1,  1,  1}};

    const glm::mat4 vp = projection * view;

    bool anyInFront = false;
    bool anyBehind  = false;
    f32 minX_ndc =  std::numeric_limits<f32>::infinity();
    f32 minY_ndc =  std::numeric_limits<f32>::infinity();
    f32 maxX_ndc = -std::numeric_limits<f32>::infinity();
    f32 maxY_ndc = -std::numeric_limits<f32>::infinity();

    for (const auto& d : kCorners) {
        const glm::vec3 cornerWorld = centerWorld + d * radius;
        const glm::vec4 clip = vp * glm::vec4(cornerWorld, 1.0f);
        if (clip.w <= 1e-4f) {
            anyBehind = true;
            continue;
        }
        anyInFront = true;
        const f32 ndcX = clip.x / clip.w;
        const f32 ndcY = clip.y / clip.w;
        minX_ndc = std::min(minX_ndc, ndcX);
        minY_ndc = std::min(minY_ndc, ndcY);
        maxX_ndc = std::max(maxX_ndc, ndcX);
        maxY_ndc = std::max(maxY_ndc, ndcY);
    }

    if (!anyInFront) return false; // sphere completamente atras de la cam

    // Si parte de la sphere esta detras y parte adelante, no se puede
    // confiar en el AABB proyectado parcial: la luz puede afectar
    // cualquier tile. Marcar todo el viewport.
    if (anyBehind) {
        minX_ndc = -1.0f; minY_ndc = -1.0f;
        maxX_ndc =  1.0f; maxY_ndc =  1.0f;
    }

    // Clamp a [-1, 1] (el AABB puede salirse). Si despues del clamp el
    // rango queda invalido (vacio), descartar.
    minX_ndc = std::max(minX_ndc, -1.0f);
    minY_ndc = std::max(minY_ndc, -1.0f);
    maxX_ndc = std::min(maxX_ndc,  1.0f);
    maxY_ndc = std::min(maxY_ndc,  1.0f);
    if (minX_ndc > maxX_ndc || minY_ndc > maxY_ndc) return false;

    // NDC [-1, 1] -> pixel [0, viewport]. ATENCION al espacio Y: el
    // shader usa `gl_FragCoord.xy` con origen ABAJO-IZQUIERDA (convencion
    // GL). Mantenemos la misma convencion aca: NDC y=-1 -> pixel y=0
    // (fila inferior del viewport), NDC y=+1 -> pixel y=viewportHeight.
    // Sin esto, los tiles del grid quedarian flippeados verticalmente
    // respecto a los pixeles que sample el shader y la luz se "veria"
    // del lado opuesto del que toca.
    const f32 minPx = (minX_ndc * 0.5f + 0.5f) * static_cast<f32>(viewportWidth);
    const f32 maxPx = (maxX_ndc * 0.5f + 0.5f) * static_cast<f32>(viewportWidth);
    const f32 minPy = (minY_ndc * 0.5f + 0.5f) * static_cast<f32>(viewportHeight);
    const f32 maxPy = (maxY_ndc * 0.5f + 0.5f) * static_cast<f32>(viewportHeight);

    auto pxToTile = [](f32 px, u32 maxTile) -> u32 {
        if (px < 0.0f) return 0u;
        const u32 t = static_cast<u32>(px) / k_LightGridTileSize;
        return std::min(t, maxTile);
    };

    outRange.minX = pxToTile(minPx, tilesX - 1);
    outRange.maxX = pxToTile(maxPx, tilesX - 1);
    outRange.minY = pxToTile(minPy, tilesY - 1);
    outRange.maxY = pxToTile(maxPy, tilesY - 1);
    return true;
}

void LightGrid::compute(const LightFrameData& lights,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        u32 viewportWidth,
                        u32 viewportHeight) {
    m_tilesX = (viewportWidth  + k_LightGridTileSize - 1) / k_LightGridTileSize;
    m_tilesY = (viewportHeight + k_LightGridTileSize - 1) / k_LightGridTileSize;
    const u32 total = m_tilesX * m_tilesY;

    m_tileData.assign(total, LightTileData{0u, 0u});
    m_lightIndices.clear();

    if (m_tilesX == 0 || m_tilesY == 0) return;
    if (lights.pointLights.empty()) return;

    // Pase 1: por luz, computar AABB de tiles y acumular `count`
    // por tile. Guardamos los rangos para no recomputarlos en pase 2.
    std::vector<TileRange> ranges;
    ranges.reserve(lights.pointLights.size());
    std::vector<u32> validLights;
    validLights.reserve(lights.pointLights.size());

    for (u32 i = 0; i < lights.pointLights.size(); ++i) {
        const auto& L = lights.pointLights[i];
        TileRange r{};
        if (!projectSphereToTileRange(L.position, L.radius,
                                       view, projection,
                                       viewportWidth, viewportHeight, r)) {
            continue; // descartada (atras / fuera)
        }
        ranges.push_back(r);
        validLights.push_back(i);
        for (u32 ty = r.minY; ty <= r.maxY; ++ty) {
            for (u32 tx = r.minX; tx <= r.maxX; ++tx) {
                ++m_tileData[ty * m_tilesX + tx].count;
            }
        }
    }

    // Pase 2: prefix-sum los counts para obtener offsets globales en el
    // flat array. Despues de esto `count` se restaura a 0 (lo usamos
    // como cursor de escritura en el pase 3).
    u32 running = 0;
    for (u32 t = 0; t < total; ++t) {
        m_tileData[t].offset = running;
        running += m_tileData[t].count;
        m_tileData[t].count = 0;
    }
    m_lightIndices.resize(running);

    // Pase 3: por cada luz valida, escribir su index en cada tile que
    // toca. Usamos `count` como cursor (se incrementa en cada inserc).
    for (u32 li = 0; li < validLights.size(); ++li) {
        const u32 lightIdx = validLights[li];
        const TileRange& r = ranges[li];
        for (u32 ty = r.minY; ty <= r.maxY; ++ty) {
            for (u32 tx = r.minX; tx <= r.maxX; ++tx) {
                LightTileData& tile = m_tileData[ty * m_tilesX + tx];
                m_lightIndices[tile.offset + tile.count] = lightIdx;
                ++tile.count;
            }
        }
    }
}

} // namespace Mood
