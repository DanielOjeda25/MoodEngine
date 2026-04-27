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
#include "engine/assets/AssetManager.h" // TextureAssetId, AudioAssetId, MeshAssetId
#include "engine/audio/AudioDevice.h"    // SoundHandle

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <string>
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

    u32 bodyId = 0;               // llenado por PhysicsSystem (0 = no creado)

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

} // namespace Mood
