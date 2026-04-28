#pragma once

// Runtime standalone del juego — variante slim de EditorApplication sin
// paneles ni herramientas de edicion. Comparte SceneRenderer + sistemas
// (scene, scripts, audio, fisica, animacion) con el editor.
//
// Hito 21 Bloque 3 Fase B: loop completo con scripts Lua, audio,
// animation, physics + HUD del juego (compartido con el editor en
// Play Mode via `engine/game/GameOverlay`) + menu de pausa con Esc.
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

class AnimationSystem;
class AssetManager;
class AudioDevice;
class AudioSystem;
class PhysicsWorld;
class Scene;
class SceneRenderer;
class ScriptSystem;
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

    /// @brief Sync de cursor con `GameState::paused()` + WASD/mouse a
    ///        FpsCamera + colision contra el GridMap. Mismo flow que
    ///        `EditorApplication::updateCameras` en Play Mode.
    void updateCamera(f32 dt);

    /// @brief Materializa rigid bodies para entidades con
    ///        RigidBodyComponent y stepea la simulacion (igual que el
    ///        editor en Play Mode).
    void updateRigidBodies(f32 dt);

    /// @brief Intenta cargar `<exe_dir>/game.json`. Si existe + valida +
    ///        el proyecto + mapa default cargan: aplica todo al scene
    ///        y retorna true. Si falla en cualquier paso retorna false
    ///        (caller cae al `buildTestMap` placeholder).
    bool tryLoadGameManifest();

    /// @brief Construye el mapa de prueba (sala 8x8 perimetral) y
    ///        materializa una entidad por tile solido. Fallback usado
    ///        cuando no hay `game.json` valido — util al correr
    ///        `MoodPlayer.exe` directo desde `build/debug/Debug/` sin
    ///        empaquetar.
    void buildTestMap();
    void rebuildSceneFromMap();

    glm::vec3 mapWorldOrigin() const;

    std::unique_ptr<Window>        m_window;
    std::unique_ptr<SceneRenderer> m_sceneRenderer;
    std::unique_ptr<AssetManager>  m_assetManager;
    std::unique_ptr<Scene>         m_scene;

    // Hito 21 Bloque 3 Fase B: sistemas que el editor tambien tiene en
    // Play Mode. Sin estos el player no puede correr scripts Lua,
    // reproducir audio, animar personajes, ni simular fisica dinamica.
    std::unique_ptr<ScriptSystem>     m_scriptSystem;
    std::unique_ptr<AudioDevice>      m_audioDevice;
    std::unique_ptr<AudioSystem>      m_audioSystem;
    std::unique_ptr<AnimationSystem>  m_animationSystem;
    std::unique_ptr<PhysicsWorld>     m_physicsWorld;

    GridMap m_map{8u, 8u, 3.0f};

    FpsCamera m_playCamera{glm::vec3(-4.5f, 1.6f, 7.5f), -90.0f, 0.0f};

    static constexpr glm::vec3 k_playerHalfExtents{0.3f, 0.9f, 0.3f};

    // Tracking de la transicion del flag `GameState::paused()` para
    // sincronizar SDL_SetRelativeMouseMode una sola vez por toggle
    // (igual que EditorApplication::m_pausedLastFrame).
    bool m_pausedLastFrame = false;

    Timer m_deltaTimer;
    bool  m_running = true;
};

} // namespace Mood
