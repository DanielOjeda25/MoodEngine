# Plan — Hito 23: AI / pathfinding básico

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 22 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Primer eslabón de "el motor puede sostener un juego". Hasta el Hito 22 había un editor funcional + scripting + render + física + packaging — pero nada que el jugador pueda enfrentar. Este hito agrega:

- **A\* sobre GridMap** como función pura.
- **`NavAgentComponent`** y **`NavSystem`** que mueven entidades hacia un target world-space.
- **Demo enemigo** spawneable desde menú: una entidad que persigue al `FpsCamera` activo en Play Mode (editor o player).
- **F1 debug** dibuja el path activo de cada NavAgent como línea cyan.
- Tests headless del A\* + del integrador de NavAgent.

No-goals: state machines de comportamiento (idle/patrol/attack), behavior trees, navmeshes off-grid, line-of-sight checks, attack damage. Todo eso queda para hitos posteriores. Este hito entrega "una entidad se mueve hacia el jugador respetando muros" — la base mínima sobre la que se construye el resto.

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests A\* (sala vacía, con muros, sin path posible, start==goal, fuera del grid).
- [ ] Tests NavSystem (agente avanza hacia target, llega y se queda quieto, recomputa al moverse el target).
- [ ] Suite total ≥ 182 (sin regresiones).

### Visuales

- [ ] `Ayuda > Agregar enemigo demo` spawnea una entidad en una esquina del mapa con `NavAgentComponent` apuntando al jugador.
- [ ] En Play Mode, el enemigo se mueve hacia el jugador siguiendo el path por tiles libres; no atraviesa muros.
- [ ] F1 (debug draw) dibuja el path activo de cada NavAgent como línea cyan + waypoint actual destacado.
- [ ] Editor + MoodPlayer ejecutan el sistema igual (NavSystem vive en `mood_engine_lib`).

---

## Bloque 1 — A\* sobre GridMap

- [ ] `engine/world/Pathfinding.{h,cpp}`: función pura `findPath(map, start, goal, options) -> vector<TileCoord>`.
  - Heurística Manhattan (L1) o octile (Chebyshev). V1: Manhattan, 4-connected sin diagonales (sin corner-cutting drama).
  - Open set: `std::priority_queue<NodeF>`; closed set: `std::vector<bool>` (tamaño WxH).
  - Retorna path vacío si no hay camino. Retorna `[start]` si start == goal.
  - Tile sólido = obstáculo; tile vacío = walkable.
- [ ] `tests/test_pathfinding.cpp`: 6 casos.
  - Sala 8x8 vacía: path directo.
  - Sala con muro al medio: path bordea.
  - Sin camino (rodeado): path vacío.
  - start == goal: `[start]`.
  - start fuera del grid: vacío.
  - Goal sólido: vacío (no se puede caminar adentro de un muro).

## Bloque 2 — NavAgentComponent + NavSystem

- [ ] `Components.h`: `NavAgentComponent { target, speed, active, path, pathIndex, repathAccumulator }`.
- [ ] `systems/NavSystem.{h,cpp}`:
  - `update(scene, map, mapWorldOrigin, dt)`.
  - Throttle de pathfinding: cada 0.5s o si `||target - lastPathTarget|| > tileSize` recompute.
  - Steering: toward `path[pathIndex]` en world XZ. `transform.position += dir * speed * dt`. Y queda fija (no salta paredes ni vuela).
  - Colisión: usa `moveAndSlide` igual que el player.
  - Avanza `pathIndex` cuando `||pos - waypoint|| < tileSize/2`.
  - Cuando `pathIndex >= path.size()`, agente queda quieto (no recomputa).
- [ ] EditorApplication + PlayerApplication invocan `m_navSystem->update(...)` cada frame entre Animation y Audio.
- [ ] EditorApplication actualiza el `target` de cada NavAgent al `playCamera.position()` en Play Mode (helper `updateNavAgentTargets()` o lambda inline).

## Bloque 3 — Demo enemigo

