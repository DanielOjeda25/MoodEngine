# Plan — Hito 33: Raycasts + Triggers expuestos a Lua (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 33 cerrado).

---

## Estado

**Hito 33 cerrado** (`v0.33.0-hito33`, suite **281/5535**). Scope acordado con el dev: candidato A del plan original — habilitar gameplay scriptable con armas + zonas detectoras. Combo natural post Hito 30 (char controller) + 31 (polish).

## Bloques cerrados

- [x] **Bloque 1 — `PhysicsWorld::raycast`**: nuevo `RaycastHit { hit, point, normal, distance, bodyId }` + método `raycast(origin, direction, maxDistance)`. Implementado via `JPH::NarrowPhaseQuery::CastRay` + `BodyLockRead` para extraer la normal en el punto de impacto. Direction no necesita estar normalizada (se escala internamente). Characters virtuales son "ghost" para queries (consistente con Hito 30).
- [x] **Bloque 2 — Lua bindings + `ScriptSystem::dispatchEvent`**: `physics.raycast(origin_table, dir_table, maxDist)` registrado en LuaBindings devolviendo tabla `{hit, point, normal, distance, bodyId}`. Origin/direction como tablas Lua 1-indexadas (convención Hito 24). `ScriptSystem::update` propaga `PhysicsWorld*` opcional → tests headless siguen pasando con `nullptr`. Nuevo `dispatchEvent(entity, eventName)` busca en `m_states[entity]` y llama la función global del sol::state via `sol::protected_function` (no-op silencioso si el script no la define).
- [x] **Bloque 3 — `TriggerComponent` + `TriggerSystem`**: nuevo componente `{halfExtents, playerInside}` (último flag runtime, no serializado). `TriggerSystem` stateless, AABB-testea cada frame la posición del char del jugador contra cada trigger. En flank-changes false→true / true→false dispatcha `on_trigger_enter` / `on_trigger_exit` al script de la entidad. Wireado en `EditorApplication` (solo Play Mode) + `PlayerApplication` (siempre).
- [x] **Bloque 4 — Persistencia + Inspector + spawner demo**: `SavedTrigger` opcional en `SavedEntity` (sin bump de schema mayor — campo nuevo opcional). `EntitySerializer` y `SceneLoader` cubren round-trip. Inspector UI: sección "Trigger" con `DragFloat3 halfExtents` undoable via `pushEditIfDone<glm::vec3>` (mismo patrón de Hito 32) + readout de `playerInside` runtime. Menú "Ayuda > Agregar trigger demo" spawnea entidad en (0, 1, 0) con `halfExtents=(1,1,1)` + script `assets/scripts/trigger_demo.lua` que loguea enter/exit.
- [x] **Bloque 5 — Tests + cierre**: 10 tests nuevos (5 en `test_raycast.cpp`, 5 en `test_trigger_system.cpp`). Cubren: hit/miss/maxDistance/dirección no-normalizada/primer body en path para raycast; flank true/flank false/charId=0/dispatch via side-effect en `GameState::hud()`/no-double-dispatch para triggers. Tag `v0.33.0-hito33`.

## Lo que NO se cubrió

- **Detectar dynamic bodies en triggers**: hoy solo el char del jugador es el "actor" detectable. Cajas físicas o NPCs entrando/saliendo del trigger no disparan callback. Patrón a extender: loop adicional sobre entidades con `RigidBodyComponent`. Documentado.
- **Raycast filtrable por layer/tag**: hoy raycast pega contra cualquier body (Static + Dynamic + Kinematic). Sin filtro por layer (ej. "ignore self") ni por componente (ej. "solo enemies"). Si aparece en un demo, agregar parámetro opcional al método.
- **Eventos extra del trigger**: `on_trigger_stay` (cada frame que el actor está dentro) no se implementó. Solo enter/exit. Si un script necesita "tick mientras dentro", puede consultar `playerInside` cada frame en su `onUpdate`.
- **42 widgets restantes del Inspector**: igual que en Hito 32, el wire-up del resto de widgets sigue siendo mecánico y pendiente.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-01):
- Raycast via `JPH::NarrowPhaseQuery::CastRay` + `BodyLockRead` para la normal (no `JPH::ShapeCast`).
- `TriggerSystem` stateless con flank-detection sobre `TriggerComponent::playerInside` (no event queue).
- `ScriptSystem::dispatchEvent` via `sol::protected_function` con miss silencioso.

---

## Riesgos identificados — resolución

- **Raycast performance en escenas grandes**: Jolt cobra O(log N) por broad phase + narrow phase test. Documentado en el header del método: mantener `maxDistance` acotado. No es problema con los demos del repo.
- **Trigger AABB axis-aligned no respeta rotation**: aceptado. Si aparece un caso (ej. trigger rotado para una puerta), bumpea a OBB o composite shape. Documentado en el header de `TriggerSystem`.
- **Schema bump del `.moodmap`**: evitado. Campo `trigger` es opcional, archivos viejos lo interpretan como ausente (defaults vacíos), nuevos lo escriben sólo cuando la entidad tiene el componente.

---

## Verificación visual del dev (criterios cumplidos)

- "Ayuda > Agregar trigger demo" spawnea entidad en (0, 1, 0) con halfExtents 1×1×1 (caja imaginaria 2×2×2m).
- Inspector muestra sección "Trigger" con DragFloat3 + undo via Ctrl+Z + readout `playerInside`.
- En Play Mode, caminar al centro del trigger imprime `[script] [info] [trigger] player entro` en la consola; al salir, `[trigger] player salio`.
- Save/cerrar/reabrir el proyecto preserva `halfExtents` editado.
- Suite 281/5535 sin regresiones.
