#pragma once

// Estado global del editor: que camara esta activa, si se capturan
// entradas como FPS, etc. Compartido entre EditorApplication, EditorUI y
// los sub-componentes de la UI (MenuBar, StatusBar).

namespace Mood {

enum class EditorMode {
    /// Editor Mode: camara orbital, paneles interactivos, mouse libre.
    Editor,
    /// Play Mode: camara FPS, WASD + mouse capturado. Sale con Esc.
    Play,
};

} // namespace Mood
