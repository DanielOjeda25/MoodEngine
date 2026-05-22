#pragma once

// Interfaz base comun a todos los paneles acoplables del editor.

#include <string>

namespace Mood {

class IPanel {
public:
    virtual ~IPanel() = default;

    /// @brief Se llama una vez por frame mientras el panel este visible.
    ///        Aqui el panel emite sus llamadas a ImGui.
    virtual void onImGuiRender() = 0;

    /// @brief Nombre legible del panel, usado en el menu "Ver" y en el titulo
    ///        de la ventana ImGui.
    virtual const char* name() const = 0;

    /// @brief F2H7: categoria para el menu "Ver" jerarquico.
    ///        Valores convencionales: "Scene", "Assets", "Debug", "World".
    ///        Default = "Scene" (el grueso de los panels existentes vive ahi).
    ///        Override por panel cuando aplique (ej. AssetBrowser → "Assets").
    virtual const char* category() const { return "Scene"; }

    /// @brief F2H78 (Ctrl+S contextual): ¿este panel guarda SU contenido con
    ///        Ctrl+S cuando tiene foco? (script/shader/item/quest editors).
    ///        El handler global de Ctrl+S consulta esto sobre los panels
    ///        `visible`: si alguno lo devuelve true, NO dispara ademas el
    ///        guardado de proyecto (el editor enfocado ya se guarda solo en
    ///        su render). Los panels guardables lo respaldan con un bool de
    ///        foco actualizado en onImGuiRender via ImGui::IsWindowFocused.
    virtual bool consumesSaveShortcut() const { return false; }

    /// @brief Toggle de visibilidad. El EditorUI revisa este flag antes de
    ///        invocar onImGuiRender.
    bool visible = true;
};

} // namespace Mood
