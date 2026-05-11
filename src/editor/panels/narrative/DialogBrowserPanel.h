#pragma once

// DialogBrowserPanel (F2H47): lista todos los `.mooddialog` del
// proyecto activo + acciones (new / open / delete). Click sobre un
// item abre el dialog en `DialogEditorPanel`.
//
// El path donde viven los dialogs es `<project_root>/assets/dialogs/`
// por convencion.

#include "editor/panels/IPanel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class EditorUI;

class DialogBrowserPanel : public IPanel {
public:
    DialogBrowserPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Dialog Browser"; }
    const char* category() const override { return "Narrative"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief Refresca el listado del directorio. Llamar tras crear/
    ///        borrar un dialog (o cuando el proyecto cambia).
    void refresh();

private:
    /// @brief Resuelve el directorio `<project_root>/assets/dialogs/`.
    ///        Si no hay proyecto activo o el directorio no existe,
    ///        retorna nullopt.
    std::optional<std::filesystem::path> dialogsDir() const;

    void drawNewDialogPopup();

    EditorUI*                                  m_ui = nullptr;
    std::vector<std::filesystem::path>         m_files;
    bool                                       m_refreshed = false;
    bool                                       m_newPopupOpen = false;
    std::string                                m_newName;
};

} // namespace Mood
