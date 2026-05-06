#pragma once

// Componente que ata una entidad a un brush CSG (F2H11). A diferencia
// de los demas componentes (Components.h), este SI tiene ownership: el
// IMesh que se uploadea al GPU vive aqui, no en AssetManager. Razon:
// los brushes son geometria EDITABLE en runtime que se regenera cada
// vez que el dev mueve una cara o agrega/quita un brush — no encajan
// en el modelo "asset cargado de disco" del AssetManager.
//
// Flujo del SceneRenderer:
//   1. Itera entidades con TransformComponent + BrushComponent.
//   2. Si `dirty == true`:
//      - buildBrushMesh(brush) -> BrushMeshData
//      - brushMeshDataToInterleaved(...) -> std::vector<f32>
//      - assets.createDynamicMesh(verts, attrs) -> std::unique_ptr<IMesh>
//      - swap meshCache con el nuevo, dirty=false.
//   3. Setea uModel = transform.worldMatrix() y dibuja.

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h"  // MaterialAssetId
#include "engine/render/rhi/IMesh.h"
#include "engine/world/csg/Brush.h"

#include <glm/mat4x4.hpp>

#include <memory>

namespace Mood {

struct BrushComponent {
    Csg::Brush brush;
    /// @brief Material aplicado a TODAS las caras del brush en F2H11.
    ///        F2H16 lo reemplaza con un material per-cara cuando llegue
    ///        el face mode + selection.
    MaterialAssetId material = 0;
    /// @brief Mesh GPU regenerada bajo demanda. unique_ptr garantiza
    ///        que se libera al destruir la entidad o al swap por una
    ///        version mas nueva (cuando dirty=true).
    std::unique_ptr<IMesh> meshCache;
    /// @brief True si el brush cambio y la meshCache esta obsoleta.
    ///        El SceneRenderer la regenera al ver dirty=true.
    bool dirty = true;

    /// @brief F2H15: cacheado al rebuild de la mesh. Si alguna cara
    ///        tiene `lockToWorld=true`, el SceneRenderer fuerza
    ///        rebuild cada frame que el transform cambia (las UVs
    ///        dependen de la posicion world). Si todas son false,
    ///        comportamiento normal con dirty manual.
    bool anyFaceLockToWorld = false;
    /// @brief F2H15: ultima worldMatrix con la que se rebuildeo la
    ///        mesh. Usado para detectar cambios cuando
    ///        anyFaceLockToWorld == true.
    glm::mat4 lastWorldMatrix{1.0f};
};

} // namespace Mood
