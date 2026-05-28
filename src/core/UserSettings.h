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

} // namespace Mood::UserSettings