- [ ] `Ayuda > Agregar enemigo demo`: spawnea una entidad "Enemigo" en una esquina del mapa con:
  - Mesh: Fox.glb por simplicidad (ya está en assets, anima walk).
  - Animator: clip "Walk".
  - NavAgentComponent (target=0,0,0 inicial; el editor lo updatea al jugador).
  - Sin RigidBody dynamic — el NavSystem ya hace moveAndSlide contra el grid; agregar Jolt dynamic causaría doble-handler.
- [ ] `EditorUI::requestSpawnEnemyDemo` + `consumeSpawnEnemyDemoRequest`.
- [ ] `EditorApplication::processSpawnEnemyDemoRequest()` en `DemoSpawners.cpp`.

## Bloque 4 — F1 debug del path

- [ ] En `EditorApplication::drawEditorScene3DOverlay` (o equivalente del player) cuando `m_debugDraw == true`:
  - Iterar entidades con `NavAgentComponent`.
  - Dibujar líneas cyan entre cada par consecutivo de waypoints en world space.
  - Destacar el waypoint actual (`path[pathIndex]`) como cubo cyan brillante.

## Bloque 5 — Integración con MoodPlayer

- [ ] `PlayerApplication::run()` también invoca `m_navSystem->update(...)`.
- [ ] PlayerApplication también actualiza el `target` del NavAgent — el `m_playCamera` es la "presa".
- [ ] Smoke test: empaquetar un proyecto con un enemigo persistido. Doble-click → el enemigo persigue.

## Bloque 6 — Tests + cierre

- [ ] `tests/test_pathfinding.cpp` (6 casos del Bloque 1).
- [ ] `tests/test_nav_system.cpp` (3-4 casos): agente avanza al target, se queda quieto al llegar, recomputa cuando target se mueve, no atraviesa muros.
- [ ] Persistencia opcional del NavAgent en `.moodmap`: V1 NO se persiste (es runtime). El demo se vuelve a spawnear cada sesión. Si se reporta como bug, agregar al schema.
- [ ] Smoke manual: editor + Play Mode + enemigo persigue + cierra. Doble-click en MoodPlayer empaquetado: ídem.
- [ ] Commits atómicos: `feat(world)`, `feat(systems)`, `feat(editor)`, `test(world)`.
- [ ] Tag `v0.23.0-hito23`.
- [ ] Crear `docs/PLAN_HITO24.md`.
- [ ] Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.

---

## Decisiones a tomar

- [ ] **¿4-connected o 8-connected?** V1: 4-connected sin diagonales — simple, sin riesgo de "corner cutting" (atravesar diagonalmente entre dos muros adyacentes). Diagonales se agregan en Hito futuro si se nota que los enemigos hacen movimientos visiblemente boxy.
- [ ] **¿Persistir NavAgent en `.moodmap`?** V1 NO. Es runtime + scope tactical (target cambia cada frame). Si en el futuro el dev quiere "este enemigo arranca patrullando esta ruta", se agrega.
- [ ] **¿Mesh del enemigo demo?** Fox.glb por reuso. En el futuro se puede crear un asset "Enemy.glb" propio o pedir uno CC0 de glTF Sample Assets.

---

## Riesgos identificados

- **Re-path cada 0.5s puede quedar visiblemente atrás del jugador** si éste corre rápido. Si pasa, bajar a 0.2s o re-pathear cuando el target se aleja > 2 tiles. Empezar con 0.5s y medir.
- **moveAndSlide del NavSystem peleando con el player que también lo usa**: cada uno tiene su propio AABB independiente — el moveAndSlide es función pura, no hay state compartido. Sin conflicto.
- **Path vacío en cada frame**: si el agente queda en un tile y el goal es inalcanzable, A\* corre 50fps gastando ciclos. El throttle de 0.5s lo limita pero igual. Mitigación: cache "last failed path" + back-off exponencial. Polish post-V1.
- **Performance**: A\* sobre 8x8 = 64 nodos, trivial. Si en el futuro el GridMap escala a 64x64 (4096 nodos), el priority_queue empieza a notarse. V1 ignora.
