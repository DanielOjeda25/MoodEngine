#pragma once

// Componentes de datos para el ECS (Hito 7). Todos son POD (no logica en
// metodos); los sistemas operan sobre ellos iterando via `Scene::forEach`.
//
// Convenciones:
//   - POD + constructores convenientes donde ayudan al call-site.
//   - Ningun componente hace I/O ni toca GL.
//   - MeshRendererComponent guarda punteros no-owning; AssetManager es
//     dueno del ciclo de vida.

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h" // TextureAssetId, AudioAssetId, MeshAssetId
#include "engine/audio/device/AudioDevice.h"    // SoundHandle
#include "engine/scripting/exposed/ExposedProperty.h" // Hito 24

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Mood {

/// @brief Nombre legible de la entidad (para Hierarchy, logs, debug).
struct TagComponent {
    std::string name;

    TagComponent() = default;
    TagComponent(std::string n) : name(std::move(n)) {}
};

/// @brief Transform 3D con posicion / rotacion Euler (grados) / escala.
///        Rotacion euler simplifica la UI del Inspector; si aparecen bugs
///        de gimbal lock, migrar a quat internamente.
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f}; // X=pitch, Y=yaw, Z=roll; en grados
    glm::vec3 scale{1.0f};

    TransformComponent() = default;
    TransformComponent(glm::vec3 p, glm::vec3 s = glm::vec3(1.0f))
        : position(p), scale(s) {}

    /// @brief Matriz de modelo en coords de mundo. Orden: T * Ry * Rx * Rz * S
    ///        (yaw-pitch-roll; convencion FPS).
    glm::mat4 worldMatrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m = glm::rotate(m, glm::radians(rotationEuler.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(rotationEuler.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(rotationEuler.z), glm::vec3(0, 0, 1));
        m = glm::scale(m, scale);
        return m;
    }
};

/// @brief Indica que la entidad se dibuja con un mesh + materiales asociados.
///        `mesh` es un id resolvible via `AssetManager::getMesh`; si el id es
///        invalido, getMesh() cae al slot 0 (cubo primitivo).
///        `materials` tiene un `MaterialAssetId` por submesh del MeshAsset.
///        Si es mas corto que el numero de submeshes, los submeshes
///        restantes usan el slot 0 (default material).
///
///        Antes del Hito 10 este componente tenia `IMesh* mesh + TextureAssetId
///        texture`. Hito 10: migrado a ids para persistir en .moodmap.
///        Hito 17: el slot pasa de `TextureAssetId` (textura sola) a
///        `MaterialAssetId` (material PBR completo). El upgrader del
///        SceneSerializer envuelve cada texture_path viejo (.moodmap v6)
///        en un material auto-generado al cargar.
struct MeshRendererComponent {
    MeshAssetId mesh = 0;                       // 0 = cubo primitivo (fallback)
    std::vector<MaterialAssetId> materials;     // 1 material por submesh

    MeshRendererComponent() = default;
    MeshRendererComponent(MeshAssetId m, MaterialAssetId mat)
        : mesh(m), materials{mat} {}
    MeshRendererComponent(MeshAssetId m, std::vector<MaterialAssetId> mats)
        : mesh(m), materials(std::move(mats)) {}

    /// @brief Devuelve el material del submesh i, o el slot 0 (default) si
    ///        la lista es mas corta.
    MaterialAssetId materialOrMissing(usize submeshIndex) const {
        return (submeshIndex < materials.size()) ? materials[submeshIndex] : 0;
    }
};

/// @brief Camara como componente. Stub por ahora; el editor sigue usando las
///        `EditorCamera`/`FpsCamera` dedicadas. Entra en uso real cuando
///        agreguemos sistema de "active camera" (Hito 13+).
struct CameraComponent {
    float fovDeg = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

/// @brief Luz. Activada en Hito 11 (Blinn-Phong forward).
///        - `Directional`: usa `direction` (no la posicion del Transform). Solo
///          se considera la PRIMERA encontrada (single sun).
///        - `Point`: usa la posicion del `TransformComponent`. Atenuacion
///          cuadratica suave hasta `radius`. Hasta MAX_POINT_LIGHTS=8 activas;
///          el resto se ignora con un warn al loguear.
struct LightComponent {
    enum class Type : u8 { Directional = 0, Point = 1 };

