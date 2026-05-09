#pragma once

// F2H19 + F2H34: comando undoable para asignar material a UNA o MAS
// caras de un brush en Face Mode. Diferente del EditBrushMaterialCommand
// de F2H16 que solo cubre slot 0 (Object Mode "global del brush"). Aca
// capturamos:
//   - Lista de faceIndex afectados (1..N).
//   - El materialIndex previo de cada cara (para revertir).
//   - El snapshot completo de bc.materials (porque el drop puede
//     agregar slot nuevo via push_back; al hacer undo hay que
//     restaurar el vector tal cual estaba, sino quedan slots
//     huerfanos despues de varios undo/redo). El slot agregado
//     se reusa entre TODAS las caras del set (un solo push_back).
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
    /// @brief Constructor multi-cara (F2H34). faceIndices.size() debe
    ///        ser igual a oldFaceMatIndices.size() y newFaceMatIndices.size().
    /// @param scene      Scene actual (no-owning).
    /// @param entityTag  Tag de la entidad con BrushComponent.
    /// @param faceIndices  Indices de las caras dentro de bc.brush.faces.
    /// @param oldMaterials  Snapshot pre del vector bc.materials.
    /// @param oldFaceMatIndices  bc.brush.faces[faceIndices[i]].materialIndex pre.
    /// @param newMaterials  Snapshot post del vector bc.materials.
    /// @param newFaceMatIndices  bc.brush.faces[faceIndices[i]].materialIndex post.
    /// @param label  Texto humano-legible para Editar > Deshacer + statusbar.
    EditBrushFaceMaterialCommand(Scene* scene,
                                  std::string entityTag,
                                  std::vector<u32> faceIndices,
                                  std::vector<MaterialAssetId> oldMaterials,
                                  std::vector<u32> oldFaceMatIndices,
                                  std::vector<MaterialAssetId> newMaterials,
                                  std::vector<u32> newFaceMatIndices,
                                  std::string label = "Asignar material a cara");

    /// @brief F2H19: constructor de conveniencia 1-cara (wrappea al multi).
    ///        Mantenido para back-compat con call-sites del editor + tests
    ///        del hito F2H19 que ya estaban en main.
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
    std::vector<u32> m_faceIndices;
    std::vector<MaterialAssetId> m_oldMaterials;
    std::vector<u32> m_oldFaceMatIndices;
    std::vector<MaterialAssetId> m_newMaterials;
    std::vector<u32> m_newFaceMatIndices;
    std::string m_label;

    void apply(const std::vector<MaterialAssetId>& materials,
                const std::vector<u32>& faceMatIndices);
};

} // namespace Mood
