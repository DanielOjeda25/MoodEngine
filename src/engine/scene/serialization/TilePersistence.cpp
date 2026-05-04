#include "engine/scene/serialization/TilePersistence.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/world/grid/GridMap.h"

#include <cstdlib>  // std::strtoul
#include <cstring>  // std::strcmp, std::strlen

namespace Mood {

bool parseTileCoords(const char* tag, u32& outX, u32& outY) {
    if (tag == nullptr) return false;
    constexpr const char* kPrefix = "Tile_";
    constexpr usize kPrefixLen = 5;
    if (std::strncmp(tag, kPrefix, kPrefixLen) != 0) return false;

    const char* xStart = tag + kPrefixLen;
    char* sep = nullptr;
    const unsigned long x = std::strtoul(xStart, &sep, 10);
    if (sep == xStart || sep == nullptr || *sep != '_') return false;

    const char* yStart = sep + 1;
    char* end = nullptr;
    const unsigned long y = std::strtoul(yStart, &end, 10);
    if (end == yStart || end == nullptr || *end != '\0') return false;

    outX = static_cast<u32>(x);
    outY = static_cast<u32>(y);
    return true;
}

bool isTileModified(Entity tile, const GridMap& map, const AssetManager& assets) {
    // Sanity: tag debe parsear como Tile_X_Y dentro del grid.
    if (!tile.hasComponent<TagComponent>()) return true;
    const auto& tag = tile.getComponent<TagComponent>();
    u32 x = 0, y = 0;
    if (!parseTileCoords(tag.name.c_str(), x, y)) {
        return true;  // tag inválido = persistir defensivo
    }
    if (x >= map.width() || y >= map.height()) {
        return true;  // fuera del grid = persistir defensivo
    }

    // Componentes "extra" que un tile default NO tiene. Si están
    // presentes con datos no-default, el tile fue modificado.
    if (tile.hasComponent<LightComponent>()) return true;
    if (tile.hasComponent<ParticleEmitterComponent>()) return true;
    if (tile.hasComponent<TriggerComponent>()) return true;
    if (tile.hasComponent<ScriptComponent>()) {
        const auto& sc = tile.getComponent<ScriptComponent>();
        if (!sc.path.empty()) return true;
    }

    // Transform.scale: tile default = (tileSize, tileSize, tileSize).
    // Comparación exacta — el editor cambia el scale en pasos discretos
    // (drag de gizmo o input numérico). Si emerge ruido de coma flotante,
    // agregar epsilon (~1e-4f).
    if (tile.hasComponent<TransformComponent>()) {
        const auto& t = tile.getComponent<TransformComponent>();
        const f32 expectedScale = map.tileSize();
        if (t.scale.x != expectedScale ||
            t.scale.y != expectedScale ||
            t.scale.z != expectedScale) {
            return true;
        }
    } else {
        return true;  // tile sin Transform es un estado raro = persistir
    }

    // MeshRenderer: tile default tiene exactamente 1 material cuyo albedo
    // es la textura del grid en (x, y). Si materials.size() != 1 → modificado;
    // si el albedo no matchea → modificado.
    if (!tile.hasComponent<MeshRendererComponent>()) {
        return true;
    }
    const auto& mr = tile.getComponent<MeshRendererComponent>();
    if (mr.materials.size() != 1u) return true;

    const MaterialAsset* mat = assets.getMaterial(mr.materials[0]);
    if (mat == nullptr) return true;

    const u32 expectedAlbedo = map.tileTextureAt(x, y);
    if (mat->albedo != expectedAlbedo) return true;

    // Pasaron todos los chequeos: tile NO modificado.
    return false;
}

} // namespace Mood
