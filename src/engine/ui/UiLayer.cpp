#include "engine/ui/UiLayer.h"

#include "core/Log.h"
#include "engine/ui/RmlRenderer.h"
#include "engine/ui/RmlSystem.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/FontEngineInterface.h>

namespace Mood {

// EventListener concreto que rutea clicks de los botones del menu de
// pausa a UiLayer::m_pendingAction. Multiples instancias (una por
// boton) — cada una sabe que accion emite.
class UiLayer::PauseListener : public Rml::EventListener {
public:
    PauseListener(UiLayer* layer, PauseAction action)
        : m_layer(layer), m_action(action) {}

    void ProcessEvent(Rml::Event& /*event*/) override {
        if (m_layer != nullptr) m_layer->m_pendingAction = m_action;
    }

private:
    UiLayer* m_layer;
    PauseAction m_action;
};

UiLayer::UiLayer() {
    m_system   = std::make_unique<RmlSystem>();
    m_renderer = std::make_unique<RmlRenderer>();

    // RmlUi requiere registrar SystemInterface + RenderInterface ANTES
    // de Rml::Initialise. El SystemInterface se setea con `Set...`; el
    // RenderInterface se pasa como argumento por contexto via
    // `Rml::Initialise()` con setteo previo (API de RmlUi 6).
    Rml::SetSystemInterface(m_system.get());
    Rml::SetRenderInterface(m_renderer.get());

    if (!Rml::Initialise()) {
        Log::editor()->warn("UiLayer: Rml::Initialise() fallo");
        return;
    }

    // RmlUi requiere AL MENOS una font cargada antes de crear contexts /
    // documents — sin font, el layout no calcula dimensiones de texto y
    // los Show() de documents no muestran nada visible. Lato Regular
    // viene de los samples de RmlUi (CC0). Tolera fallo: el editor sigue
    // sin UI de juego si la font no esta.
    if (!Rml::LoadFontFace("assets/ui/fonts/LatoLatin-Regular.ttf",
                            /*fallback_face=*/true)) {
        Log::editor()->warn(
            "UiLayer: no se pudo cargar 'assets/ui/fonts/LatoLatin-Regular.ttf'."
            " Documents con texto no van a renderear correctamente.");
    }

    m_context = Rml::CreateContext("main", {static_cast<int>(m_width),
                                              static_cast<int>(m_height)});
    if (m_context == nullptr) {
        Log::editor()->warn("UiLayer: Rml::CreateContext() fallo");
        Rml::Shutdown();
        return;
    }

    Log::editor()->info(
        "UiLayer: RmlUi inicializado (context '{}', {}x{}).",
        "main", m_width, m_height);

    // Bloque 2 smoke test: un Document inline con un cuadrado rojo
    // semitransparente en la esquina superior derecha. Sin fonts (no
    // tenemos fuentes cargadas todavia), el "texto" no se renderiza
    // — solo el background del div. Verificamos que el RenderInterface
    // dibuja geometria a la pantalla. El Bloque 3 reemplaza este
    // placeholder por `assets/ui/hud.rml`.
    // HUD del juego (Hito 20 Bloque 3). Bindea HP + Ammo via DataModel
    // para que la UI lea valores mutables desde C++/Lua. Empieza
    // oculto: `setHudVisible(true)` lo muestra cuando el editor entra
    // a Play Mode.
    if (auto handle = m_context->CreateDataModel("hud"); handle) {
        handle.Bind("hp",   &m_hudState.hp);
        handle.Bind("ammo", &m_hudState.ammo);
        m_hudModel = handle.GetModelHandle();
    } else {
        Log::editor()->warn("UiLayer: no se pudo crear data model 'hud'");
    }

    m_hudDocument = m_context->LoadDocument("assets/ui/hud.rml");
    if (m_hudDocument != nullptr) {
        Log::editor()->info("UiLayer: HUD cargado ('assets/ui/hud.rml').");
    } else {
        Log::editor()->warn(
            "UiLayer: 'assets/ui/hud.rml' no se pudo cargar.");
    }

    // Menu de pausa (Hito 20 Bloque 4). Empieza oculto; el
    // EditorApplication lo muestra con `setPauseVisible(true)` al
    // recibir Esc en Play Mode. Listeners en C++ rutean los clicks
    // a `m_pendingAction`.
    m_pauseDocument = m_context->LoadDocument("assets/ui/pause_menu.rml");
    if (m_pauseDocument != nullptr) {
        m_listenerResume  = std::make_unique<PauseListener>(this, PauseAction::Resume);
        m_listenerOptions = std::make_unique<PauseListener>(this, PauseAction::Options);
        m_listenerQuit    = std::make_unique<PauseListener>(this, PauseAction::Quit);
        if (auto* btn = m_pauseDocument->GetElementById("btn-resume"))
            btn->AddEventListener(Rml::EventId::Click, m_listenerResume.get());
        if (auto* btn = m_pauseDocument->GetElementById("btn-options"))
            btn->AddEventListener(Rml::EventId::Click, m_listenerOptions.get());
        if (auto* btn = m_pauseDocument->GetElementById("btn-quit"))
            btn->AddEventListener(Rml::EventId::Click, m_listenerQuit.get());
        Log::editor()->info("UiLayer: pause menu cargado ('assets/ui/pause_menu.rml').");
    } else {
        Log::editor()->warn(
            "UiLayer: 'assets/ui/pause_menu.rml' no se pudo cargar.");
    }
}

UiLayer::~UiLayer() {
    if (m_context != nullptr) {
        // Rml::Shutdown destruye todos los contexts internamente; no
        // hace falta RemoveContext explicito.
        Rml::Shutdown();
        m_context = nullptr;
    }
    // Los adapters se destruyen DESPUES del shutdown (RmlUi puede
    // emitir logs durante Shutdown); el unique_ptr cleanup ocurre al
    // salir del dtor.
}

void UiLayer::resize(u32 width, u32 height) {
    if (m_context == nullptr) return;
    if (width == m_width && height == m_height) return;
    Log::editor()->info(
        "UiLayer::resize {}x{} -> {}x{}",
        m_width, m_height, width, height);
    m_width  = width;
    m_height = height;
    m_context->SetDimensions({static_cast<int>(width),
                              static_cast<int>(height)});
    // Forzar re-layout de los documents al cambiar dimensiones — sin
    // esto los % / dp quedan cacheados con el size anterior.
    // (DirtyLayout es private en ElementDocument; usar el equivalente
    //  publico DispatchEvent("resize") o re-show del document.)
    if (m_hudDocument && m_hudVisible) {
        m_hudDocument->Hide();
        m_hudDocument->Show();
    }
    if (m_pauseDocument && m_pauseVisible) {
        m_pauseDocument->Hide();
        m_pauseDocument->Show();
    }
}

void UiLayer::update() {
    if (m_context == nullptr) return;
    m_context->Update();
}

void UiLayer::render() {
    if (m_context == nullptr || m_renderer == nullptr) return;
    m_renderer->beginFrame(m_width, m_height);
    m_context->Render();
    m_renderer->endFrame();
}

void UiLayer::setHudVisible(bool visible) {
    if (m_hudDocument == nullptr) return;
    if (m_hudVisible == visible) return;
    m_hudVisible = visible;
    if (visible) m_hudDocument->Show();
    else         m_hudDocument->Hide();
}

void UiLayer::markHudDirty() {
    if (!m_hudModel) return;
    m_hudModel.DirtyVariable("hp");
    m_hudModel.DirtyVariable("ammo");
}

void UiLayer::setPauseVisible(bool visible) {
    if (m_pauseDocument == nullptr) return;
    if (m_pauseVisible == visible) return;
    m_pauseVisible = visible;
    if (visible) m_pauseDocument->Show();
    else         m_pauseDocument->Hide();
}

UiLayer::PauseAction UiLayer::consumePauseAction() {
    const PauseAction r = m_pendingAction;
    m_pendingAction = PauseAction::None;
    return r;
}

void UiLayer::processMouseMove(int x, int y) {
    if (m_context == nullptr) return;
    m_context->ProcessMouseMove(x, y, /*modifiers=*/0);
}

void UiLayer::processMouseButton(int button, bool down) {
    if (m_context == nullptr) return;
    if (down) m_context->ProcessMouseButtonDown(button, /*modifiers=*/0);
    else      m_context->ProcessMouseButtonUp  (button, /*modifiers=*/0);
}

} // namespace Mood
