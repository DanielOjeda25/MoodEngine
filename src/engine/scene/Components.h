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
///        `materials` tiene una `TextureAssetId` por submesh del MeshAsset; si
///        es mas corto que el numero de submeshes, los submeshes restantes
///        usan el slot 0 (missing.png).
///
///        Antes del Hito 10 este componente tenia `IMesh* mesh + TextureAssetId
///        texture`. La migracion a ids permite (a) persistirlo en .moodmap
///        (paths logicos estables), (b) soportar modelos importados con
///        multiples submeshes, (c) swapear el mesh en el Inspector sin tocar
///        el resto de la entidad.
struct MeshRendererComponent {
    MeshAssetId mesh = 0;                       // 0 = cubo primitivo (fallback)
    std::vector<TextureAssetId> materials;      // 1 textura por submesh

    MeshRendererComponent() = default;
    MeshRendererComponent(MeshAssetId m, TextureAssetId t) : mesh(m), materials{t} {}
    MeshRendererComponent(MeshAssetId m, std::vector<TextureAssetId> mats)
        : mesh(m), materials(std::move(mats)) {}

    /// @brief Devuelve la textura del submesh i, o el slot 0 (missing) si
    ///        la lista es mas corta.
    TextureAssetId materialOrMissing(usize submeshIndex) const {
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

} // namespace Mood
