#pragma once

// F4H1.5 extract — helpers compartidos por todos los LuaBindings_*.cpp.
// Pre-F4H1.5 cada archivo `LuaBindings_Health` / `_Ragdoll` / `_Vehicle`
// (y futuros `_Weapon` / `_Enemy` de Fase 4) tenia su propia copia
// inline de `findByTag`. Centralizamos aca para que (a) un cambio de
// convencion sobre tags (ej. tags duplicados con escenarios, fuzzy match,
// indexado por hash) toque 1 archivo en lugar de N; (b) Fase 4 (que va
// a sumar bindings de armas/enemigos/spawners) reuse sin re-implementar.
//
// Header-only inline — el cost de inlinear el walk de entities es nulo
// vs el overhead de un call indirecto.

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <string>

namespace Mood::bindings {

/// @brief Busca la primera entity con el `tag` exacto en `scene`.
///        Convencion "tags unicos" del editor (Hammer-style) — si hay
///        duplicados, gana el primero del orden del registry. Devuelve
///        falsy si no encuentra (entity default-constructed).
inline Entity findEntityByTag(Scene& scene, const std::string& tag) {
    Entity out;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (out) return;
        if (t.name == tag) out = e;
    });
    return out;
}

} // namespace Mood::bindings
