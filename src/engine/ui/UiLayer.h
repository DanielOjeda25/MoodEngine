#pragma once

// Capa de UI del juego (Hito 20). Owns el `Rml::Context` global y los
// adapters (System + Render) que RmlUi necesita. Vive en
// `EditorApplication` paralelo al `EditorUI` (Dear ImGui — UI del
// editor); RmlUi se renderiza solo durante Play Mode (Bloque 3+).
//
// Bloque 1: Context vacio, sin Documents. Verifica integracion.
// Bloque 2: render del Context al overlay framebuffer del viewport.
// Bloque 3+: documents reales (HUD, menu de pausa).

#include "core/Types.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <memory>
#include <string>

namespace Rml {
    class Context;
    class ElementDocument;
}

namespace Mood {

class RmlSystem;
class RmlRenderer;

/// @brief Estado expuesto al HUD via Rml::DataModel. Mutable desde C++
///        / Lua; el binding `{{hp}}`/`{{ammo}}` del .rml refleja en
///        vivo. Hito 20 Bloque 3.
struct HudState {
    int hp   = 100;
    int ammo = 30;
};

class UiLayer {
public:
    UiLayer();
    ~UiLayer();

    UiLayer(const UiLayer&) = delete;
    UiLayer& operator=(const UiLayer&) = delete;

    /// @brief `true` si RmlUi se inicializo con exito y el Context es
    ///        valido. Si `false`, el editor sigue funcionando sin UI
    ///        de juego (la UI del editor con ImGui no se afecta).
    bool isAvailable() const { return m_context != nullptr; }

    /// @brief Resize del Context. Llamar cuando el viewport cambie de
    ///        tamano (asi los documents respetan el viewbox).
    void resize(u32 width, u32 height);

    /// @brief Update del Context (llama Rml::Context::Update). Procesa
    ///        animaciones CSS, eventos, layouts dirty. Llamar 1 vez por
    ///        frame.
    void update();

    /// @brief Render del Context (RmlUi llama a `RmlRenderer::*`). Llamar
    ///        despues del scene render y antes de ImGui.
    void render();

    /// @brief Hito 20 Bloque 3: muestra/oculta el HUD del juego. Llamado
    ///        por `EditorApplication` segun `EditorMode::Play`.
    void setHudVisible(bool visible);

    /// @brief Hito 20 Bloque 4: menu de pausa. Toggle desde
    ///        `EditorApplication` cuando el dev aprieta Esc.
    void setPauseVisible(bool visible);
    bool isPauseVisible() const { return m_pauseVisible; }

    /// @brief Acciones que el menu de pausa emite. Las consume
    ///        `EditorApplication` para reaccionar (cerrar menu, salir
    ///        al editor, popup de opciones, etc).
    enum class PauseAction { None, Resume, Options, Quit };

    /// @brief Devuelve la accion pendiente (si hay) y la resetea.
    ///        Llamada una vez por frame por `EditorApplication`.
    PauseAction consumePauseAction();

    /// @brief Inputs de mouse para que RmlUi pueda hover/click sobre los
    ///        botones del menu. Coords en pixel-space del viewport
    ///        (origen top-left). EditorApplication los emite cuando el
    ///        cursor esta sobre el panel Viewport y el menu de pausa
    ///        esta visible.
    void processMouseMove(int x, int y);
    void processMouseButton(int button, bool down);

    /// @brief Acceso al estado del HUD para mutarlo desde C++ o Lua.
    ///        Tras editarlo, el binding `{{hp}}`/`{{ammo}}` se
    ///        actualiza en el siguiente Update().
    HudState& hud() { return m_hudState; }
    const HudState& hud() const { return m_hudState; }

    /// @brief Marca el data model como dirty para forzar re-bind de los
    ///        valores en pantalla. Llamar tras editar `hud()` en codigo
    ///        (los setters Lua del Bloque 5 lo llaman automatico).
    void markHudDirty();

private:
    class PauseListener; // fwd: Rml::EventListener concreto

    std::unique_ptr<RmlSystem>   m_system;
    std::unique_ptr<RmlRenderer> m_renderer;
    Rml::Context*                m_context = nullptr; // owned by RmlUi
    Rml::ElementDocument*        m_hudDocument = nullptr; // owned by Context
    Rml::ElementDocument*        m_pauseDocument = nullptr;
    Rml::DataModelHandle         m_hudModel;
    HudState                     m_hudState;

    // Listeners del menu de pausa (uno por boton). Owned aca; los
    // elementos de RmlUi solo guardan el puntero crudo.
    std::unique_ptr<PauseListener> m_listenerResume;
    std::unique_ptr<PauseListener> m_listenerOptions;
    std::unique_ptr<PauseListener> m_listenerQuit;
    PauseAction                  m_pendingAction = PauseAction::None;

    u32                          m_width   = 1280;
    u32                          m_height  = 720;
    bool                         m_hudVisible   = false;
    bool                         m_pauseVisible = false;
};

} // namespace Mood
