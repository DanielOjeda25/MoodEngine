#pragma once

// F2H16: comando undoable para cambios de material del slot 0 de
// un MeshRendererComponent (drop de material sobre una entidad
// con mesh — Hito 17 + Hito 32 dejaron el path sin command).
//
// F2H16 cubre solo el slot 0 (que es el caso usado por el drop
// handler). Multi-slot edits del Inspector ya usan
// EditPropertyCommand<MaterialAssetId> via pushEditIfDone (Hito 32).

#include "editor/commands/Command.h"
#include "engine/assets/manager/AssetManager.h"

#include <string>

namespace Mood {

class Scene;

class EditMeshRendererMaterialCommand : public ICommand {
public:
    /// @param slotIndex Por ahora siempre 0 (drop handler usa solo
    ///        slot 0). Si emerge necesidad de drop sobre submeshes
    ///        especificos, parametrizar aca.
    EditMeshRendererMaterialCommand(Scene* scene,
                                      std::string entityTag,
                                      u32 slotIndex,
                                      MaterialAssetId oldMat,
                                      MaterialAssetId newMat,
                                      std::string label =
                                          "Asignar material a mesh");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    Scene* m_scene;
    std::string m_entityTag;
    u32 m_slotIndex;
    MaterialAssetId m_oldMat;
    MaterialAssetId m_newMat;
    std::string m_label;

    void applyMaterial(MaterialAssetId mat);
};

} // namespace Mood
