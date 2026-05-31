#pragma once

// UserSettings (F2H43): preferencias del usuario persistidas a un JSON
// global por usuario (no por proyecto). Vive en
// `%APPDATA%\MoodEngine\settings.json` (Windows). Compartido entre
// MoodEditor y MoodPlayer — ambos llaman `init()` al arrancar y leen
// el idioma persistido para pasarselo a `I18n::init()`.
//
// Convencion: el setter marca dirty pero NO escribe al disco. El caller
// es responsable de llamar `save()` cuando corresponde (al cambiar
// idioma desde el menu, al salir del programa, etc). Esto evita writes
// accidentales por cambios transitorios.

#include "core/Types.h"
#include "core/i18n/I18n.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Mood::UserSettings {

/// F3H21: modos de visualizacion del viewport perspectivo del editor.
/// Convencion Blender (Z dropdown) / Unreal (Alt+1/2/3/4) / Unity
/// (Shaded/Wireframe/Shaded+Wireframe). MaterialPreview es el default
/// — equivalente a "Solid" de Blender pero con texturas.
enum class ViewportRenderMode : int {
    Wireframe       = 0,  ///< Solo edges, sin texturas.
    Solid           = 1,  ///< Flat shading, sin texturas (albedo plano).
    MaterialPreview = 2,  ///< Como ahora (texturas + ilum simple, sin shadows/SSR/bloom).
    Rendered        = 3,  ///< Full pipeline (==Play mode visual).
};

/// F3H7: preferencias del editor per-instalacion. Sensibilidades de
/// camara orto + tamano de gizmo + umbral click-vs-drag. NO viven en
/// `.moodproj` porque son ergonomia del dev (notebook touchpad vs mouse
/// 16K DPI, pantalla 4K vs 1080p), no decisiones del proyecto.
struct EditorSettings {
    /// Alto inicial del frustum ortografico en world units al crear un
    /// viewport orto nuevo. Default 32 — Hammer-style "ves ~32m". Mundo
    /// abierto subir; interior detallado bajar. El zoom posterior (wheel)
    /// modifica el state vivo de la camara, no este default.
    f32 orthoInitialZoom = 32.0f;

    /// Multiplicador por tick del wheel sobre la camara orto. Default
    /// 1.1 — cada wheel up reduce `worldHeight` por 1/1.1 (~9.1%) y cada
    /// wheel down lo aumenta. Subir = zoom mas agresivo.
    f32 orthoZoomFactor = 1.1f;

    /// Largo del brazo de los handles translate/scale del gizmo en
    /// pixeles de pantalla. Default 60 — calibrado para 1080p. En 4K
    /// el dev probablemente quiere ~100.
    f32 gizmoArmLengthPx = 60.0f;

    /// Tamano target del ring de rotate del gizmo en pixeles de pantalla.
    /// Default 70 (F2H35 fix: ring CONSTANTE en pantalla — pre-F2H35 era
    /// proporcional al AABB y se hacia muy chico al alejar la cam).
    f32 gizmoRotateRingPx = 70.0f;

    /// Umbral en pixeles para distinguir click puro de drag (LMB sobre
    /// viewport). Default 4 — clamp `>= 1`. Comparacion real es
    /// `dx*dx + dy*dy >= threshold*threshold` en los 2 viewports
    /// (perspectiva + orto). Subir si el trackpad genera drags
    /// accidentales.
    int clickDragThresholdPx = 4;

    /// F3H14: resolucion en pixeles de los thumbnails del Asset Browser
    /// (meshes). Default 128 — calibrado para grids 80-96 px en 1080p.
    /// En 4K subir a 192/256; en notebook bajar a 64 si hay >100 meshes
    /// para reducir VRAM. Clamp `[64, 512]`. Cambiar este valor invalida
    /// el cache en memoria; los PNGs en disco coexisten por size en el
    /// filename (no se borran al cambiar).
    int thumbnailResolution = 128;

    /// F3H16: milisegundos de hover prolongado antes de mostrar el
    /// tooltip ampliado del Asset Browser (preview grande + metadata).
    /// Default 500 ms — sensacion natural sin "saltar" al primer movimiento.
    /// Subir si el dev encuentra el tooltip intrusivo; bajar a 200 si
    /// quiere previews instantaneas. Clamp `[0, 3000]`.
    int hoverPreviewDelayMs = 500;

    /// F3H21: modo de visualizacion del viewport perspectivo (Wireframe /
    /// Solid / MaterialPreview / Rendered). Default MaterialPreview —
    /// equivalente al comportamiento pre-F3H21. Persistido per-instalacion
    /// porque es preferencia ergonomica (el dev que quiere ver wireframe
    /// siempre lo quiere asi, no por proyecto).
    ViewportRenderMode viewportRenderMode = ViewportRenderMode::MaterialPreview;

