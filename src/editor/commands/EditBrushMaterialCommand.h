#pragma once

// F2H16: comando undoable para cambios de material en un brush.
// Captura por TAG (no por handle EnTT) para sobrevivir a undo/redo
// de Delete/Create que invaliden los handles. Mismo patron que
// BooleanOpCommand de F2H12.
//
// Uso: cuando el user dropa una textura/material sobre un brush,
// el handler dropea + crea snapshot pre del bc.material, ejecuta
// la asignacion, snapshot post, y empuja un EditBrushMaterialCommand
// al HistoryStack.

#include "editor/commands/Command.h"
#include "engine/assets/manager/AssetManager.h"  // MaterialAssetId

#include <string>

namespace Mood {

class Scene;

class EditBrushMaterialCommand : public ICommand {
public:
    /// @brief @param entityTag Tag de la entidad con BrushComponent.
    ///        Capturado por valor para que sobreviva a recreaciones
    ///        del handle.
    /// @param oldMat / newMat IDs de material. El comando se aplica
    ///        leyendo el componente actual y reemplazando.
    /// @param label Texto humano-legible para el menu Editar > Deshacer
    ///        y para el statusbar.
    EditBrushMaterialCommand(Scene* scene,
                              std::string entityTag,
                              MaterialAssetId oldMat,
                              MaterialAssetId newMat,
                              std::string label = "Asignar material a brush");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    Scene* m_scene;
    std::string m_entityTag;
    MaterialAssetId m_oldMat;
    MaterialAssetId m_newMat;
    std::string m_label;

    void applyMaterial(MaterialAssetId mat);
};

} // namespace Mood
