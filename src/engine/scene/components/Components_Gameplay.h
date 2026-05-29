#pragma once

// F2H81 (auditoría): split de Components.h por categoría. Componentes de
// gameplay / lógica / audio / animación / partículas / triggers. Incluir
// via `Components.h`.

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h" // TextureAssetId, AudioAssetId, AnimationClipAssetId
#include "engine/audio/device/AudioDevice.h"    // SoundHandle
#include "engine/inventory/InventoryState.h"   // F2H51: estado del InventoryComponent
#include "engine/scripting/exposed/ExposedProperty.h" // Hito 24

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Mood {

/// @brief Behavior en Lua (Hito 8). `path` es logico (ej. "scripts/rotator.lua").
///        `loaded` lo flipea `ScriptSystem` cuando carga con exito;
///        `lastError` guarda el ultimo mensaje para mostrarlo en el Inspector.
///        El `sol::state` del script NO vive aca (sol::state no es copiable);
///        lo maneja el `ScriptSystem` en un mapa `entt::entity -> sol::state`.
struct ScriptComponent {
    std::string path;
    bool loaded = false;
    std::string lastError;

    /// Hito 24: exposed properties.
    /// `exposedProps` se rellena al cargar el script (engine.exposed
    /// llamadas registran (name, type, default) aca). NO se serializa
    /// — se redescubre cada carga.
    /// `overrides` es editable desde el Inspector y persistido en
    /// `.moodmap`. Cuando engine.exposed("name", default) lo encuentra,
    /// devuelve el override en vez del default.
    std::vector<ExposedProperty> exposedProps;
    std::unordered_map<std::string, ExposedValue> overrides;

    ScriptComponent() = default;
    ScriptComponent(std::string p) : path(std::move(p)) {}
};

/// @brief Fuente de audio asociada a una entidad (Hito 9). Reproducción
///        manejada por `AudioSystem`. Si `is3D`, usa el `TransformComponent`
///        de la misma entidad para posicionar; si no, se mezcla plano.
///
/// El `SoundHandle` se setea cuando `AudioSystem` arranca la reproducción
/// (los clips con `playOnStart=true` arrancan en el primer update). El flag
/// `started` evita que un playOnStart dispare varias veces.
struct AudioSourceComponent {
    AudioAssetId clip = 0;          // 0 = missing silencio; default inocuo
    float volume = 1.0f;            // lineal, escalado al master del device
    bool loop = false;
    bool playOnStart = true;        // dispara en el primer update del sistema
    bool is3D = false;              // usa TransformComponent para posicion

    SoundHandle handle = 0;         // llenado por AudioSystem cuando play
    bool started = false;           // guard contra re-disparo de playOnStart

    AudioSourceComponent() = default;
    AudioSourceComponent(AudioAssetId c) : clip(c) {}
};

/// @brief Hito 19: marca a una entidad como animable y elige el clip.
///        El esqueleto vive en el MeshAsset (no se duplica acá). El time
///        avanza por delta cada frame en `AnimationSystem`. `clipName`
///        vacio -> primer clip del MeshAsset (default sensato).
///
/// F2H49: ademas de los clips embebidos en el MeshAsset (resueltos por
/// `clipName`), se pueden adjuntar clips standalone cargados desde FBX
/// sin malla (anim_walk.fbx, anim_idle.fbx, etc.) via `externalClips`.
struct AnimatorComponent {
    std::string clipName;          // vacio = primer clip del MeshAsset
    float time = 0.0f;              // segundos desde el inicio del clip
    float speed = 1.0f;             // escala temporal (1=normal, 2=doble, 0=pause)
    bool playing = true;
    bool loop = true;

    /// Clips standalone adjuntos por alias logico ("walk", "idle", ...).
    /// El alias es la clave que el gameplay / CharacterController usa para
    /// pedir reproduccion; el AssetId apunta al `AnimationClip` cacheado
    /// en `AssetManager`. Vacio = no hay clips externos, el animator solo
    /// usa los embebidos del MeshAsset.
    std::vector<std::pair<std::string, AnimationClipAssetId>> externalClips;

    /// Remap `clipBoneIndex -> skeletonBoneIndex` por clip, calculado por
    /// `AnimationSystem` en el primer uso (lookup de `track.boneName` en
    /// el esqueleto destino). Vacio = todavia no bindeado. Misma key que
    /// los AssetIds de `externalClips`.
    std::unordered_map<AnimationClipAssetId, std::vector<int>> externalBindCache;
};

/// @brief Hito 19: matrices de skinning ya compuestas (= globalPose *
///        inverseBind por hueso). El shader las sube como
///        `uBoneMatrices[]`. Lo recalcula `AnimationSystem` cada frame.
///        Esta cacheado en el componente (no en el sistema) porque
///        `EditorRenderPass` lo necesita por entidad — y el sistema corre
///        antes que el render del frame.
struct SkeletonComponent {
    std::vector<glm::mat4> skinningMatrices; // size == skeleton.bones.size()
};

/// @brief Hito 23: agente navegable. NavSystem lo procesa cada frame
///        para mover la entidad hacia `target` siguiendo paths del A*
///        sobre el GridMap. La entidad NO debe tener tambien
///        RigidBodyComponent::Dynamic — el NavSystem ya hace
///        moveAndSlide y crear ambos handlers daria peleas de
///        autoridad sobre el Transform.
struct NavAgentComponent {
    /// @brief Posicion world-space hacia donde el agente trata de ir.
    ///        El caller (sistema o script) la actualiza por frame; el
    ///        NavSystem detecta cambios > 1 tile y recomputa el path.
    glm::vec3 target{0.0f};
    /// @brief Velocidad de avance en m/s. Default = 2 m/s ~ caminar.
    f32 speed = 2.0f;
    /// @brief Si false, el sistema lo skipea (idle).
    bool active = true;

    /// --- Estado interno (no serializar) ---
    /// Path en grid coords desde la posicion actual hasta target. Se
    /// recomputa segun `repathAccumulator` o si target se aleja
    /// > tileSize del ultimo target pathed.
    std::vector<glm::ivec2> path;
    /// Indice del proximo waypoint en `path` que el agente esta
    /// caminando. Si `pathIndex >= path.size()` el agente llego.
    usize pathIndex = 0;
    /// Acumulador para throttle de re-pathfinding (cada 0.5s).
    f32 repathAccumulator = 0.0f;
    /// Target world-space del ultimo path computado — para detectar
    /// si el target real se movio mas que un tile y forzar repath.
    glm::vec3 lastPathTarget{1e9f};
};

/// @brief Hito 29: emisor de particulas CPU. Sistema de struct-of-arrays
///        per-emisor; `ParticleSystem` lo procesa en update. El render
///        (billboards) corre en el SceneRenderer despues de la geometria
///        opaca y antes del post-process. V1 sin sorting por depth — los
///        emisores con varias instancias superpuestas pueden mostrar
///        artifacts.
struct ParticleEmitterComponent {
    // Hito 37 C: shape de emision. Las particulas se sampean en una
    // region alrededor de la posicion del emisor segun este enum.
    enum class EmissionShape : u8 { Point = 0, Box = 1, Sphere = 2, Disc = 3, Cone = 4 };
    EmissionShape emissionShape = EmissionShape::Point;
    f32           emissionShapeSize = 1.0f;
    // Hito 40 A: axis del cono (solo aplica si emissionShape == Cone).
    glm::vec3     emissionConeAxis{0.0f, 1.0f, 0.0f};

    // --- Configuracion editable ---
    f32 emitRate     = 60.0f;          // particles/sec
    f32 lifetimeMin  = 1.0f;           // segundos
    f32 lifetimeMax  = 1.5f;
    glm::vec3 velocityMin{-0.4f, 1.0f, -0.4f}; // m/s, world-space
    glm::vec3 velocityMax{ 0.4f, 2.0f,  0.4f};
    f32 sizeStart    = 0.30f;          // metros (ancho del billboard)
    f32 sizeEnd      = 0.05f;
    glm::vec4 colorStart{1.0f, 0.75f, 0.2f, 1.0f}; // naranja
    glm::vec4 colorEnd  {1.0f, 0.10f, 0.0f, 0.0f}; // rojo transparente
    /// 0 = sin gravedad. 1 = gravedad real (-9.81 m/s^2 en Y). Negativo
    /// lo invierte (humo subiendo se modela mejor con gravityFactor>0
    /// y velocityMin/Max apuntando hacia +Y).
    f32 gravityFactor = 0.0f;
    TextureAssetId texture = 0;        // 0 = missing.png; el shader hace billboard
    u32 maxParticles  = 256;           // cap de la pool
    bool emitting     = true;          // false = pausa spawn, vivas siguen avanzando
    bool additive     = false;         // true = blend aditivo (fuego/sparks); false = alpha (humo)
    /// Hito 31 F: localSpace=true => positions/velocities almacenadas en
    /// el espacio local de la entidad. Cuando la entidad se mueve, las
    /// particulas la siguen (humo en una chimenea que viaja, sparks
    /// pegadas a un personaje). Default false = world-space (las
    /// particulas se desprenden del emisor).
    bool localSpace   = false;

    // --- Estado runtime (NO serializar) ---
    f32 emitAccumulator = 0.0f;        // particulas pendientes (fraccional)
    u64 rngState        = 0xC0FFEEu;   // xorshift64 — se inicializa al primer update si vale 0

    // Struct-of-arrays. Tamano = maxParticles. `alive[i]==0` indica slot libre
    // — el spawn nuevo recicla el primero que encuentre. Reservados al primer
    // update para no inflar el componente cuando el dev solo lo lista en el
    // Inspector sin entrar Play.
    std::vector<glm::vec3> positions;   // world-space
    std::vector<glm::vec3> velocities;  // m/s
    std::vector<f32>       ages;        // segundos desde spawn
    std::vector<f32>       lifetimes;   // total por particula (lerp uniforme entre min/max)
    std::vector<u8>        alive;       // 0/1
    u32 aliveCount = 0;
};

/// @brief Trigger volume (Hito 33). Detecta cuando el jugador entra/sale
///        de su AABB y dispatcha `on_trigger_enter` / `on_trigger_exit`
///        al script de la entidad (si tiene ScriptComponent).
///        Sin colision solida — el char puede atravesarlo libremente.
///
/// AABB: centro = TransformComponent.position; tamaño = halfExtents * 2.
/// halfExtents NO usa el scale del Transform; representan metros directos.
struct TriggerComponent {
    glm::vec3 halfExtents{1.0f, 1.0f, 1.0f}; // 2x2x2m por default

    // --- F2H73: triggers avanzados ---
    /// @brief Si no-vacio, los eventos de body (`on_trigger_body_*`) solo
    ///        disparan para entities cuyo TagComponent.name == requiredTag
    ///        (filtro por tipo, estilo Unity layers / Unreal class filter).
    ///        Vacio = cualquier body. No afecta al player (ver triggersOnPlayer).
    std::string requiredTag;
    /// @brief Si false, el trigger ignora al char del player (solo reacciona
    ///        a bodies). Default true (comportamiento clasico).
    bool triggersOnPlayer = true;
    /// @brief Si true, tras el PRIMER enter (player o body) el trigger se
    ///        marca `fired` y deja de disparar (checkpoints, cutscenes
    ///        one-time). Se re-arma al recargar el mapa (fired no persiste).
    bool oneShot = false;
    /// @brief Master switch — un script puede apagar/prender el trigger
    ///        (`hud`/entity API). Disabled = no dispatcha nada.
    bool enabled = true;

    // --- Estado runtime (NO serializado) ---
    /// true mientras el jugador este dentro. TriggerSystem detecta el flanco.
    bool playerInside = false;
    /// F2H73: true una vez que un oneShot disparo su enter. Mientras sea
    /// true el trigger no vuelve a procesar. Arranca false al cargar.
    bool fired = false;
    // Hito 37 B: set runtime de bodies actualmente dentro del AABB.
    // Forward decl-friendly: usamos entt::entity raw (typedef u32) en
    // lugar de incluir <entt/entt.hpp> aca.
    std::unordered_set<u32> bodiesInside;
};

/// F2H48: marca una entidad como NPC con un dialog asociado. `dialogPath`
/// es el path logico del `.mooddialog` (resolvible por AssetManager via
/// VFS). `autoStartOnInteract`: si true y la entidad tambien tiene un
/// `TriggerComponent`, el sistema de dialog auto-dispara `start()` cuando
/// el player presiona E dentro del trigger.
struct DialogComponent {
    std::string dialogPath;                  // p.ej. "dialogs/intro.mooddialog"
    bool        autoStartOnInteract = true;  // default = ergonomico
    // Estado runtime (no serializado): hash del path ya cargado para
    // evitar `loadDialog` redundante por frame mientras el player esta
    // dentro del trigger. 0 = no cargado todavia.
    u32         cachedDialogId = 0;
};

/// F2H51: Inventario engine-grade attachable a CUALQUIER entidad (player,
/// NPC, chest, vendor, drop pile). 3 layout modes configurables por
/// instancia (lista plana / grid 2D / equipment slots). El motor NO impone
/// semantica de gameplay — el dev del juego decide. Toda la logica vive
/// en `Inventory::State` (testeable sin ImGui).
struct InventoryComponent {
    Inventory::State state;  // mode + config + entries
};

/// F2H52: Marca a una entidad como "item pickeable en el mundo". Cuando el
/// player entra al `TriggerComponent` adjunto y presiona E, el
/// `ItemPickupSystem` agrega el item a su `InventoryComponent` (via
/// `inventory.add(player, itemPath, quantity)`) y dispara el hook Lua
/// `on_pickup`. Si `destroyOnPickup=true` (default), la entidad se elimina.
struct ItemPickupComponent {
    std::string itemPath;       // p.ej. "items/iron_sword.mooditem"
    int         quantity = 1;
    /// @brief Default true: pickup desaparece del mundo al levantarlo. Caso
    ///        comun (~90% de los items). False permite "dispenser infinito"
    ///        para test fixtures, maquinas que dan pociones, etc.
    bool        destroyOnPickup = true;
    /// @brief Estado runtime (no serializado): cache del `ItemAssetId`
    ///        re-resuelto via `AssetManager::loadItem` al primer trigger
    ///        para evitar lookups por frame mientras el player esta dentro.
    ///        Mismo patron que `DialogComponent::cachedDialogId`. 0 = no
    ///        cargado todavia.
    u32         cachedItemId = 0;
};

/// @brief F4H1 — Salud de una entidad (cimiento del combate de Fase 4).
///        Plain data: `current`/`max` en hit points (`current==0 && dead`).
///        `lastDamageTime` (segundos desde Play mode start) y
///        `hitFlashTimer` (segundos restantes del flash blanco on-hit)
///        son transients runtime — no se serializan.
struct HealthComponent {
    f32  current        = 100.0f;
    f32  max            = 100.0f;
    bool dead           = false;
    /// Transients (no serializar):
    f32  lastDamageTime = -1.0f;
    f32  hitFlashTimer  = 0.0f;
};

/// @brief F4H2 — Entidad efimera de particula one-shot (impact burst).
///        Usado por el WeaponSystem para los puffs de impacto. El
///        WeaponSystem::tickSystem decrementa `ttl` y destruye la
///        entidad cuando llega a 0. No se serializa (es transient).
struct ParticleBurstComponent {
    f32 ttl = 1.0f;
};

/// @brief F4H2 — Arma equipada por una entidad (engine-generic).
///        Plain data: solo `weaponAssetId` (ref al `.moodweapon` cargado
///        en `AssetManager`) + `currentAmmo` + timers. El motor NO
///        guarda los stats — vienen del Spec via `AssetManager::getWeapon`.
///        Cambiar de arma = cambiar `weaponAssetId`.
///        `weaponAssetId == 0` significa "sin arma equipada" — el
///        `WeaponSystem::fire` lo trata como no-op.
struct WeaponComponent {
    /// @brief Id del WeaponSpec equipado. 0 = sin arma. Resuelto por
    ///        el AssetManager desde el path logico al cargar la escena.
    u32  weaponAssetId  = 0;

    /// @brief Munición actual en el mag. Inicializado a `spec.magazineSize`
    ///        al equipar el arma por primera vez. -1 = "todavía no
    ///        inicializado" (auto-fill al primer fire/Inspector view).
    int  currentAmmo    = -1;

    /// Transients (no serializar):
    /// @brief Cooldown del proximo disparo (segundos). Reset al firing
    ///        rate del Spec al disparar. WeaponSystem::tick lo decae.
    f32  fireTimer      = 0.0f;
    /// @brief Tiempo restante del reload (segundos). > 0 = reloading.
    f32  reloadTimer    = 0.0f;
    /// @brief Bandera del frame: true si el input de fire esta sostenido.
    ///        El bridge de input la setea cada frame; WeaponSystem la lee.
    ///        Auto-clear al final del tick para evitar arrastre.
    bool firing         = false;
};

} // namespace Mood