    /// F3H21: ON = transicion lerp al cambiar de vista numpad (Blender
    /// Smooth View); OFF = teleport instantaneo (Unity/Unreal). Default
    /// ON — el lerp evita la desorientacion al saltar entre vistas
    /// ortogonales. Si el dev prefiere instantaneo (mouse-heavy workflow
    /// o trackpad lento), apagar.
    bool smoothViewEnabled = true;

    /// F3H21: duracion del lerp numpad en milisegundos. Default 200 ms
    /// (Blender Smooth View timer). Clamp `[0, 1000]` — 0 = teleport
    /// (equivalente a smoothViewEnabled=false), 1000 = casi medio segundo
    /// (demasiado lento para uso frecuente).
    int smoothViewDurationMs = 200;

    /// F3H29: far plane de la EditorCamera + FpsCamera (Play mode) en
    /// world units. Default 1000.0 (era 100.0 hardcoded en F2H17). Para
    /// mapas urbanos / arenas medianas; subir a 10000+ para open-world.
    /// Clamp `[100, 100000]` en sanitize — bajo de 100 el dev pierde la
    /// vista interior, encima de 100km la precision Z degrada al punto
    /// de z-fighting masivo (Reverse-Z queda como hito propio si emerge).
    /// El default que CameraComponent gana al spawnearse desde el editor
    /// tambien lee este valor (D3 — coherencia editor/gameplay default).
    f32 editorCameraFarPlane = 1000.0f;

    /// F3H29: radius maximo de la EditorCamera orbital (clamp upper de
    /// `m_radius`). Default 500.0 (era 50.0 hardcoded). Permite alejarse
    /// para ver mapas grandes / brushes enormes. Clamp `[10, 50000]` en
    /// sanitize. No afecta zoom min (sigue 0.5 hardcoded, no tiene
    /// sentido alejarlo).
    f32 editorCameraMaxOrbitRadius = 500.0f;

    /// F3H22: categoria activa del Inspector con icons laterales (estilo
    /// Properties Editor de Blender). IDs validos:
    ///   "object"      — Transform + Tag + VisGroup
    ///   "render"      — MeshRenderer + Brush + Light + Camera + Particles
    ///   "animation"   — Animator
    ///   "audio"       — AudioSource + Listener
    ///   "physics"     — RigidBody + Collider + Joint + Ragdoll + Cloth + Trigger + ForceField
    ///   "gameplay"    — Script + Inventory + ItemPickup + Dialog + Vehicle + Quest
    ///   "environment" — EnvironmentComponent (singleton scene-wide, accesible sin selección)
    /// Default "object" — siempre presente (toda entity tiene Transform).
    /// Sanitize: ID desconocido cae a "object" silencioso (sin log).
    std::string inspectorActiveCategory = "object";

    /// F3H23: stats overlay del viewport (estilo Unity bottom bar — single
    /// line al pie con chips por metric). Cada flag controla un widget
    /// independiente; el dev toggleta desde Preferences > Editor > Stats
    /// Overlay. Defaults: FPS + drawcalls + tris ON (las 3 métricas más
    /// usadas en day-to-day); memoria + lights + entities OFF (info
    /// adicional, on-demand).
    struct StatsOverlaySettings {
        bool showFps       = true;
        bool showDrawcalls = true;
        bool showTris      = true;
        bool showMemGpu    = false;
        bool showMemCpu    = false;
        bool showLights    = false;
        bool showEntities  = false;

        /// Helper para saber si hay al menos un widget activo (gate del
        /// render del bottom bar — sin chips activos, no se dibuja nada).
        bool anyEnabled() const {
            return showFps || showDrawcalls || showTris
                || showMemGpu || showMemCpu || showLights || showEntities;
        }
    };
    StatsOverlaySettings statsOverlay{};

    /// F3H23: tamano del ring buffer del profiler in-engine. N frames de
    /// historia para mostrar avg/min/max + histograma. Default 240 (≈4s a
    /// 60fps). Clamp `[60, 1200]` — debajo no hay suficiente data para
    /// detectar spikes, encima la memoria del ring se infla sin upside.
    int profilerFrameCount = 240;

    /// F3H24: toasts no-modales en la esquina inferior derecha (estilo
    /// VSCode). Default ON — el dev los apaga si quiere modo "headless".
    /// Cuando off, `Toasts::push` es no-op silencioso (los logs del
    /// LogRingSink siguen capturando todo, solo se omite el chip visual).
    bool toastsEnabled = true;

    /// F3H24: lifetime default de los toasts en milisegundos. Default
    /// 3000 ms (3 segundos — VSCode usa ~5s, Unity ~3s; 3s sentí más
    /// natural para mensajes de "Guardado" / "Asset importado"). Clamp
    /// `[500, 10000]`: debajo de 500 ms es subliminal; encima de 10s
    /// los toasts se acumulan visualmente.
    int toastsLifetimeMs = 3000;

    /// F3H25: autosave del mapa actual. Default ON — el dev lo apaga si
    /// trabaja sobre filesystems lentos o no quiere I/O periódico. Cuando
    /// off, el timer no dispara writes (el flujo de save manual sigue OK).
    bool autosaveEnabled = true;

