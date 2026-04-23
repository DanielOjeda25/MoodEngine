#pragma once

// Fachada del almacenamiento de entidades. Oculta `entt::registry` al resto
// del motor: el codigo cliente trabaja con `Mood::Entity` y las llamadas
// template-izadas (`createEntity`, `forEach<Components...>`). El registry
// solo se expone al header de Entity.

#include <entt/entt.hpp>

#include <string>
#include <string_view>

namespace Mood {

class Entity;

class Scene {
public:
    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    /// @brief Crea una entidad con `TagComponent{name}` y `TransformComponent`
    ///        con valores por defecto (posicion origen, escala 1).
    Entity createEntity(std::string_view name = {});

    /// @brief Destruye la entidad y todos sus componentes. Si el handle
    ///        no pertenece a esta escena, no-op.
    void destroyEntity(Entity entity);

    /// @brief Itera todas las entidades que tienen TODOS los componentes
    ///        listados. La lambda recibe `(Entity, Component&...)`.
    template<typename... Components, typename Func>
    void forEach(Func&& func) {
        auto view = m_registry.view<Components...>();
        for (auto handle : view) {
            func(makeEntity(handle), view.template get<Components>(handle)...);
        }
    }

    /// @brief Cantidad de entidades vivas. Util para tests y debug.
    std::size_t entityCount() const { return m_registry.storage<entt::entity>()->in_use(); }

    /// @brief Acceso al registry. Expuesto para `Entity`; el resto del motor
    ///        no deberia tocarlo directamente.
    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

private:
    Entity makeEntity(entt::entity handle);

    entt::registry m_registry;
};

} // namespace Mood
