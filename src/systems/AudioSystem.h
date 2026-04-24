#pragma once

// AudioSystem (Hito 9 Bloque 3): iterate entidades con `AudioSourceComponent`
// y delega reproduccion al `AudioDevice`. No es dueno del device — lo recibe
// por referencia desde `EditorApplication`.
//
// Convenciones:
//   - playOnStart: dispara en el primer `update` en que la entidad es visible
//     al sistema. `started` evita re-disparos.
//   - is3D: pos se lee del `TransformComponent` de la misma entidad y se
//     refresca cada frame.
//   - Al hacer `rebuildSceneFromMap` (clear del registry), el caller llama
//     `AudioSystem::clear()` para detener handles que apuntaban a entidades
//     destruidas.

#include "core/Types.h"
#include "engine/audio/AudioDevice.h"

#include <glm/vec3.hpp>

namespace Mood {

class Scene;
class AssetManager;

class AudioSystem {
public:
    explicit AudioSystem(AudioDevice& device, AssetManager& assets)
        : m_device(device), m_assets(assets) {}

    /// @brief Procesa `AudioSourceComponent` por frame.
    ///        - playOnStart: dispara la reproduccion y guarda el handle.
    ///        - is3D: actualiza posicion del sound handle.
    void update(Scene& scene, f32 dt);

    /// @brief Setea el listener (posicion + orientacion) para la
    ///        atenuacion 3D. Llamar una vez por frame con la camara activa.
    void setListener(const glm::vec3& worldPos,
                     const glm::vec3& forward,
                     const glm::vec3& up);

    /// @brief Detiene todos los sonidos activos. Llamar desde
    ///        `EditorApplication::rebuildSceneFromMap` antes del
    ///        `registry.clear()` — handles referencian entidades que
    ///        van a dejar de existir.
    void clear();

private:
    AudioDevice&   m_device;
    AssetManager&  m_assets;
};

} // namespace Mood