    Type type = Type::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;                    // solo Point
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  // solo Directional, normalizada
    bool enabled = true;
    bool castShadows = false;                // solo Directional (Hito 16)
};

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

/// @brief Rigid body (Hito 12). Presencia física en Jolt: gravedad y
///        colisiones. `bodyId` lo llena `PhysicsSystem` cuando crea el
///        body en el `PhysicsWorld`; hasta entonces vale 0.
///        Cambiar shape/mass en runtime requiere recrear el body (hoy solo
///        se permite fuera de Play Mode; el Inspector lo va a grayar).
struct RigidBodyComponent {
    enum class Type  : u8 { Static = 0, Kinematic = 1, Dynamic = 2 };
    enum class Shape : u8 { Box = 0, Sphere = 1, Capsule = 2 };

    Type  type  = Type::Dynamic;
    Shape shape = Shape::Box;
    glm::vec3 halfExtents{0.5f}; // Box: (x,y,z). Sphere: (r,_,_). Capsule: (halfH, r, _).
    f32 mass = 1.0f;              // kg, solo Dynamic
    // Hito 34 A: friction per-body. Default 0.5 (heredado del Hito 31 D —
    // realista para madera-sobre-madera). Aplica al body Static y Dynamic;
    // en Static no afecta al body en si pero si al contacto contra otros.
    // Editar desde el Inspector se aplica al re-materializar (proximo
    // entrar a Play Mode); no hay setter en runtime por ahora.
    f32 friction = 0.5f;

    u32 bodyId = 0;               // llenado por PhysicsSystem (0 = no creado)

    // F2H40: cache del ultimo halfExtents sincronizado al body Jolt.
    // Si `halfExtents != lastSyncedHalfExtents`, `updateRigidBodies`
    // llama `setBodyHalfExtents` para resincronizar. Cubre 2 vectores:
    //   1. Para Box bodies, auto-actualiza `halfExtents = t.scale*0.5`
    //      cuando el dev escala el Transform (Inspector / gizmo). Sin
    //      esto, el visual se ensancha pero la colision queda con el
    //      tamaño original — el player atraviesa o cae al vacio.
    //   2. Para cualquier shape, sincroniza si el dev edita
    //      `halfExtents` directo en el Inspector (Sphere radius,
    //      Capsule height, etc.).
    // NO se serializa (estado runtime). Default 0 fuerza primer sync
    // al materializar el body.
    glm::vec3 lastSyncedHalfExtents{0.0f};

    // Hito 41 fix-up #2: pending velocidades del Save/Load. Si
    // `applyLoadedSave` corre ANTES de que el body se materialice
    // (bodyId == 0), las vels del snapshot se quedan stash aca.
    // `updateRigidBodies` las aplica al body recien creado y limpia
    // el flag. NO se serializan en `.moodmap` ni `.moodsave` (estado
    // transitorio entre load y siguiente updateRigidBodies).
    bool hasPendingVel = false;
    glm::vec3 pendingLinearVel{0.0f};
    glm::vec3 pendingAngularVel{0.0f};

    RigidBodyComponent() = default;
    RigidBodyComponent(Type t, Shape s, glm::vec3 he, f32 m = 1.0f)
        : type(t), shape(s), halfExtents(he), mass(m) {}
};

/// @brief Configuracion del entorno de render (Hito 15 Bloque 4):
///        skybox + fog + post-process (exposure + tonemap). Se agrega a UNA
///        entidad cualquiera de la scene (convencion: un objeto vacio
///        llamado "Environment"). Si hay mas de una entidad con este
///        componente, `EditorApplication` usa la primera que encuentra.
///        Si no hay ninguna, los uniforms quedan en sus defaults.
///
///        El campo `skyboxPath` esta reservado para un futuro asset
///        catalog de skyboxes (cubemap por proyecto). Hito 15 sigue
///        usando un cubemap fijo cargado al iniciar (`sky_day`); el campo
///        se persiste pero no se aplica todavia.
struct EnvironmentComponent {
    // Skybox (placeholder hasta que haya catalogo de cubemaps).
    std::string skyboxPath{"skyboxes/sky_day"};

