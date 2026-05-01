#pragma once

// TriggerSystem (Hito 33): detecta enter/exit del jugador en cada
// `TriggerComponent` y dispatcha `on_trigger_enter` / `on_trigger_exit`
// al ScriptComponent de la entidad trigger (si tiene uno).
//
// Diseño v1: solo el character controller del jugador es el "actor"
// detectado — para v1 alcanza con kill volumes / checkpoints / gatillos
// area-based. Detectar dynamic bodies (cajas físicas) o entidades NPC
// es trivial extension (loop adicional sobre RigidBodyComponent), pero
// no fue scope de este hito.
//
// Test de overlap: AABB axis-aligned. Centro del trigger =
// TransformComponent.position; size = TriggerComponent.halfExtents * 2.
// Posición del jugador = PhysicsWorld::characterPosition. Si la pos
// está dentro del AABB, el jugador "está dentro" del trigger.
//
// Estado: `playerInside` se guarda en TriggerComponent (runtime, no
// serializado). Cada frame comparamos el flanco false→true (enter) o
// true→false (exit) para decidir el dispatch.

#include "core/Types.h"

namespace Mood {

class PhysicsWorld;
class Scene;
class ScriptSystem;

class TriggerSystem {
public:
    /// @param scene Scene con entidades TriggerComponent.
    /// @param physics Para leer la posicion del player char.
    /// @param scripts Para dispatchar `on_trigger_enter`/`exit`.
    /// @param playerCharId Handle del char controller del jugador.
    ///        Si es 0 (no hay player), no dispatcha nada y mantiene
    ///        `playerInside=false` en cada trigger.
    void update(Scene& scene,
                PhysicsWorld& physics,
                ScriptSystem& scripts,
                u32 playerCharId);
};

} // namespace Mood
