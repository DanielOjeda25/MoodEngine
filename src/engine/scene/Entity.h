#pragma once

// Wrapper liviano sobre `entt::entity` + puntero a `Mood::Scene`. Provee la
// API con la que el editor y el resto del motor tratan entidades — todas
// las llamadas template-izadas se resuelven via `Scene::registry()`.
//
// Size: 8 bytes (id) + 8 bytes (puntero). Copiable/movible trivialmente.

#include "engine/scene/Scene.h"

#include <entt/entt.hpp>

namespace Mood {

class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene)
        : m_handle(handle), m_scene(scene) {}

    /// @brief Agrega un componente (o reemplaza si ya existe mediante
    ///        `replace`). Devuelve referencia al componente recien creado.
    template<typename T, typename... Args>
    T& addComponent(Args&&... args) {
        return m_scene->registry().emplace_or_replace<T>(
            m_handle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& getComponent() {
        return m_scene->registry().get<T>(m_handle);
    }

    template<typename T>
    const T& getComponent() const {
        return m_scene->registry().get<T>(m_handle);
    }

    template<typename T>
    bool hasComponent() const {
        return m_scene->registry().all_of<T>(m_handle);
    }

    template<typename T>
    void removeComponent() {
        m_scene->registry().remove<T>(m_handle);
    }

    /// @brief Handle raw de EnTT. Evitar filtrarlo fuera del engine/scene/.
    entt::entity handle() const { return m_handle; }

    /// @brief Valida que la entidad apunte a un handle + scene no-nulos.
    ///        Las entidades default-constructed son falsy.
    explicit operator bool() const {
        return m_handle != entt::null && m_scene != nullptr;
    }

    bool operator==(const Entity& other) const {
        return m_handle == other.m_handle && m_scene == other.m_scene;
    }
    bool operator!=(const Entity& other) const { return !(*this == other); }

private:
    entt::entity m_handle{entt::null};
    Scene* m_scene{nullptr};
};

} // namespace Mood
