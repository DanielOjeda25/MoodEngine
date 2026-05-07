#pragma once

// F2H19: comando undoable para drop de un .lua sobre una entidad.
// Cubre 2 sub-casos:
//   1. La entidad NO tenia ScriptComponent: el comando agrega uno con
//      el path nuevo. Undo remueve el componente.
//   2. La entidad SI tenia ScriptComponent: el comando reemplaza
//      `sc.path` (y resetea `loaded`/`lastError`). Undo restaura el path
//      previo.
//
// Captura por TAG (no handle EnTT) — mismo patron que el resto de
// comandos post-F2H16.
//
// NOTA: las exposed props (`overrides`/`exposedProps`) NO se snapshotean.
// Razon: el flow normal es "drop reemplaza el script entero", asi que
// las overrides del script anterior dejan de aplicar (son por
// nombre+default del script). Si el undo restaura el path viejo, las
// overrides se redescubren al re-cargar via mtime check (esto puede
// limpiar overrides que coincidian — aceptable para drop, no es undo
// de InspectorEdit).

#include "editor/commands/Command.h"

#include <string>

namespace Mood {

class Scene;

class EditScriptComponentCommand : public ICommand {
public:
    /// @param scene      Scene actual (no-owning).
    /// @param entityTag  Tag de la entidad target.
    /// @param hadComponent  true si la entidad ya tenia ScriptComponent
    ///                       antes del drop (para saber si undo debe
    ///                       remover el componente o restaurar path).
    /// @param oldPath  Path previo. Si !hadComponent es ignorado.
    /// @param newPath  Path nuevo (post-drop).
    /// @param label    Texto humano-legible.
    EditScriptComponentCommand(Scene* scene,
                                std::string entityTag,
                                bool hadComponent,
                                std::string oldPath,
                                std::string newPath,
                                std::string label = "Asignar script a entidad");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    Scene* m_scene;
    std::string m_entityTag;
    bool m_hadComponent;
    std::string m_oldPath;
    std::string m_newPath;
    std::string m_label;
};

} // namespace Mood
