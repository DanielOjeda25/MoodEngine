#include "systems/GridRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/IMesh.h"
#include "engine/render/IRenderer.h"
#include "engine/render/IShader.h"
#include "engine/render/ITexture.h"
#include "engine/world/GridMap.h"

#include <glm/ext/matrix_transform.hpp>

namespace Mood {

GridRenderer::GridRenderer(IRenderer& renderer, IShader& shader,
                           IMesh& cubeMesh, AssetManager& assets)
    : m_renderer(renderer), m_shader(shader),
      m_cubeMesh(cubeMesh), m_assets(assets) {}

void GridRenderer::draw(const GridMap& map, const glm::vec3& mapWorldOrigin,
                        const glm::mat4& view, const glm::mat4& projection) {
    // View/projection son constantes para todo el frame; se setean una vez.
    // uModel + textura cambian por tile.
    m_shader.bind();
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", projection);
    m_shader.setInt("uTexture", 0);

    // Tracking de textura bound para evitar rebindear si el tile siguiente
    // usa la misma: optimizacion barata que aprovecha el ordenamiento natural
    // del iterado (los tiles del borde van juntos).
    TextureAssetId lastBound = static_cast<TextureAssetId>(-1);

    const f32 tileSize = map.tileSize();

    for (u32 y = 0; y < map.height(); ++y) {
        for (u32 x = 0; x < map.width(); ++x) {
            if (!map.isSolid(x, y)) continue;

            const TextureAssetId tileTex = map.tileTextureAt(x, y);
            if (tileTex != lastBound) {
                m_assets.getTexture(tileTex)->bind(0);
                lastBound = tileTex;
            }

            const glm::vec3 worldPos(
                mapWorldOrigin.x + (static_cast<f32>(x) + 0.5f) * tileSize,
                mapWorldOrigin.y + 0.5f * tileSize,
                mapWorldOrigin.z + (static_cast<f32>(y) + 0.5f) * tileSize);

            glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos);
            model = glm::scale(model, glm::vec3(tileSize));

            m_shader.setMat4("uModel", model);
            m_renderer.drawMesh(m_cubeMesh, m_shader);
        }
    }
}

} // namespace Mood
