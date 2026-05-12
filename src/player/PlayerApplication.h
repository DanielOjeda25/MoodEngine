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
#include "engine/saving/SaveLoad.h"  // SaveData (Hito 38)
#include "engine/scene/core/FpsCamera.h"
#include "engine/world/grid/GridMap.h"
#include "systems/physics/TriggerSystem.h"

#include <glm/vec3.hpp>

#include <filesystem>
#include <memory>

namespace Mood {

class AnimationSystem;
class NavSystem;
class ParticleSystem;
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
    std::unique_ptr<NavSystem>        m_navSystem;
    std::unique_ptr<ParticleSystem>   m_particleSystem;  // Hito 29
    TriggerSystem                     m_triggerSystem;   // Hito 33: stateless
    std::unique_ptr<PhysicsWorld>     m_physicsWorld;

    // Hito 30: character controller del jugador. 0 = no creado todavia.
    u32 m_playerCharId = 0;
    f32 m_jumpCooldown = 0.0f;   // segundos hasta el proximo salto permitido
    // Hito 34 C: coyote time + jump buffer (paridad con EditorApplication).
    f32 m_coyoteTimer = 0.0f;
    f32 m_jumpBufferTimer = 0.0f;
    bool m_spacePrevFrame = false;
    // F2H48: flanco up->down de la tecla E para interact con NPCs +
    // estados prev-frame de digits 1..9 para elegir choices durante
    // dialog activo.
    bool m_ePrevFrame = false;
    bool m_digitPrevFrame[9] = {false, false, false, false, false, false, false, false, false};
    // F2H52 H: Tab toggle del widget `inventory_panel`.
    bool m_tabPrevFrame = false;
    bool m_crouching = false;
    // Hito 40 G: ventanas configurables per-proyecto (cargadas del
    // `.moodproj`). Defaults coinciden con los hardcoded del Hito 34 C.
    f32 m_coyoteWindowSec     = 0.10f;
    f32 m_jumpBufferWindowSec = 0.15f;
    // Hito 31 D: crouch lerp visual + headbob (paridad con EditorApplication).
    f32 m_crouchVisualT = 0.0f;
    f32 m_headbobTime   = 0.0f;
    // Hito 34 D: velocidad horizontal normalizada [0..1] contra walkSpeed
    // — escala la amplitud del bob (paridad con EditorApplication).
    f32 m_horizSpeed01 = 0.0f;

    GridMap m_map{8u, 8u, 3.0f};

    // F2H41: spawn al centro del mapa (0,1.6,0). Mismo cambio que en
    // EditorApplication.h — convencion del motor.
    FpsCamera m_playCamera{glm::vec3(0.0f, 1.6f, 0.0f), -90.0f, 0.0f};

    static constexpr glm::vec3 k_playerHalfExtents{0.3f, 0.9f, 0.3f};

    // Tracking de la transicion del flag `GameState::paused()` para
    // sincronizar SDL_SetRelativeMouseMode una sola vez por toggle
    // (igual que EditorApplication::m_pausedLastFrame).
    bool m_pausedLastFrame = false;

    Timer m_deltaTimer;
    bool  m_running = true;

    // Hito 38 B: Main Menu state. true = mostrar New/Load/Quit;
    // false = juego corriendo. La transicion main_menu → game se gatilla
    // desde los botones del menu o tras un Load exitoso.
    bool m_inMainMenu = true;
    // Path al `.moodmap` activo. Se setea en tryLoadGameManifest +
    // buildTestMap; lo persiste el SaveLoad::save y lo lee Load Game
    // para verificar coincidencia (v1: no cambia de mapa, asume que
    // el save coincide con el map actualmente cargado).
    std::filesystem::path m_currentMapPath;
    /// Hito 38 B: render del Main Menu sobre el viewport. Tres botones:
    /// New Game (sale del menu y resetea GameState), Load Game (pfd
    /// dialog .moodsave + apply state), Quit (m_running=false).
    void drawMainMenu();
    /// Hito 38 B: aplica un SaveData al estado runtime (hud + char pos +
    /// camara yaw/pitch). Asume que el map actualmente cargado coincide
    /// con `data.mapPath` (no cambia de mapa).
    void applyLoadedSave(const SaveLoad::SaveData& data);
    /// Hito 38 B: arma un SaveData con el state actual y lo escribe a
    /// `<exeDir>/quicksave.moodsave` (sin file dialog — F5 keyboard
    /// shortcut). Loguea el resultado.
    void quickSave();
    /// Hito 41 fix-up: idem quickSave pero con file dialog `pfd::save_file`
    /// para que el dev elija nombre + ubicacion. Atajo F6 durante el juego.
    void saveAs();
    /// Helper: arma el SaveData del state runtime actual. Reusado por
    /// quickSave + saveAs.
    SaveLoad::SaveData captureCurrentState();
};

} // namespace Mood
