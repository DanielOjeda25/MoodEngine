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
#include <vector>

namespace Mood {

struct BrushComponent {
    Csg::Brush brush;
    /// @brief F2H17: slots de material per-cara. `face.materialIndex`
    ///        de cada cara (campo de F2H11) indexea este vector.
    ///        Default `{0}` = 1 slot con material default (look
    ///        "blank gris"). Cuando el dev dropea texturas en Face
    ///        Mode, se agregan slots nuevos y `face.materialIndex`
    ///        se actualiza para apuntar al slot correspondiente.
    ///        Antes de F2H17 era un solo `MaterialAssetId material`
    ///        compartido por todas las caras.
    std::vector<MaterialAssetId> materials{0};
    /// @brief F2H17: una mesh GPU por submesh (1 por slot de
    ///        material). Antes era una mesh unica para todo el brush.
    ///        El SceneRenderer hace 1 draw call por entry con su
    ///        material correspondiente. Si `dirty=true`, todo el
    ///        vector se regenera.
    std::vector<std::unique_ptr<IMesh>> meshCache;
    /// @brief F2H17: paralelo a `meshCache`, contiene el material
    ///        slot de cada submesh. `bc.materials[meshCacheSlots[i]]`
    ///        da el MaterialAssetId a bindear cuando se dibuja
    ///        `meshCache[i]`.
    std::vector<u32> meshCacheSlots;
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

    BrushComponent() = default;
    BrushComponent(const BrushComponent&) = delete;
    BrushComponent& operator=(const BrushComponent&) = delete;
    BrushComponent(BrushComponent&&) = default;
    BrushComponent& operator=(BrushComponent&&) = default;
};

} // namespace Mood