    // Fog
    u32 fogMode = 0;                    // 0=Off, 1=Linear, 2=Exp, 3=Exp2
    glm::vec3 fogColor{0.55f, 0.65f, 0.75f};
    float fogDensity = 0.015f;
    float fogLinearStart = 5.0f;
    float fogLinearEnd = 50.0f;

    // Post-process
    float exposure = 0.0f;              // EVs
    u32 tonemapMode = 2;                // 0=None, 1=Reinhard, 2=ACES

    // IBL intensity (Hito 18). Multiplicador del aporte del IBL al
    // ambient del PBR. 1.0 = aporte completo del cubemap; 0.0 = sin
    // IBL (cae a `uAmbient` escalar). Util cuando el cubemap es muy
    // claro y "ahoga" las point lights y el directional. Tipicos:
    // 0.4-0.7 para escenas con luces directas; 1.0 para escenas
    // exteriores donde el cielo manda.
    float iblIntensity = 1.0f;
};

/// @brief Marca a una entidad como instancia de un prefab (Hito 14).
///        Solo guarda el path logico del `.moodprefab` que la origino — sin
///        propagacion bidireccional (cambiar el prefab no actualiza esta
///        instancia, eso queda para hitos posteriores).
///        El SceneSerializer persiste este path como `prefab_path` en el
///        `.moodmap`, asi un round-trip preserva el link.
struct PrefabLinkComponent {
    std::string path; // logico, ej. "prefabs/torch.moodprefab"

    PrefabLinkComponent() = default;
    PrefabLinkComponent(std::string p) : path(std::move(p)) {}
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
struct AnimatorComponent {
    std::string clipName;          // vacio = primer clip del MeshAsset
    float time = 0.0f;              // segundos desde el inicio del clip
    float speed = 1.0f;             // escala temporal (1=normal, 2=doble, 0=pause)
    bool playing = true;
    bool loop = true;
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
    // Default Point = comportamiento del Hito 29 (todas spawnean en el
    // centro). Box, Sphere, Disc, Cone dan volumetric emit.
    // `emissionShapeSize` se interpreta segun el shape:
    //   Box:    halfExtents iguales (cubo de lado 2*size).
    //   Sphere: radio.
    //   Disc:   radio del disco en el plano XZ local del emisor.
    //   Cone (Hito 39 A): radio de la base; altura del cono fija a
    //          `emissionShapeSize`. Axis = +Y. Particula spawneada en
    //          un disc cuya area decrece linealmente con la altura
    //          (uniform sampling sobre el volumen del cono).
    enum class EmissionShape : u8 { Point = 0, Box = 1, Sphere = 2, Disc = 3, Cone = 4 };
    EmissionShape emissionShape = EmissionShape::Point;
    f32           emissionShapeSize = 1.0f;
    // Hito 40 A: axis del cono (solo aplica si emissionShape == Cone).
    // Default +Y. Se asume normalizado; si el caller pasa (0,0,0) el
    // sample fallback usa +Y. Editar via Inspector con DragFloat3.
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
///
/// Hito 36 C: agrega callback `on_trigger_stay` cada frame que el player
/// sigue dentro (sin flank).
/// Hito 37 B: tambien detecta entidades con `RigidBodyComponent` (Dynamic +
/// Kinematic). Para esas dispatcha `on_trigger_body_enter(bodyEntId)` /
/// `on_trigger_body_exit(bodyEntId)` / `on_trigger_body_stay(bodyEntId)`
/// — el arg es el handle raw del entt::entity del body. `bodiesInside`
/// guarda el set runtime de bodies actualmente dentro (no serializado).
struct TriggerComponent {
    glm::vec3 halfExtents{1.0f, 1.0f, 1.0f}; // 2x2x2m por default
    // Estado runtime (no serializado): true mientras el jugador este
    // dentro. TriggerSystem detecta el flanco (false→true / true→false).
    bool playerInside = false;
    // Hito 37 B: set runtime de bodies actualmente dentro del AABB.
    // Forward decl-friendly: usamos entt::entity raw (typedef u32) en
    // lugar de incluir <entt/entt.hpp> aca.
    std::unordered_set<u32> bodiesInside;
};

} // namespace Mood
