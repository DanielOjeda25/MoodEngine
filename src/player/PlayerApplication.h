#pragma once

// Runtime standalone del juego — variante slim de EditorApplication sin
// paneles ni herramientas de edicion. Hito 21 Bloque 1: scaffolding
// minimo (ventana + contexto GL + ImGui + Esc para cerrar). Los bloques
// siguientes le agregan render de escena, scripts, audio, fisica.
//
// El binario `MoodPlayer.exe` es lo que recibe el usuario final tras
// "Archivo > Empaquetar proyecto" en el editor. Por eso comparte el
// maximo posible de codigo con MoodEditor (mismo `engine/`, `systems/`,
// `core/`, `platform/`) y solo difiere en este archivo + `main.cpp`.

#include <memory>

namespace Mood {

class Window;

class PlayerApplication {
public:
    PlayerApplication();
    ~PlayerApplication();

    PlayerApplication(const PlayerApplication&) = delete;
    PlayerApplication& operator=(const PlayerApplication&) = delete;

    /// @brief Loop principal. Retorna el exit code para main().
    int run();

private:
    void processEvents();
    void beginFrame();
    void endFrame();

    std::unique_ptr<Window> m_window;
    bool m_running = true;
};

} // namespace Mood
