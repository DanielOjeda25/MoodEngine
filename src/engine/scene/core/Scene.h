#pragma once

// Fachada del almacenamiento de entidades. Oculta `entt::registry` al resto
// del motor: el codigo cliente trabaja con `Mood::Entity` y las llamadas
// template-izadas (`createEntity`, `forEach<Components...>`). El registry
// solo se expone al header de Entity.

#include "core/Types.h"
#include "engine/scene/VisGroup.h"  // F2H33

#include <entt/entt.hpp>

#include <string>
#include <string_view>
#include <vector>

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

    // --- F2H33: VisGroups ---
    // Storage plano de grupos (membership 1-a-N por entity via
    // VisGroupMembershipComponent). El editor muta via comandos undoable;
    // SceneRenderer y ScenePick consultan `findVisGroup(id)->hidden` antes
    // de procesar cada entity.

    const std::vector<VisGroup>& visgroups() const { return m_visgroups; }
    std::vector<VisGroup>& visgroupsMutable() { return m_visgroups; }

    /// @brief Busca un grupo por id. Devuelve nullptr si no existe.
    VisGroup* findVisGroup(u64 id);
    const VisGroup* findVisGroup(u64 id) const;

    /// @brief Crea un VisGroup nuevo con id auto-asignado (siguiente libre
    ///        empezando en 1). Devuelve referencia al recien creado.
    VisGroup& addVisGroup(std::string name, glm::vec3 color);

    /// @brief Inserta un VisGroup con id explicito (usado por el loader y
    ///        por undo de DeleteVisGroupCommand). Si ya existe uno con ese
    ///        id, lo reemplaza.
    VisGroup& insertVisGroup(VisGroup vg);

    /// @brief Elimina el grupo del registry. NO toca las entities miembros
    ///        — el comando de delete se ocupa de removerles el componente
    ///        de membership. No-op si no existe.
    void removeVisGroup(u64 id);

    /// @brief Reemplaza completo el array (loader, reset). Limpia el
    ///        existente.
    void resetVisGroups(std::vector<VisGroup> vgs) { m_visgroups = std::move(vgs); }

    /// @brief F2H65: lookup de Entity por raw entt handle. Devuelve una
    ///        Entity falsy si el handle no esta vivo en el registry.
    ///        Util para JointComponent.targetEntity → Entity B.
    Entity entityFromHandle(entt::entity handle);

private:
    Entity makeEntity(entt::entity handle);

    entt::registry m_registry;
    std::vector<VisGroup> m_visgroups;  // F2H33
};

} // namespace Mood
