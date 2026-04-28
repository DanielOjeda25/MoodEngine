#pragma once

// Runtime standalone del juego — variante slim de EditorApplication sin
// paneles ni herramientas de edicion. Comparte SceneRenderer + sistemas
// (scene, scripts, audio, fisica, animacion) con el editor.
//
// Hito 21 Bloque 3 Fase A: carga un mapa de prueba (sala 8x8 con
// bordes solidos) y lo renderiza desde una FpsCamera. WASD + mouse
// para moverse, Esc para cerrar.
//
// El binario `MoodPlayer.exe` es lo que recibe el usuario final tras
// "Archivo > Empaquetar proyecto" en el editor.

#include "core/Time.h"
#include "core/Types.h"
#include "engine/scene/FpsCamera.h"
#include "engine/world/GridMap.h"

#include <glm/vec3.hpp>

#include <memory>

namespace Mood {

class AssetManager;
class Scene;
class SceneRenderer;
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

    /// @brief Lee WASD + delta del mouse relativo y actualiza
    ///        m_playCamera. Aplica colisiones contra el GridMap (igual
    ///        que el editor en Play Mode).
    void updateCamera(f32 dt);

    /// @brief Construye el mapa de prueba (sala 8x8 perimetral) y
    ///        materializa una entidad por tile solido. Placeholder
    ///        hasta que Bloque 4 cargue el `.moodmap` real desde
    ///        `game.json`.
    void buildTestMap();
    void rebuildSceneFromMap();

    /// @brief Origen del mundo del mapa (tile (0,0) -> world). Igual
    ///        que en EditorApplication.
    glm::vec3 mapWorldOrigin() const;

    std::unique_ptr<Window>        m_window;
    std::unique_ptr<SceneRenderer> m_sceneRenderer;
    std::unique_ptr<AssetManager>  m_assetManager;
    std::unique_ptr<Scene>         m_scene;

    GridMap m_map{8u, 8u, 3.0f};

    // FPS camera: misma posicion default que el editor en Play Mode.
    FpsCamera m_playCamera{glm::vec3(-4.5f, 1.6f, 7.5f), -90.0f, 0.0f};

    // AABB del jugador para colisiones (mismo half-extent que el editor).
    static constexpr glm::vec3 k_playerHalfExtents{0.3f, 0.9f, 0.3f};

    Timer m_deltaTimer;
    bool  m_running = true;
};

} // namespace Mood
