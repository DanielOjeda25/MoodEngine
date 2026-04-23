#pragma once

// Sistema que dibuja un GridMap como cubos 3D. Extraido de EditorApplication
// al inicio del Hito 6: el editor va a necesitar reemplazar el mapa al
// abrir/crear proyectos, y el loop de tiles no deberia vivir en el shell.

#include "core/Types.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class IRenderer;
class IShader;
class IMesh;
class AssetManager;
class GridMap;

class GridRenderer {
public:
    /// @brief Referencias capturadas; los objetos apuntados deben vivir mas
    ///        que el GridRenderer (EditorApplication los destruye en orden
    ///        correcto en su destructor).
    GridRenderer(IRenderer& renderer, IShader& shader, IMesh& cubeMesh,
                 AssetManager& assets);

    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;

    /// @brief Dibuja todos los tiles solidos del mapa como cubos unitarios
    ///        escalados a `map.tileSize()` y posicionados relativo a
    ///        `mapWorldOrigin` (el tile (0,0) cae ahi).
    ///        El shader se bindea internamente; view/projection se setean
    ///        una vez antes del loop. Optimiza rebindeos de textura
    ///        trackeando la ultima bound.
    void draw(const GridMap& map, const glm::vec3& mapWorldOrigin,
              const glm::mat4& view, const glm::mat4& projection);

private:
    IRenderer& m_renderer;
    IShader& m_shader;
    IMesh& m_cubeMesh;
    AssetManager& m_assets;
};

} // namespace Mood