    /// F3H25: intervalo del autosave en minutos. Default 5 min — Unity
    /// usa 5, Unreal usa 5 (configurable), Blender usa 2. Clamp `[1, 60]`.
    /// El autosave sólo dispara si el mapa actual está dirty
    /// (`m_projectDirty=true`); si no, skip silencioso para ahorrar I/O.
    int autosaveIntervalMin = 5;
};

/// F4H2 Bloque B: keybindings data-driven del player. Mapa
/// nombre-de-acción → identificador del binding (string portable: nombre
/// de tecla en lowercase o `mouse_left/right/middle`). El binding se
/// resuelve a SDL scancode / mouse button con `InputActions::resolveBinding`.
///
/// Defaults: `fire="mouse_left"`, `reload="r"`. El dev puede agregar más
/// (interact, jump, etc) editando `settings.json` o futuro UI rebindable.
///
/// Ejemplo de keybindings válidos:
///   "mouse_left" / "mouse_right" / "mouse_middle"
///   "r" / "e" / "space" / "lshift" / "lctrl"
struct InputSettings {
    /// Mapa acción → binding. Los lookups en `InputActions::isActionPressed`
    /// son case-insensitive sobre el binding (no sobre la acción).
    std::unordered_map<std::string, std::string> keybindings;

    InputSettings() {
        keybindings["fire"]        = "mouse_left";
        keybindings["reload"]      = "r";
        // F4H3 — bindings de swap de armas (HL/Apex style).
        keybindings["weapon_next"] = "mouse_wheel_up";
        keybindings["weapon_prev"] = "mouse_wheel_down";
        keybindings["weapon_last"] = "q";
        keybindings["weapon_1"]    = "1";
        keybindings["weapon_2"]    = "2";
        keybindings["weapon_3"]    = "3";
        keybindings["weapon_4"]    = "4";
    }
};

/// @brief Lee `settings.json` del disco. Si no existe o tiene parse
///        error, usa defaults (idioma=Spanish) y NO crea el archivo
///        (se creara en el primer `save()`). Llamar UNA vez al arrancar
///        antes de `I18n::init()`.
void init();

/// @brief No-op por ahora; reservado para futuro flush al disco si se
///        agrega autosave-on-change.
void shutdown();

/// @brief Escribe el estado actual al disco. Crea el directorio
///        `%APPDATA%\MoodEngine\` si no existe. Loggea warn si falla.
bool save();

/// @brief Idioma cargado de `settings.json` (o default Spanish si no
///        habia archivo).
I18n::Language language();

/// @brief Marca el idioma nuevo en memoria. NO escribe al disco — el
///        caller llama `save()` cuando corresponde.
void setLanguage(I18n::Language lang);

/// @brief F2H76: id del tema visual del editor (ej. "dark", "light",
///        "midnight"). Cargado de `settings.json`; default "dark" si no
///        habia archivo o campo. La resolucion id -> paleta ImGui la hace
///        `EditorThemes::applyTheme` (el id desconocido cae a "dark").
const std::string& theme();

/// @brief Marca el tema nuevo en memoria. NO escribe al disco — el caller
///        llama `save()` cuando corresponde.
void setTheme(const std::string& id);

/// @brief Path absoluto al `settings.json`. Util para debug/test.
std::filesystem::path settingsPath();

/// @brief F3H7: editor preferences loaded from `settings.json` (defaults
///        si no habia subkey).
const EditorSettings& editor();

/// @brief Marca los editor settings nuevos en memoria. NO escribe al
///        disco — el caller llama `save()` cuando corresponde.
void setEditor(const EditorSettings& s);

/// @brief F3H7: serializa el struct a JSON. Solo escribe fields que
///        difieren del default — devuelve object vacio si todo es
///        default (el caller puede chequear `.empty()` para decidir si
///        incluir la subkey "editor" en settings.json).
nlohmann::json editorSettingsToJson(const EditorSettings& s);

/// @brief F3H7: lee + sanitize. Fields ausentes/invalidos → defaults.
///        Valores fuera de rango sano se clampean (zoom factor > 1.0,
///        thresholds >= 1, sizes > 0). JSON no-object → defaults.
EditorSettings editorSettingsFromJson(const nlohmann::json& j);

/// @brief F4H2 Bloque B: input keybindings del player (per-instalación).
const InputSettings& input();

/// @brief F4H2 Bloque B: marca el input nuevo en memoria. NO escribe al
///        disco — el caller llama `save()` cuando corresponde.
void setInput(const InputSettings& s);

/// @brief F4H2 Bloque B: serializa el struct a JSON. Sólo emite el mapa
///        `keybindings` si difiere del default (fire=mouse_left, reload=r).
nlohmann::json inputSettingsToJson(const InputSettings& s);

/// @brief F4H2 Bloque B: lee + sanitize. JSON no-object → defaults. Keys
///        no-string del mapa se ignoran silenciosamente.
InputSettings inputSettingsFromJson(const nlohmann::json& j);

} // namespace Mood::UserSettings
