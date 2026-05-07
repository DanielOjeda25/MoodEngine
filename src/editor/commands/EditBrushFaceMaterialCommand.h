#pragma once

// F2H19: comando undoable para asignar material a UNA cara de un brush
// en Face Mode. Diferente del EditBrushMaterialCommand de F2H16 que solo
// cubre slot 0 (Object Mode "global del brush"). Aca capturamos:
//   - El materialIndex previo de la cara afectada (para revertir).
//   - El snapshot completo de bc.materials (porque el drop puede
//     agregar slot nuevo via push_back; al hacer undo hay que
//     restaurar el vector tal cual estaba, sino quedan slots
//     huerfanos despues de varios undo/redo).
//
// Captura por TAG (no handle EnTT) — mismo patron que el resto de
// comandos post-F2H16.

#include "editor/commands/Command.h"
#include "engine/assets/manager/AssetManager.h"  // MaterialAssetId

#include <string>
#include <vector>

#include "core/Types.h"

namespace Mood {

class Scene;

class EditBrushFaceMaterialCommand : public ICommand {
public:
    /// @param scene      Scene actual (no-owning).
    /// @param entityTag  Tag de la entidad con BrushComponent.
    /// @param faceIndex  Indice de la cara dentro de bc.brush.faces.
    /// @param oldMaterials  Snapshot pre del vector bc.materials.
    /// @param oldFaceMatIndex  bc.brush.faces[faceIndex].materialIndex pre.
    /// @param newMaterials  Snapshot post del vector bc.materials.
    /// @param newFaceMatIndex  bc.brush.faces[faceIndex].materialIndex post.
    /// @param label  Texto humano-legible para Editar > Deshacer + statusbar.
    EditBrushFaceMaterialCommand(Scene* scene,
                                  std::string entityTag,
                                  u32 faceIndex,
                                  std::vector<MaterialAssetId> oldMaterials,
                                  u32 oldFaceMatIndex,
                                  std::vector<MaterialAssetId> newMaterials,
                                  u32 newFaceMatIndex,
                                  std::string label = "Asignar material a cara");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    Scene* m_scene;
    std::string m_entityTag;
    u32 m_faceIndex;
    std::vector<MaterialAssetId> m_oldMaterials;
    u32 m_oldFaceMatIndex;
    std::vector<MaterialAssetId> m_newMaterials;
    u32 m_newFaceMatIndex;
    std::string m_label;

    void apply(const std::vector<MaterialAssetId>& materials,
                u32 faceMatIndex);
};

} // namespace Mood
