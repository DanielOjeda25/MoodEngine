#pragma once

// F2H33: VisGroups (grupos planos nombrados de entidades) inspirados en
// Valve Hammer Editor. Cada grupo tiene id estable + nombre + color + flag
// hidden. Una entidad pertenece a 0 o 1 grupo (membership 1-a-N — Hammer 4
// es asi). Persistido en `.moodmap` v14 como array `visgroups` top-level
// + campo opcional `visgroupId` por entity / brush.
//
// Hide flag: SceneRenderer_Render skipea entities/brushes cuyo grupo esta
// hidden (no se encolan al frame). ScenePick tambien las skipea (no
// pickables). El componente sigue presente en la entidad — solo el render
// + picking observa el flag. Scripts y physics siguen activos en entidades
// ocultas (alineado con Hammer: VisGroup no es "disable", es "ocultar
// en viewport").
//
// API minima: el storage vive en `Scene` via `Scene::visgroups()`. Los
// comandos undoable mutan via los metodos directos de la coleccion.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Mood {

class Entity;
class Scene;

/// @brief Componente opcional que indica a que VisGroup pertenece la
///        entidad. `groupId == 0` (sentinel "sin grupo") = no debe
///        existir el componente; preferimos quitarlo a setearlo en 0.
struct VisGroupMembershipComponent {
    u64 groupId = 0;

    VisGroupMembershipComponent() = default;
    explicit VisGroupMembershipComponent(u64 g) : groupId(g) {}
};

/// @brief Definicion de un grupo. ID estable a lo largo del proyecto
///        (se persiste). Nombre + color libres; el dev los edita desde
///        el VisGroupsPanel.
struct VisGroup {
    u64 id = 0;                              // 0 reservado = "sin grupo"
    std::string name;
    glm::vec3 color{0.7f, 0.7f, 0.7f};       // RGB en [0,1]; default gris
    bool hidden = false;
};

/// @brief Devuelve el groupId de la entidad (0 si no pertenece a ningun
///        grupo). Helper de lectura — no toca el ECS si el componente
///        no existe.
u64 visgroupIdOf(Entity entity);

/// @brief True si la entidad pertenece a un VisGroup que esta hidden.
///        Si la entidad no tiene componente o el id no matchea ningun
///        grupo, devuelve false.
bool isEntityHiddenByVisGroup(Scene& scene, Entity entity);

} // namespace Mood
